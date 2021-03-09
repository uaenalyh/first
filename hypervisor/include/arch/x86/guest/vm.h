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
 * @brief This file declares all external APIs that shall be provided by the vp-base.vm module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * vp-base.vm module.
 */

/* Defines for VM Launch and Resume */
#define VM_RESUME 0    /**< Pre-defined macro used by vmx_vmrun to resume a VM */
#define VM_LAUNCH 1    /**< Pre-defined macro used by vmx_vmrun to launch a VM */

#ifndef ASSEMBLER

#include <bits.h>
#include <spinlock.h>
#include <vcpu.h>
#include <vmx_io.h>
#include <vcpuid.h>
#include <vpci.h>
#include <cpu_caps.h>
#include <e820.h>
#include <vm_config.h>

/**
 * @brief Data structure to represent all vCPUs' info of one VM.
 *
 * This data structure represents all vCPUs' info of one VM. It includes an array of all allowed vCPUs in one VM.
 * It is supposed to be used when the hypervisor creates the vCPUs for one VM, and runs the VM.
 *
 * @consistency N/A
 * @alignment PAGE_SIZE
 *
 * @remark N/A
 */
struct vm_hw_info {
	struct acrn_vcpu vcpu_array[MAX_VCPUS_PER_VM]; /**< The array to store each vCPU info for one VM. */
	uint16_t created_vcpus; /**< Number of created vCPUs of one VM */
} __aligned(PAGE_SIZE);

/**
 * @brief Data structure to represent an image's info used by bootloader.
 *
 * This data structure represents an image's info used by bootloader. Here it is just used for boot-time parameters.
 * The information includes the image's original address in RAM, loading address, and its size. It is supposed
 * to be used when the hypervisor creates/loads a guest OS to run.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct sw_module_info {
	void *src_addr; /**< the HVA where an image is originally saved */
	uint64_t load_addr; /**< the GPA where an image shall be loaded at */
	uint32_t size; /**<  the module size */
};

/**
 * @brief Data structure to represent a kernel's info .
 *
 * This data structure represents a kernel's info, including its original address, loading address, running
 * entry point and its size. It is supposed to be used when the hypervisor creates/loads a guest OS to run.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct sw_kernel_info {
	void *kernel_src_addr; /**< the HVA where a kernel is originally saved */
	uint64_t kernel_load_addr; /**< the GPA where the kernel shall be loaded at */
	uint64_t kernel_entry_addr; /**< the GPA where the entry point starts to run */
	uint32_t kernel_size; /**< the kernel size */
};

/**
 * @brief Data structure to represent a guest OS info, including kernel and boot-time parameters.
 *
 * This data structure represents a guest OS info, including kernel and boot-time parameters info.
 * It is supposed to be used when the hypervisor creates/loads a guest OS to run.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct vm_sw_info {
	enum os_kernel_type kernel_type; /**<  Guest kernel type */
	struct sw_kernel_info kernel_info;  /**< Guest kernel info */
	struct sw_module_info bootargs_info; /**< Guest OS boot-time parameters */
};

/**
 * @brief An enumeration type to indicate a VM's state.
 *
 * This enumeration type represents several states of one VM, including OFF, CREATED, STARTED and PAUSED.
 *
 * @remark N/A
 */
enum vm_state {
	VM_POWERED_OFF = 0, /**< Powered off state of one VM */
	VM_CREATED, /**< Created state of one VM, and awaiting to start (boot) */
	VM_STARTED, /**< Started (booted) state of one VM */
	VM_PAUSED, /**< Paused state of one VM */
};

/**
 * @brief Data structure to represent a VM's I/O bitmap, EPT base addr and EPT ops.
 *
 * This data structure represents a VM's I/O bitmap, EPT base addr and EPT operation callback functions.
 * The instance of this structure is used when the hypervisor created a VM or change the I/O and EPT setting.
 *
 * @consistency N/A
 * @alignment PAGE_SIZE
 *
 * @remark N/A
 */
struct vm_arch {
	uint8_t io_bitmap[PAGE_SIZE * 2U]; /**< the I/O bitmap of one VM */

	void *nworld_eptp;  /**< the pointer to the EPT table of one VM */
	struct memory_ops ept_mem_ops;  /**< the EPT memory operations of one VM */
} __aligned(PAGE_SIZE);

/**
 * @brief Data structure to represent all the info of one VM
 *
 * This data structure represents all the info of one VM, including its vCPU configuration, guest OS kernel,
 * boot arguments and virtual PCI devices. The instance of this structure is used in the whole life cycle of one VM.
 *
 * @consistency N/A
 * @alignment PAGE_SIZE
 *
 * @remark N/A
 */
struct acrn_vm {
	struct vm_arch arch_vm; /**< the member of 'struct vm_arch', including I/O bitmap and EPT table */
	struct vm_hw_info hw; /**< the VM's vCPU information */
	struct vm_sw_info sw; /**< the kernel/boot arguments info associated with this VM */
	uint32_t e820_entry_num; /**< e820 entries number */
	const struct e820_entry *e820_entries; /**< the pointer to the e820 entries */
	uint16_t vm_id; /**< the VM identifier */
	enum vm_state state; /**< the VM state */
	struct iommu_domain *iommu; /**< iommu domain of this VM */

