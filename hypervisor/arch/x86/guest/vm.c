/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <per_cpu.h>
#include <lapic.h>
#include <vm.h>
#include <vm_reset.h>
#include <bits.h>
#include <e820.h>
#include <multiboot.h>
#include <vtd.h>
#include <reloc.h>
#include <ept.h>
#include <guest_pm.h>
#include <console.h>
#include <ptdev.h>
#include <vmcs.h>
#include <pgtable.h>
#include <mmu.h>
#include <logmsg.h>
#include <vboot_info.h>
#include <vboot.h>
#include <board.h>
#include <sgx.h>
#include <sbuf.h>
#include <pci_dev.h>
#include <vacpi.h>

vm_sw_loader_t vm_sw_loader;

/* Local variables */

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
}

/**
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmap is to be initialized
 */
static void setup_io_bitmap(struct acrn_vm *vm)
{
	/* block all IO port access from Guest */
	(void)memset(vm->arch_vm.io_bitmap, 0xFFU, PAGE_SIZE * 2U);
}

/**
 * return a pointer to the virtual machine structure associated with
 * this VM ID
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	return &vm_array[vm_id];
}

/**
 * @pre vm_config != NULL
 */
static inline uint16_t get_vm_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	uint16_t cpu_id = INVALID_CPU_ID;

	cpu_id = ffs64(vm_config->vcpu_affinity[0]);

	return (cpu_id < get_pcpu_nums()) ? cpu_id : INVALID_CPU_ID;
}

/**
 * @pre vm != NULL && vm_config != NULL
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t base_hpa = vm_config->memory.start_hpa;
	uint32_t i;

	for (i = 0U; i < vm->e820_entry_num; i++) {
		const struct e820_entry *entry = &(vm->e820_entries[i]);

		if (entry->length == 0UL) {
			break;
		}

		/* Do EPT mapping for GPAs that are backed by physical memory */
		if (entry->type == E820_TYPE_RAM) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr, entry->length,
				EPT_RWX | EPT_WB);

			base_hpa += entry->length;
		}

		/* GPAs under 1MB are always backed by physical memory */
		if ((entry->type != E820_TYPE_RAM) && (entry->baseaddr < (uint64_t)MEM_1M)) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr, entry->length,
				EPT_RWX | EPT_UNCACHED);

			base_hpa += entry->length;
		}
	}
}

/* Add EPT mapping of EPC reource for the VM */
static void prepare_epc_vm_memmap(struct acrn_vm *vm)
{
}

/**
 * @brief get bitmap of pCPUs whose vCPUs have LAPIC PT enabled
 *
 * @param[in] vm pointer to vm data structure
 * @pre vm != NULL
 *
 * @return pCPU bitmap
 */
static uint64_t lapic_pt_enabled_pcpu_bitmap(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint64_t bitmap = 0UL;

	foreach_vcpu (i, vm, vcpu) {
		bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &bitmap);
	}

	return bitmap;
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 * @pre vm->state == VM_POWERED_OFF
 */
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm = NULL;
	int32_t status = 0;
	bool need_cleanup = false;
	uint32_t i;
	uint16_t pcpu_id;

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	(void)memset((void *)vm, 0U, sizeof(struct acrn_vm));
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;

	init_ept_mem_ops(&vm->arch_vm.ept_mem_ops, vm->vm_id);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp, &vm->arch_vm.ept_mem_ops);

	(void)memcpy_s(&vm->uuid[0], sizeof(vm->uuid), &vm_config->uuid[0], sizeof(vm_config->uuid));
	/* For PRE_LAUNCHED_VM and POST_LAUNCHED_VM */
	if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
		vm->sworld_control.flag.supported = 1U;
	}

	create_prelaunched_vm_e820(vm);
	prepare_prelaunched_vm_memmap(vm, vm_config);
	status = init_vm_boot_info(vm);

	if (status == 0) {
		prepare_epc_vm_memmap(vm);

		spinlock_init(&vm->vm_lock);
		spinlock_init(&vm->emul_mmio_lock);

		vm->arch_vm.vlapic_state = VM_VLAPIC_X2APIC;
		vm->intr_inject_delay_delta = 0UL;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		/* Create virtual uart;*/
		init_vuart(vm, vm_config->vuart);

		vrtc_init(vm);

		vpci_init(vm);
		enable_iommu();

		/* vpic wire_mode default is INTR */
		vm->wire_mode = VPIC_WIRE_INTR;

		/* Populate return VM handle */
		*rtn_vm = vm;
		vm->sw.io_shared_page = NULL;
		status = set_vcpuid_entries(vm);
		if (status == 0) {
			vm->state = VM_CREATED;
		} else {
			need_cleanup = true;
		}
	}

	if (need_cleanup) {
		if (vm->arch_vm.nworld_eptp != NULL) {
			(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
		}
	}

	if (status == 0) {
		/* We have assumptions:
		 *   1) vcpus used by SOS has been offlined by DM before UOS re-use it.
		 *   2) vcpu_affinity[] passed sanitization is OK for vcpu creating.
		 */
		for (i = 0U; i < vm_config->vcpu_num; i++) {
			pcpu_id = ffs64(vm_config->vcpu_affinity[i]);
			status = prepare_vcpu(vm, pcpu_id);
			if (status != 0) {
				break;
			}
		}
	}

	return status;
}

