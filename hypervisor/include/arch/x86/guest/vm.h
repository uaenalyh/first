/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/* Defines for VM Launch and Resume */
#define VM_RESUME 0
#define VM_LAUNCH 1

#ifndef ASSEMBLER

#include <bits.h>
#include <spinlock.h>
#include <vcpu.h>
#include <vioapic.h>
#include <vpic.h>
#include <vmx_io.h>
#include <vuart.h>
#include <trusty.h>
#include <vcpuid.h>
#include <vpci.h>
#include <cpu_caps.h>
#include <e820.h>
#include <vm_config.h>

struct vm_hw_info {
	/* vcpu array of this VM */
	struct acrn_vcpu vcpu_array[MAX_VCPUS_PER_VM];
	uint16_t created_vcpus; /* Number of created vcpus */
} __aligned(PAGE_SIZE);

struct sw_module_info {
	/* sw modules like ramdisk, bootargs, firmware, etc. */
	void *src_addr; /* HVA */
	void *load_addr; /* GPA */
	uint32_t size;
};

struct sw_kernel_info {
	void *kernel_src_addr; /* HVA */
	void *kernel_load_addr; /* GPA */
	void *kernel_entry_addr; /* GPA */
	uint32_t kernel_size;
};

struct vm_sw_info {
	enum os_kernel_type kernel_type; /* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	struct sw_module_info bootargs_info;
};

/* Enumerated type for VM states */
enum vm_state {
	VM_POWERED_OFF = 0,
	VM_CREATED, /* VM created / awaiting start (boot) */
	VM_STARTED, /* VM started (booted) */
	VM_PAUSED, /* VM paused */
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[PAGE_SIZE * 2];

	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	struct memory_ops ept_mem_ops;

} __aligned(PAGE_SIZE);

struct acrn_vm {
	struct vm_arch arch_vm; /* Reference to this VM's arch information */
	struct vm_hw_info hw; /* Reference to this VM's HW information */
	struct vm_sw_info sw; /* Reference to SW associated with this VM */
	uint32_t e820_entry_num;
	const struct e820_entry *e820_entries;
	uint16_t vm_id; /* Virtual machine identifier */
	enum vm_state state; /* VM state */
	struct iommu_domain *iommu; /* iommu domain of this VM */

	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX];

	uint8_t uuid[16];

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
	struct acrn_vpci vpci;

	uint8_t vrtc_offset;
} __aligned(PAGE_SIZE);

/*
 * @pre vlapic != NULL
 */
static inline uint64_t vm_active_cpus(const struct acrn_vm *vm)
{
	uint64_t dmask = 0UL;
	uint16_t i;
	const struct acrn_vcpu *vcpu;

	foreach_vcpu (i, vm, vcpu) {
		bitmap_set_nolock(vcpu->vcpu_id, &dmask);
	}

	return dmask;
}

/*
 * @pre vcpu_id < MAX_VCPUS_PER_VM
 * @pre &(vm->hw.vcpu_array[vcpu_id])->state != VCPU_OFFLINE
 */
static inline struct acrn_vcpu *vcpu_from_vid(struct acrn_vm *vm, uint16_t vcpu_id)
{
	return &(vm->hw.vcpu_array[vcpu_id]);
}

static inline struct acrn_vcpu *vcpu_from_pid(struct acrn_vm *vm, uint16_t pcpu_id)
{
	uint16_t i;
	struct acrn_vcpu *vcpu, *target_vcpu = NULL;

	foreach_vcpu (i, vm, vcpu) {
		if (pcpuid_from_vcpu(vcpu) == pcpu_id) {
			target_vcpu = vcpu;
			break;
		}
	}

	return target_vcpu;
}

void make_shutdown_vm_request(uint16_t pcpu_id);
bool need_shutdown_vm(uint16_t pcpu_id);
int32_t shutdown_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
void start_vm(struct acrn_vm *vm);
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm);
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config);
void launch_vms(uint16_t pcpu_id);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);

int32_t direct_boot_sw_loader(struct acrn_vm *vm);

void vrtc_init(struct acrn_vm *vm);

bool is_safety_vm(const struct acrn_vm *vm);
bool is_rt_vm(const struct acrn_vm *vm);
enum vm_vlapic_state check_vm_vlapic_state(const struct acrn_vm *vm);
#endif /* !ASSEMBLER */

/**
 * @}
 */

#endif /* VM_H_ */
