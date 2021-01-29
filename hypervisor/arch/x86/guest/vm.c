/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
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
#include <console.h>
#include <ptdev.h>
#include <vmcs.h>
#include <pgtable.h>
#include <mmu.h>
#include <logmsg.h>
#include <vuart.h>
#include <vboot_info.h>
#include <board.h>
#include <vacpi.h>
#include <virq.h>

/**
 * @defgroup vp-base_vm vp-base.vm
 * @ingroup vp-base
 * @brief Implementation of all external APIs to VM operations.
 *
 * This module implements the VM operations executed from other modules. It offers APIs to launch
 * all guest VMs and to shutdown or pause one specific VM. The main part is focused on implementation of how
 * to create and boot a VM.
 *
 * Usage:
 * - 'init' module depends on this module to launch all VMs.
 * - 'vp-base.vm_reset' module depends on this module to send a request and shutdown or pause a VM.
 * - 'vp-dm.vperipheral' module depends on this module to shutdown one VM when reading vRTC failed.
 * - 'vp-base.hv_main' module depends on this module to check if a VM should be shutdown.
 * - 'vp-base.vcpuid' module depends on this module to check if a VM is "safety VM".
 * - 'vp-base.virq' module depends on this module to check if a VM is "safety VM".
 * - 'vp-base.vmsr' module depends on this module to check if a VM is "safety VM".
 *
 * Dependency:
 * - This module depends on 'hwmgmt.apic' module to send IPI to other physical CPU.
 * - This module depends on 'hwmgmt.cpu' module to start/offline the CPU and get per-cpu data and physical CPU
 *   ID/numbers.
 * - This module depends on 'hwmgmt.iommu' module to release IOMMU.
 * - This module depends on 'hwmgmt.mmu' module to sanitize each PTE of EPT PML4 items.
 * - This module depends on 'hwmgmt.page' module to set EPT memory operations callback functions, which are called from
 *        this module to setup EPT memory mapping for the VMs and to do other EPT operations.
 * - This module depends on 'hwmgmt.vboot' module to initialize guest VM boot info, including kernel and boot arguments.
 * - This module depends on 'lib.bits' module to set/get/clear bit in specific value.
 * - This module depends on 'lib.util' module to do memory set.
 * - This module depends on 'vp-base.vm-config' module to get the VM configuration information.
 * - This module depends on 'vp-base.vcpu' module to prepare/launch/reset/offline the vCPU, and to access the
 *        states of the specific vCPU.
 * - This module depends on 'vp-base.vcpuid' module to setup vCPUID entries for the VM.
 * - This module depends on 'vp-base.virq' module to make a request to the specific vCPU.
 * - This module depends on 'vp-dm.vperipheral' module to initialize vRTC and vPCI devices.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the external APIs that shall be provided by the vp-base.vm module.
 *
 * This file implements all external functions that shall be provided by the vp-base.vm module.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: setup_io_bitmap, get_vm_bsp_pcpu_id, prepare_prelaunched_vm_memmap,
 * get_pcpu_bitmap.
 *
 * Decomposed functions include: create_vm, start_vm and prepare_vm.
 */


/**
 * @brief Global array of "struct acrn_vm" elements storing info of all VMs.
 *
 * This global variable is filled when launch_vms is called; it is PAGE size aligned.
 * It includes all the VM related elements, like vCPU, IOMMU, e820 entries, vPCI and vRTC.
 * It's also used by other modules through calling get_vm_from_vmid.
 */
static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

/**
 * @brief Check whether the given VM is the safety VM.
 *
 * This function is called to check the given VM's ID(vm_id). A VM is the safety VM if and only if the vm_id is 0.
 *
 * @param[in] vm Pointer to a VM which contains the VM ID to be checked.
 *
 * @return A boolean value indicating whether the given VM is the safety VM.
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark It should be called after the launch_vms is called
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
bool is_safety_vm(const struct acrn_vm *vm)
{
	/** Return true if vm->vm_id is 0, others return false. */
	return (vm->vm_id == 0U);
}

/**
 * @brief Initialize the IO bitmap of the given VM.
 *
 * This function is called to initialize the given VM's IO bitmap to block all IO port access from guest OS. The total
 * size of one VM's IO bitmap is 8K * 8 bits. And its host base address will be copied to the field
 * VMX_IO_BITMAP_A_FULL of this VM's VMCS domain.
 *
 * @param[inout] vm Pointer to the VM whose I/O bitmap is to be initialized.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by create_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation
 */