/*
 * @pre vm != NULL
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	uint16_t this_pcpu_id;
	uint64_t mask;
	struct acrn_vcpu *vcpu = NULL;
	struct acrn_vm_config *vm_config = NULL;
	int32_t ret = 0;

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state == VM_PAUSED) {
		vm->state = VM_POWERED_OFF;
		this_pcpu_id = get_pcpu_id();
		mask = lapic_pt_enabled_pcpu_bitmap(vm);

		/*
		 * If the current pcpu needs to offline itself,
		 * it will be done after shutdown_vm() completes
		 * in the idle thread.
		 */
		if (bitmap_test(this_pcpu_id, &mask)) {
			bitmap_clear_nolock(this_pcpu_id, &mask);
			make_pcpu_offline(this_pcpu_id);
		}

		foreach_vcpu (i, vm, vcpu) {
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);

			if (bitmap_test(pcpuid_from_vcpu(vcpu), &mask)) {
				make_pcpu_offline(pcpuid_from_vcpu(vcpu));
			}
		}

		wait_pcpus_offline(mask);

		if ((mask != 0UL) && (!start_pcpus(mask))) {
			pr_fatal("Failed to start all cpus in mask(0x%lx)", mask);
			ret = -ETIMEDOUT;
		}

		vm_config = get_vm_config(vm->vm_id);
		vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;

		vpci_cleanup(vm);

		deinit_vuart(vm);

		ptdev_release_all_entries(vm);

		/* Free iommu */
		destroy_iommu_domain(vm->iommu);

		/* Free EPT allocated resources assigned to VM */
		destroy_ept(vm);
	} else {
		ret = -EINVAL;
	}

	/* Return status to caller */
	return ret;
}

/**
 *  * @pre vm != NULL
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *bsp = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	bsp = vcpu_from_vid(vm, BOOT_CPU_ID);
	vcpu_make_request(bsp, ACRN_REQUEST_INIT_VMCS);
	launch_vcpu(bsp);
}

/**
 *  * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (vm->state != VM_PAUSED) {
		if (is_rt_vm(vm)) {
			/**
			 * For RTVM, we can only pause its vCPUs when it stays at following states:
			 *  - It is powering off by itself
			 *  - It is created but doesn't start
			 */
			if ((vm->state == VM_POWERING_OFF) || (vm->state == VM_CREATED)) {
				foreach_vcpu (i, vm, vcpu) {
					pause_vcpu(vcpu, VCPU_ZOMBIE);
				}

				vm->state = VM_PAUSED;
			}
		} else {
			foreach_vcpu (i, vm, vcpu) {
				pause_vcpu(vcpu, VCPU_ZOMBIE);
			}

			vm->state = VM_PAUSED;
		}
	}
}

/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = 0;
	struct acrn_vm *vm = NULL;

	err = create_vm(vm_id, vm_config, &vm);

	if (err == 0) {
		build_vacpi(vm);

		(void)vm_sw_loader(vm);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
	}
}

/**
 * @pre vm_config != NULL
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id, bsp_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		bsp_id = get_vm_bsp_pcpu_id(vm_config);
		if (pcpu_id == bsp_id) {
			prepare_vm(vm_id, vm_config);
		}
	}
}

/*
 * @brief Check state of vLAPICs of a VM
 *
 * @pre vm != NULL
 */
enum vm_vlapic_state check_vm_vlapic_state(const struct acrn_vm *vm)
{
	enum vm_vlapic_state vlapic_state;

	vlapic_state = vm->arch_vm.vlapic_state;
	return vlapic_state;
}

/**
 * if there is RT VM return true otherwise return false.
 */
bool has_rt_vm(void)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (is_rt_vm(get_vm_from_vmid(vm_id))) {
			break;
		}
	}

	return ((vm_id == CONFIG_MAX_VM_NUM) ? false : true);
}

void make_shutdown_vm_request(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

bool need_shutdown_vm(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
}