	spinlock_t ept_lock;	/**< Spin-lock used to protect ept add/modify/remove for a VM */

	spinlock_t vm_lock; /**< The lock that protects VM state updates */
	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX]; /**< emulated port I/O handler descriptor */

	uint32_t vcpuid_entry_nr;  /**< the vCPUID entries number */
	uint32_t vcpuid_level;  /**< the maximum leaf of basic function vCPUID information */
	uint32_t vcpuid_xlevel; /**< the maximum leaf of extended function vCPUID information */
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES]; /**< the vCPUID entries info */
	struct acrn_vpci vpci;  /**< the VM's virtual PCI device info */

	uint8_t vrtc_offset; /**< used to store the value in vRTC index register when guest VM accesses its RTC */
} __aligned(PAGE_SIZE);

/**
 * @brief Get all the online vCPU IDs of the given VM
 *
 * This interface is used to get all the online vCPU IDs of the given VM, and fill them into a bitmap.
 *
 * @param[in] vm Pointer to the data structure representing the VM whose online vCPU IDs will be reported.
 *
 * @return A bitmap containing the online vCPU IDs info.
 *
 * @pre vm != NULL
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
static inline uint64_t vm_active_cpus(const struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint64_t.
	 *  - dmask representing a bitmap to be filled the vCPU ID info, initialized as 0.
	 */
	uint64_t dmask = 0UL;
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a loop counter, not initialized.
	 */
	uint16_t i;
	/** Declare the following local variables of type 'const struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to a vCPU data structure, not initialized.
	 */
	const struct acrn_vcpu *vcpu;

	/** For each vcpu in the online vCPUs of vm, using i as the loop counter */
	foreach_vcpu (i, vm, vcpu) {
		/** Call bitmap_set_nolock with the following parameters, in order to set the corresponding bit
		 *  of the vcpu->vcpu_id in 'dmask'.
		 *  - vcpu->vcpu_id
		 *  - &dmask
		 */
		bitmap_set_nolock(vcpu->vcpu_id, &dmask);
	}

	/** Return dmask, the bitmap of the vCPUs ID */
	return dmask;
}

/**
 * @brief Get a pointer to the vCPU data according to its ID and the given VM.
 *
 * This interface is used to get a pointer to the vCPU data according to its vCPU ID and the VM this vCPU
 * belongs to.
 *
 * @param[in] vm Pointer to a VM data structure which the returned vCPU structure is in.
 * @param[in] vcpu_id A vCPU ID used to identify the vCPU structure to be returned.
 *
 * @return A pointer to the vCPU data.
 *
 * @pre vm != NULL
 * @pre vcpu_id < MAX_VCPUS_PER_VM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline struct acrn_vcpu *vcpu_from_vid(struct acrn_vm *vm, uint16_t vcpu_id)
{
	/** Return &(vm->hw.vcpu_array[vcpu_id]), the vCPU data pointer */
	return &(vm->hw.vcpu_array[vcpu_id]);
}

/**
 * @brief Get a pointer to the vCPU data according to its corresponding physical CPU ID and the given VM.
 *
 * This interface is used to get the vCPU data according to its corresponding physical CPU ID and the given VM which
 * this physical CPU runs.
 *
 * @param[in] vm Pointer to a VM data structure which the returned vCPU structure is in.
 * @param[in] pcpu_id The ID of a physical CPU which runs the given VM.
 *
 * @return A pointer to the vCPU data.
 *
 * @retval non-NULL if the physical CPU runs the given VM and there is a corresponding vCPU associated to that
 * physical CPU.
 * @retval NULL if the given VM has no vCPU associated with the give physical CPU.
 *
 * @pre vm != NULL
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline struct acrn_vcpu *vcpu_from_pid(struct acrn_vm *vm, uint16_t pcpu_id)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a loop counter, not initialized.
	 */
	uint16_t i;
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to the vCPU data, not initialized.
	 *  - target_vcpu representing a pointer to the target vCPU data, initialized as NULL.
	 */
	struct acrn_vcpu *vcpu, *target_vcpu = NULL;

	/** For each vcpu in the online vCPUs of vm, using i as the loop counter */
	foreach_vcpu (i, vm, vcpu) {
		/** If pcpu_id equals the return value of pcpuid_from_vcpu(vcpu) */
		if (pcpuid_from_vcpu(vcpu) == pcpu_id) {
			/** Set target_vcpu to vcpu, which is running on the physical CPU (ID is pcpu_id) */
			target_vcpu = vcpu;
			/** Terminate the loop */
			break;
		}
	}

	/** Return target_vcpu */
	return target_vcpu;
}

void make_shutdown_vm_request(uint16_t pcpu_id);
bool need_shutdown_vm(uint16_t pcpu_id);
int32_t shutdown_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
void prepare_vm(uint16_t vm_id, const struct acrn_vm_config *vm_config);
void launch_vms(uint16_t pcpu_id);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);

void direct_boot_sw_loader(struct acrn_vm *vm);

void vrtc_init(struct acrn_vm *vm);

bool is_safety_vm(const struct acrn_vm *vm);
#endif /* !ASSEMBLER */

/**
 * @}
 */

#endif /* VM_H_ */