static void setup_io_bitmap(struct acrn_vm *vm)
{
	/** Call memset with the following parameters, in order to set each bit of IO bitmap as 1, and discard
	 *  its return value.
	 *  - vm->arch_vm.io_bitmap
	 *  - 0xFF
	 *  - PAGE_SIZE * 2
	 */
	(void)memset(vm->arch_vm.io_bitmap, 0xFFU, PAGE_SIZE * 2U);
}

/**
 * @brief Get a pointer to the VM structure associated with the given VM ID.
 *
 * This function is called to get a pointer to the "struct acrn_vm" with the given VM ID.
 *
 * @param[in] vm_id The ID of the VM which will be returned.
 *
 * @return The pointer to the "struct acrn_vm" with the given vm_id
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 *
 * @post ret != NULL, where 'ret' is a special identifier referring to the return value of this function.
 * @post ret->vm_id == vm_id, where 'ret' is a special identifier referring to the return value of this function.
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark It can be called only after launch_vms has been called once.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	/** Return &vm_array[vm_id]; vm_array is the array storing each VM data structure */
	return &vm_array[vm_id];
}

/**
 * @brief Get the first physical CPU ID of one VM from its configuration data.
 *
 * There is a vCPU affinity field in each VM configuration domain (struct acrn_vm_config). The vCPU affinity field
 * is a bitmap indicating the physical CPUs the vCPU can run on. The first physical CPU is named as BP.
 * This function is called to get its ID.
 *
 * @param[in] vm_config The pointer to the VM configuration data which includes the vCPU affinity info.
 *
 * @return The physical BP ID
 *
 * @retval "a valid physical CPU ID" if the configured ID in vCPU affinity is less than total physical CPU number.
 * @retval "an invalid ID(0xFFFF)" if the configured ID is not less than total physical CPU number.
 *
 * @pre vm_config != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by launch_vms.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint16_t get_vm_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	/** Declare the following local variables of type uint16_t.
	 *  - cpu_id representing the physical CPU ID to be returned, initialized as FFFFH.
	 */
	uint16_t cpu_id = INVALID_CPU_ID;

	/** Set cpu_id to the value returned by ffs64(vm_config->vcpu_affinity[0]). */
	cpu_id = ffs64(vm_config->vcpu_affinity[0]);

	/** Return cpu_id, which should be less than the total physical CPU number; if not return FFFFH. */
	return (cpu_id < get_pcpu_nums()) ? cpu_id : INVALID_CPU_ID;
}

