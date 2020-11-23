/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIG_H_
#define VM_CONFIG_H_

/**
 * @addtogroup vp-base_vm-config
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the specific for VM configurations external data structures
 *		  and APIs provided by vp-base.vm-config module.
 */

#include <types.h>
#include <pci.h>
#include <multiboot.h>
#include <acrn_common.h>
#include <vm_configurations.h>

/**
 * @brief Bitmap for specific CPU of affinity
 *
 * Given an ID of a physical CPU, this macro expands to a bitmap with only the corresponding bit set. This macro is
 * designed to be used in the initializers of static VM configuration data which define the set of physical CPUs a VM
 * can run on.
 *
 * @param[in] n the ID of the physical CPU
 *
 * @return A 32-bit unsigned integer with bit \a n set and the others cleared
 *
 * @mode NOT_APPLICABLE
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define AFFINITY_CPU(n)      (1U << (n))

#define MAX_VCPUS_PER_VM     MAX_PCPU_NUM /**< Maximum number of virtual CPU for one VM */
#define MAX_VUART_NUM_PER_VM 2U	/**< Maximum number of virtual UART devices for one VM */
#define MAX_VM_OS_NAME_LEN   32U /**< Maximum character number for OS name, including terminated NULL */
#define MAX_MOD_TAG_LEN      32U /**< Maximum character number for module tag name, including terminated NULL */

#define PCI_DEV_TYPE_PTDEV  (1U << 0U) /**< Pass-through PCI device type */
#define PCI_DEV_TYPE_HVEMUL (1U << 1U) /**< Hypervisor emulated PCI device type */

/**
 * @brief Definition of static configurations of the memory region allocated to a VM.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct acrn_vm_mem_config {
	uint64_t start_hpa;	/**< Starting HPA of the memory allocated to a pre-launched VM */
	uint64_t size;		/**< Size of the memory allocated to a VM */
};


/**
 * @brief Definition of virtual UART.
 *
 * @consistency N/A
 *
 * @alignment 1
 *
 * @remark N/A
 */
struct target_vuart {
	uint8_t vm_id;	  /**< Target VM ID */
	uint8_t vuart_id; /**< Target virtual UART index in a VM */
};

/**
 * @brief This enumeration type represents the type of a virtual UART device.
 *
 * @remark N/A
 */
enum vuart_type {
	VUART_LEGACY_PIO = 0, /**< Legacy PIO vuart */
	VUART_PCI,	      /**< PCI vuart, may removed */
};

/**
 * @brief Definition of base address of a virtual UART device.
 *
 * @consistency N/A
 *
 * @alignment 2
 *
 * @remark This is I/O port address for legacy virtual UART device,
 *		   and is a BDF value for PCI virtual UART device.
 */
union vuart_addr {
	uint16_t port_base;	/**< I/O Port address for legacy vUART */
	/**
	 * @brief Definition of BDF value for PCI virtual UART device.
	 *
	 * @consistency N/A
	 *
	 * @alignment 2
	 */
	struct {
		uint8_t f : 3;  /**< PCI function number */
		uint8_t d : 5;  /**< PCI device number */
		uint8_t b;	/**< PCI bus number */
	} bdf;
};

/**
 * @brief Definition of configurations for a virtual UART device.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct vuart_config {
	enum vuart_type type;	/**< vUART type, VUART_LEGACY_PIO or VUART_PCI */
	union vuart_addr addr;	/**< I/O port address for legacy vUART, or BDF value for PCI vUART */
	uint16_t irq;		/**< IRQ for vUART device. */
	struct target_vuart t_vuart; /**< Target vUART */
} __aligned(8);

/**
 * @brief This enumeration type represents the type of image format of OS kernel.
 *
 * @remark N/A
 */
enum os_kernel_type {
	KERNEL_BZIMAGE = 1, /**< Kernel in Linux bzimage format */
	KERNEL_ZEPHYR, /**< Raw Zephyr kernel image */
};

/**
 * @brief Definition of configurations of an OS kernel of a VM.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct acrn_vm_os_config {
	char name[MAX_VM_OS_NAME_LEN];	      /**< OS name */
	enum os_kernel_type kernel_type;      /**< Kernel type, used for kernel specific loading method */
	char kernel_mod_tag[MAX_MOD_TAG_LEN]; /**< Multiboot module tag for kernel */
	char bootargs[MAX_BOOTARGS_SIZE];     /**< Boot arguments */
	uint64_t kernel_load_addr;	/**< Load address of kernel image */
	uint64_t kernel_entry_addr;	/**< Entry address of kernel image */
} __aligned(8);

/**
 * @brief Definition of configurations of a PCI device.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct acrn_vm_pci_dev_config {
	uint32_t emu_type; /**< Type of emulation of PCI device */
	union pci_bdf vbdf; /**< Virtual BDF value of PCI device */
	union pci_bdf pbdf; /**< Physical BDF value of PCI device */
	uint64_t vbar_base[PCI_BAR_COUNT]; /**< Virtual BAR base address of PCI device */
	const struct pci_vdev_ops *vdev_ops; /**< Link to operations for PCI configuration access */
} __aligned(8);

/**
 * @brief Definition of configurations of a VM.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct acrn_vm_config {
	char name[MAX_VM_OS_NAME_LEN];  /**< VM name, for debug usage. */
	uint16_t vcpu_num;		/**< Number of VCPU of the VM */
	uint64_t vcpu_affinity[MAX_VCPUS_PER_VM]; /**< Bitmaps for vCPUs' affinity */
	uint64_t guest_flags; /**< VM flags, only GUEST_FLAG_HIGHEST_SEVERITY is supported */
	struct acrn_vm_mem_config memory; /**< Memory configuration of VM */
	uint16_t pci_dev_num;		  /**< Number of PCI pass-through devices in a VM */
	struct acrn_vm_pci_dev_config *pci_devs; /**< A pointer to the list of all PCI devices pass-throughed to a VM */
	struct acrn_vm_os_config os_config;		 /**< OS information for VM */
	struct vuart_config vuart[MAX_VUART_NUM_PER_VM]; /**< Configurations of virtual UART devices of VM */
} __aligned(8);


struct acrn_vm_config *get_vm_config(uint16_t vm_id);

/**
* @brief Global variable indicating configuration data, each VM configuration data should take a place in this array.
*
*  This global variable shall be defined externally.
**/
extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];

/**
 * @}
 */

#endif /* VM_CONFIG_H_ */