/**
 * @brief Setup EPT memory mapping for the given VM according to its e820 table.
 *
 * This function is called to setup the EPT mapping for the given VM according to its e820 table configuration.
 * Before the guest VM boots up, the hypervisor need setup the EPT mapping between the guest VM's memory space
 * and host memory space. Also it is called GPA --> HPA (Guest Physical Address --> Host Physical Address).
 * HPA space information is from the given VM's configuration data included in 'vm_config'.
 *
 * @param[inout] vm Pointer to a VM whose EPT mapping will be set up.
 * @param[in] vm_config The pointer to the VM configuration data.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vm_config != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by create_vm; and e820 table should be configured first.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	/** Declare the following local variables of type uint64_t.
	 *  - base_hpa representing the start address of one VM's host physical memory space, initialized as
	 *  'vm_config->memory.start_hpa'.
	 */
	uint64_t base_hpa = vm_config->memory.start_hpa;
	/** Declare the following local variables of type uint64_t.
	 *  - remaining_hpa_size representing the left size (not mapped) of the host physical memory space,
	 *  initialized as vm_config->memory.size.
	 */
	uint64_t remaining_hpa_size = vm_config->memory.size;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing a loop counter used as index of the e820 entries, not initialized.
	 */
	uint32_t i;

	/** For each i ranging from 0 to vm->e820_entry_num -1 [with a step of 1] */
	for (i = 0U; i < vm->e820_entry_num; i++) {
		/** Declare the following local variables of type 'struct e820_entry *'.
		 *  - entry representing the pointer to one entry of the e820, initialized as &(vm->e820_entries[i]).
		 */
		const struct e820_entry *entry = &(vm->e820_entries[i]);

		/** If 'entry->length' equals 0 */
		if (entry->length == 0UL) {
			/** Terminate the loop */
			break;
		}

		/** If entry->type is E820_TYPE_RAM */
		if (entry->type == E820_TYPE_RAM) {
			/** If remaining_hpa_size is not less than entry->length */
			if (remaining_hpa_size >= entry->length) {
				/** Call ept_add_mr with the following parameters, in order to setup EPT mapping
				 *  between GPA and HPA.
				 *  - vm
				 *  - vm->arch_vm.nworld_eptp
				 *  - base_hpa
				 *  - entry->baseaddr
				 *  - entry->length
				 *  - EPT_RWX | EPT_WB
				 */
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
					entry->length, EPT_RWX | EPT_WB);

				/** Increment base_hpa by entry->length as next HPA to map */
				base_hpa += entry->length;
				/** Decrement remaining_hpa_size by entry->length as the host physical memory size
				 *  left */
				remaining_hpa_size -= entry->length;
			} else {
				/** Logging the following information with a log level of LOG_WARNING.
				 *  - current function name
				 *  - warning message
				 */
				pr_warn("%s: HPA size incorrectly configured in v820\n", __func__);
			}
		}

		/** If entry->type is not E820_TYPE_RAM and remaining_hpa_size less than entry->length and
		 *  entry->baseaddr less than MEM_1M
		 */
		if ((entry->type != E820_TYPE_RAM) && (entry->baseaddr < (uint64_t)MEM_1M) &&
			(remaining_hpa_size >= entry->length)) {
			/** Call ept_add_mr with the following parameters, in order setup do EPT mapping from GPA
			 *  (first 1MB) to HPA, set the property as EPT_UNCACHED, for guest OS could use first 1MB
			 *  space for some specific usage.
			 *  - vm
			 *  - vm->arch_vm.nworld_eptp
			 *  - base_hpa
			 *  - entry->baseaddr
			 *  - entry->length
			 *  - EPT_RWX | EPT_UNCACHED
			 */
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr, entry->length,
				EPT_RWX | EPT_UNCACHED);

			/** Increment base_hpa by entry->length as next HPA to map */
			base_hpa += entry->length;
			/** Decrement remaining_hpa_size by entry->length as left host phyiscal memory size*/
			remaining_hpa_size -= entry->length;
		}
	}
}

/**
 * @brief Get ID bitmap of all the physical CPUs which actually run the given VM
 *
 * This function is called to get all the physical CPUs' ID from the given VM's vCPU, and save the IDs to a bitmap.
 * Each vCPU is mapped to one specific physical CPU.
 *
 * @param[in] vm Pointer to a VM whose vCPUs corresponding physical CPU ID will be reported.
 *
 * @return A bitmap of the physical CPU IDs.
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal function called by shutdown_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation
 */
static uint64_t get_pcpu_bitmap(struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a loop counter used as index of vm->hw.vcpu_array, not initialized.
	 */
	uint16_t i;
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to each element of the vm->hw.vcpu_array, not initialized.
	 */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variables of type uint64_t.
	 *  - bitmap representing all the VM's physical CPUs' ID, initialized as 0.
	 */
	uint64_t bitmap = 0UL;

	/** For each vcpu in the online vCPUs of the given VM, using i as the loop counter */
	foreach_vcpu(i, vm, vcpu) {
		/** Call bitmap_set_nolock with the following parameters, in order to set the bit of the
		 *  variable 'bitmap' corresponding to the vCPU's physical CPU ID.
		 *  - pcpuid_from_vcpu(vcpu)
		 *  - &bitmap
		 */
		bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &bitmap);
	}

	/** Return bitmap: it includes all the physical CPU IDs got from given VM's vCPU */
	return bitmap;
}

/**
 * @brief Setup a VM entity according to the given VM ID and configuration data.
 *
 * This function is called to setup a VM entity. It gets a VM instance from vm_array with vm_id as index, and then
 * builds the VM instance by calling internal or external functions. For example, setup the EPT mapping, initialize
 * the guest OS boot information( kernel loading address and boot arguments), setup IO bitmap, initialize virtual
 * devices (vRTC, vPCI), setup vCPUID entiries and setup vCPUs.
 *
 * @param[in] vm_id The index of vm_array, also it will be treated as the ID of the VM instance.
 * @param[in] vm_config The pointer to the VM configuration data.
 * @param[out] rtn_vm A pointer's pointer used to return the created VM instance.
 *
 * @return A status value to indicate whether the VM created successfully.
 *
 * @retval 0 if the VM is created successfully.
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @pre vm_config != NULL
 * @pre rtn_vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by prepare_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm_id and \a rtn_vm are different among parallel invocation
 */
static int32_t create_vm(uint16_t vm_id, const struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing a pointer to a VM instance from vm_array, initialized as NULL.
	 */
	struct acrn_vm *vm = NULL;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing a loop counter used as index of vm_config->vcpu_affinity, not initialized.
	 */
	uint32_t i;
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing the physical CPU ID associated with one vCPU, not initialized.
	 */
	uint16_t pcpu_id;
	/** Declare the following local variables of type bool.
	 *  - enforce_4k_ipage representing a flag to indicate EPT page is 4K or not, initialized as false.
	 */
	bool enforce_4k_ipage = false;

	/** Set vm to &vm_array[vm_id] */
	vm = &vm_array[vm_id];
	/** Call memset with the following parameters, in order to clear all bits of the VM instance,
	 *  and discard its return value.
	 *  - vm
	 *  - 0
	 *  - sizeof(struct acrn_vm)
	 */
	(void)memset((void *)vm, 0U, sizeof(struct acrn_vm));
	/** Set vm->vm_id to vm_id */
	vm->vm_id = vm_id;
	/** Set vm->hw.created_vcpus to 0 */
	vm->hw.created_vcpus = 0U;

	/** If the VM to create is safety VM, whose EPT page size set to 4KB. */
	if (is_safety_vm(vm)) {
		/** Set enforce_4k_ipage to true */
		enforce_4k_ipage = true;
	}
	/** Call init_ept_mem_ops with the following parameters, in order to initialize EPT operating functions.
	 *  - &vm->arch_vm.ept_mem_ops
	 *  - vm->vm_id
	 *  - enforce_4k_ipage
	 */
	init_ept_mem_ops(&vm->arch_vm.ept_mem_ops, vm->vm_id, enforce_4k_ipage);
	/** Set vm->arch_vm.nworld_eptp to the value of the PML4 base address returned by
	 *  vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info)
	 */
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	/** Call sanitize_pte with the following parameters, in order to initialize the entries of the EPT's PML4.
	 *  - vm->arch_vm.nworld_eptp
	 *  - &vm->arch_vm.ept_mem_ops
	 */
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp, &vm->arch_vm.ept_mem_ops);

	/** Call create_prelaunched_vm_e820 with the following parameters, in order to initialize the e820 table.
	 *  - vm
	 */
	create_prelaunched_vm_e820(vm);
	/** Call prepare_prelaunched_vm_memmap with the following parameters, in order to setup the EPT mapping between
	 *  each entry of the VM's e820 table and the host physical memory, which range is configured in 'vm_config'.
	 *  - vm
	 *  - vm_config
	 */
	prepare_prelaunched_vm_memmap(vm, vm_config);
	/** Call init_vm_boot_info with the following parameters, in order to get the guest OS kernel image/boot
	 *  arguments information and give their values to the VM's related fields.
	 *  - vm
	 */
	init_vm_boot_info(vm);

	/** Call spinlock_init with the following parameter, in order to initialize the spinlock for protecting EPT
	 *  manipulations.
	 *  - &vm->ept_lock
	 */
	spinlock_init(&vm->ept_lock);

	/** Call spinlock_init with the following parameter, in order to initialize the spinlock for protecting VM state
	 *  transitions.
	 *  - &vm->vm_lock
	 */
	spinlock_init(&vm->vm_lock);

	/** Call setup_io_bitmap with the following parameters, in order to setup IO bit-mask so the VM-exit occurs
	 *  on selected IO ranges.
	 *  - vm
	 */
	setup_io_bitmap(vm);

	/** Call init_vuart with the following parameters, in order to create a virtual uart so the guest OS can use it
	 *  to output log info.
	 *  - vm
	 *  - vm_config->vuart
	 */
	init_vuart(vm, vm_config->vuart);

	/** Call vrtc_init with the following parameters, in order to initialize the vRTC of the VM.
	 *  - vm
	 */
	vrtc_init(vm);

	/** Call vpci_init with the following parameters, in order to initialize vPCI devices of the given VM.
	 *  - vm
	 */
	vpci_init(vm);

	/** Set '*rtn_vm' to 'vm', in order to return the pointer to the created VM structure to the caller */
	*rtn_vm = vm;
	/** Call set_vcpuid_entries with the following parameters, in order to setup vCPUID entries for this VM.
	 *  - vm
	 */
	set_vcpuid_entries(vm);

	/** Set vm->state to VM_CREATED */
	vm->state = VM_CREATED;

	/** For each i ranging from 0 to 'vm_config->vcpu_num - 1' [with a step of 1] */
	for (i = 0U; i < vm_config->vcpu_num; i++) {
		/** Set pcpu_id to the value of 'ffs64(vm_config->vcpu_affinity[i])', which is the physical CPU ID
		 *  configured in vm_config->vcpu_affinity
		 */
		pcpu_id = ffs64(vm_config->vcpu_affinity[i]);
		/** Call prepare_vcpu with the following parameters, in order to prepare the vCPU associated with the
		 *  given 'vm',  and discard its return value.
		 *  - vm
		 *  - pcpu_id
		 */
		(void)prepare_vcpu(vm, pcpu_id);
	}

	/** Return 0 */
	return 0;
}

/**
 * @brief Shutdown the given VM and release its related resources
 *
 * This function is called to shutdown the given VM, and release its related resources. More specifically,
 * this function offlines all its virtual CPUs, release its EPT table and the vPCI devices.
 *
 * @param[inout] vm Pointer to a VM whose resources will be released.
 *
 * @return A status to indicate whether the VM is shutdown successfully.
 *
 * @retval 0 if the state of the given (to shutdown) VM is PAUSED
 * @retval -EINVAL if the given VM is not in PAUSED state.
 *
 * @pre vm != NULL
 * @pre vm->state == VM_PAUSED
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a loop counter used to as index to probe the vCPU of this VM, not initialized.
	 */
	uint16_t i;
	/** Declare the following local variables of type uint16_t.
	 *  - this_pcpu_id representing ID of this physical CPU, not initialized.
	 */
	uint16_t this_pcpu_id;
	/** Declare the following local variables of type uint64_t.
	 *  - mask representing a 'bitmap' of this VM's physical CPU IDs, not initialized.
	 */
	uint64_t mask;
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to one vCPU data structure of this VM, initialized as NULL.
	 */
	struct acrn_vcpu *vcpu = NULL;

	/** Set vm->state to VM_POWERED_OFF */
	vm->state = VM_POWERED_OFF;
	/** Set this_pcpu_id to the return value of 'get_pcpu_id()' */
	this_pcpu_id = get_pcpu_id();
	/** Set mask to the return value of 'get_pcpu_bitmap(vm)', which is the ID bitmap of all the
	 *  physical CPUs which run the given VM
	 */
	mask = get_pcpu_bitmap(vm);

	/** If this_pcpu_id is set in the 'mask' (a bitmap of physical CPU IDs), which means the current
	 *  physical CPU is running the given VM. In this case only an offline flag shall be set as this
	 *  physical CPU can only be offlined later.
	 */
	if (bitmap_test(this_pcpu_id, &mask)) {
		/** Call bitmap_clear_nolock with the following parameters, in order to clear the bit in 'mask'.
		 *  - this_pcpu_id
		 *  - &mask
		 */
		bitmap_clear_nolock(this_pcpu_id, &mask);
		/** Call make_pcpu_offline with the following parameters, in order to set its offline flag.
		 *  - this_pcpu_id
		 */
		make_pcpu_offline(this_pcpu_id);
	}

	/** For each vcpu in the online vCPUs of the given VM, using i as the loop counter */
	foreach_vcpu(i, vm, vcpu) {
		/** Call reset_vcpu with the following parameters, in order to cleanup this vCPU data.
		 *  - vcpu
		 */
		reset_vcpu(vcpu);
		/** Call offline_vcpu with the following parameters, in order to set offline flag in this vCPU.
		 *  - vcpu
		 */
		offline_vcpu(vcpu);

		/** If the physical CPU ID associated with this vCPU is set in the 'mask' */
		if (bitmap_test(pcpuid_from_vcpu(vcpu), &mask)) {
			/** Call make_pcpu_offline with the following parameters, in order to send an offline
			 *  request to the corresponding physical CPU.
			 *  - pcpuid_from_vcpu(vcpu)
			 */
			make_pcpu_offline(pcpuid_from_vcpu(vcpu));
		}
	}

	/** Call wait_pcpus_offline with the following parameters, in order to wait for all the physical CPUs
	 *  to be in the offline state, except current physical CPU.
	 *  - mask
	 */
	wait_pcpus_offline(mask);

	/** Call vpci_cleanup with the following parameters, in order to release vPCI resources.
	 *  - vm
	 */
	vpci_cleanup(vm);

	/** Call deinit_vuart with the following parameters, in order to deinit vUART.
	 *  - vm
	 */
	deinit_vuart(vm);

	/** Call destroy_iommu_domain with the following parameters, in order to release IOMMU resources.
	 *  - vm->iommu
	 */
	destroy_iommu_domain(vm->iommu);

	/** Call destroy_ept with the following parameters, in order to clear the EPT of the given VM.
	 *  - vm
	 */
	destroy_ept(vm);

	/** Return 0 to indicate the shutdown_vm run successfully */
	return 0;
}

/**
 * @brief Start the created VM after all its resources allocated.
 *
 * After a VM's resources have been allocated in prepare_vm, this function is called to kick off its virtual BP
 * to schedule its thread to run guest VM.
 *
 * @param[inout] vm Pointer to the VM which will be launched.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by prepare_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation.
 */
static void start_vm(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - bsp representing the virtual BP pointer, initialized as NULL.
	 */
	struct acrn_vcpu *bsp = NULL;

	/** Set vm->state to VM_STARTED, to change VM state */
	vm->state = VM_STARTED;

	/** Set bsp to the value of vcpu_from_vid(vm, BOOT_CPU_ID), the virtual BP of the VM */
	bsp = vcpu_from_vid(vm, BOOT_CPU_ID);
	/** Call vcpu_make_request with the following parameters, in order to send a request to the virtual BP
	 *  for VMCS data initialization which is a prerequisite to run into guest VM..
	 *  - bsp
	 *  - ACRN_REQUEST_INIT_VMCS
	 */
	vcpu_make_request(bsp, ACRN_REQUEST_INIT_VMCS);
	/** Call launch_vcpu with the following parameters, in order to start the virtual BP.
	 *  - bsp
	 */
	launch_vcpu(bsp);
}

/**
 * @brief Pause a VM by pausing all its vCPUs
 *
 * This function is called to pause all the vCPUs of the given VM, and then set the vm->state as PAUSED.
 *
 * @param[inout] vm Pointer the VM which is to be paused.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre (vm->state == VM_STARTED) || (vm->state == VM_PAUSED)
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules; also called by shutdown_vm
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void pause_vm(struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a loop counter, not initialized.
	 */
	uint16_t i;
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to struct acrn_vcpu data , initialized as NULL.
	 */
	struct acrn_vcpu *vcpu = NULL;

	/** If vm->state is VM_STARTED, just a started VM can be paused */
	if (vm->state == VM_STARTED) {
		/** For each vcpu in the online vCPUs of the given VM, using i as the loop counter */
		foreach_vcpu(i, vm, vcpu) {
			/** Call pause_vcpu with the following parameters, in order to switch the vCPU to
			 *  VCPU_ZOMBIE state.
			 *  - vcpu
			 */
			pause_vcpu(vcpu);
		}

		/** Set vm->state to VM_PAUSED */
		vm->state = VM_PAUSED;
	}
}

/**
 * @brief Prepare a VM instance and launch it
 *
 * This function is called to get a VM instance and initialize the resources, like memory, vCPU, devices, ACPI table
 * and guest OS image/boot arguments. It calls some internal functions to do the work. Also it calls the function
 * start_vm to launch the VM.
 *
 * @param[in] vm_id The ID of the VM instance.which will be created.
 * @param[in] vm_config A pointer to the VM configuration data.
 *
 * @return None
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @pre vm_config != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is a public API called by other modules; also called by launch_vms
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vm_id is different among parallel invocation
 */
void prepare_vm(uint16_t vm_id, const struct acrn_vm_config *vm_config)
{
	/** Declare the following local variables of type int32_t.
	 *  - err representing the return value of create_vm , initialized as 0.
	 */
	int32_t err = 0;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM to be created by create_vm, initialized as NULL.
	 */
	struct acrn_vm *vm = NULL;

	/** Set err to the return value of create_vm(vm_id, vm_config, &vm) */
	err = create_vm(vm_id, vm_config, &vm);

	/** If err equals 0, which means the create_vm has done successfully. */
	if (err == 0) {
		/** If 'vm' is not a safety VM */
		if (!is_safety_vm(vm)) {
			/** Call build_vacpi with the following parameters, in order to
			 *  build ACPI tables for the created VM.
			 *  - vm
			 */
			build_vacpi(vm);
		}

		/** Call direct_boot_sw_loader with the following parameters, in order to load the guest OS
		 *  image and boot arguments. And fill their information into the created VM data structure.
		 *  - vm
		 */
		direct_boot_sw_loader(vm);

		/** Call start_vm with the following parameters, in order to launch the created VM.
		 *  - vm
		 */
		start_vm(vm);

		/** Logging the following information with a log level of LOG_ACRN.
		 *  - vm_id
		 *  - vm_config->name
		 */
		pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
	}
}

/**
 * @brief Launch all VMs which are configured in the VM configuration data
 *
 * This function is called to launch all VMs, which are configured in VM configuration data. This function will be
 * run on each physical CPU, but only when current CPU runs the VM's virtual BP will it create and launch the VM.
 *
 * @param[in] pcpu_id The ID of current running CPU
 *
 * @return None
 *
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is a public API called by init module.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void launch_vms(uint16_t pcpu_id)
{
	/** Declare the following local variables of type uint16_t.
	 *  - vm_id representing the ID of the VMs, used as a loop counter, not initialized.
	 *  - bsp_id representing boot CPU ID of one VM, not initialized.
	 */
	uint16_t vm_id, bsp_id;
	/** Declare the following local variables of type 'const struct acrn_vm_config *'.
	 *  - vm_config representing a pointer to a VM configuration data, not initialized.
	 */
	const struct acrn_vm_config *vm_config;

	/** For each vm_id ranging from 0 to CONFIG_MAX_VM_NUM - 1 [with a step of 1] */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		/** Set vm_config to the return value of get_vm_config(vm_id) */
		vm_config = get_vm_config(vm_id);

		/** Set bsp_id to the return value of get_vm_bsp_pcpu_id(vm_config), which is the VM's BP ID in
		 *  its 'vm_config'
		 */
		bsp_id = get_vm_bsp_pcpu_id(vm_config);
		/** If pcpu_id equals bsp_id, indicating that the current running physical CPU runs the VM's
		 *  virtual BP.
		 */
		if (pcpu_id == bsp_id) {
			/** Call prepare_vm with the following parameters, in order to create and launch a VM.
			 *  - vm_id
			 *  - vm_config
			 */
			prepare_vm(vm_id, vm_config);
		}
	}
}

/**
 * @brief Make a request to the given physical CPU that the VM is running on to be set offline
 *
 * This function is called to make a request to the given physical CPU that the VM is running on to be set offline.
 * It sets a flag of NEED_SHUTDOWN_VM on the physical CPU's flag. And if the physical CPU is not the current
 * running CPU, send an IPI to it.
 *
 * @param[in] pcpu_id ID of the physical CPU which will be offline
 *
 * @return None
 *
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void make_shutdown_vm_request(uint16_t pcpu_id)
{
	/** Call bitmap_set_lock with the following parameters, in order set the flag into the pcpu_flag field in
	 *  the per-CPU region of the given physical CPU.
	 *  - NEED_SHUTDOWN_VM
	 *  - &per_cpu(pcpu_flag, pcpu_id)
	 */
	bitmap_set_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
	/** If get_pcpu_id() is not pcpu_id, which means current running CPU is not the given physical CPU */
	if (get_pcpu_id() != pcpu_id) {
		/** Call send_single_init with the following parameters, in order to send IPI to the physical CPU.
		 *  - pcpu_id
		 */
		send_single_init(pcpu_id);
	}
}

/**
 * @brief Check whether the physical CPU need to be set offline
 *
 * This function is called to check whether the physical CPU need to be set offline. It checks the flag
 * in pcpu_flag of the CPU's private data. If the flag is set, then return true and clear it.
 *
 * @param[in] pcpu_id  ID of the physical CPU to be checked
 *
 * @return true If NEED_SHUTDOWN_VM is set in pcpu_flag of the physical CPU, or false otherwise
 *
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
bool need_shutdown_vm(uint16_t pcpu_id)
{
	/** Call bitmap_test_and_clear_lock with the following parameters, in order to check and clear the flag
	 *  in the pcpu_flag field in the per-CPU region of the given physical CPU, and return its return value.
	 *  - NEED_SHUTDOWN_VM
	 *  - &per_cpu(pcpu_flag, pcpu_id)
	 */
	return bitmap_test_and_clear_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
}

/**
 * @}
 */
