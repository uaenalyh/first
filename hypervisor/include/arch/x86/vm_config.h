/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIG_H_
#define VM_CONFIG_H_

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#include <types.h>
#include <pci.h>
#include <multiboot.h>
#include <acrn_common.h>
#include <vm_configurations.h>

#define AFFINITY_CPU(n)      (1U << (n))
#define MAX_VCPUS_PER_VM     MAX_PCPU_NUM
#define MAX_VUART_NUM_PER_VM 2U
#define MAX_VM_OS_NAME_LEN   32U
#define MAX_MOD_TAG_LEN      32U

#define PCI_DEV_TYPE_PTDEV  (1U << 0U)
#define PCI_DEV_TYPE_HVEMUL (1U << 1U)

struct acrn_vm_mem_config {
	uint64_t start_hpa; /* the start HPA of VM memory configuration, for pre-launched VMs only */
	uint64_t size; /* VM memory size configuration */
};

struct target_vuart {
	uint8_t vm_id; /* target VM id */
	uint8_t vuart_id; /* target vuart index in a VM */
};

enum vuart_type {
	VUART_LEGACY_PIO = 0, /* legacy PIO vuart */
	VUART_PCI, /* PCI vuart, may removed */
};

union vuart_addr {
	uint16_t port_base; /* addr for legacy type */
	struct { /* addr for pci type */
		uint8_t f : 3; /* BITs 0-2 */
		uint8_t d : 5; /* BITs 3-7 */
		uint8_t b; /* BITs 8-15 */
	} bdf;
};

struct vuart_config {
	enum vuart_type type; /* legacy PIO or PCI  */
	union vuart_addr addr; /* port addr if in legacy type, or bdf addr if in pci type */
	uint16_t irq;
	struct target_vuart t_vuart; /* target vuart */
} __aligned(8);

enum os_kernel_type {
	KERNEL_BZIMAGE = 1,
	KERNEL_ZEPHYR,
};

struct acrn_vm_os_config {
	char name[MAX_VM_OS_NAME_LEN]; /* OS name, useful for debug */
	enum os_kernel_type kernel_type; /* used for kernel specifc loading method */
	char kernel_mod_tag[MAX_MOD_TAG_LEN]; /* multiboot module tag for kernel */
	char bootargs[MAX_BOOTARGS_SIZE]; /* boot args/cmdline */
	uint64_t kernel_load_addr;
	uint64_t kernel_entry_addr;
} __aligned(8);

struct acrn_vm_pci_dev_config {
	uint32_t emu_type; /* the type how the device is emulated. */
	union pci_bdf vbdf; /* virtual BDF of PCI device */
	union pci_bdf pbdf; /* physical BDF of PCI device */
	uint64_t vbar_base[PCI_BAR_COUNT]; /* vbar base address of PCI device */
	const struct pci_vdev_ops *vdev_ops; /* operations for PCI CFG read/write */
} __aligned(8);

struct acrn_vm_config {
	char name[MAX_VM_OS_NAME_LEN]; /* VM name identifier, useful for debug. */
	uint16_t vcpu_num; /* Number of vCPUs for the VM */

	uint64_t vcpu_affinity[MAX_VCPUS_PER_VM]; /* bitmaps for vCPUs' affinity */
	uint64_t guest_flags; /* VM flags that we want to configure for guest
			       * Now we have two flags:
			       *	GUEST_FLAG_SECURE_WORLD_ENABLED
			       *	GUEST_FLAG_LAPIC_PASSTHROUGH
			       * We could add more guest flags in future;
			       */
	struct acrn_vm_mem_config memory; /* memory configuration of VM */
	uint16_t pci_dev_num; /* indicate how many PCI devices in VM */
	struct acrn_vm_pci_dev_config *pci_devs; /* point to PCI devices BDF list */
	struct acrn_vm_os_config os_config; /* OS information the VM */

	struct vuart_config vuart[MAX_VUART_NUM_PER_VM]; /* vuart configuration for VM */
} __aligned(8);

struct acrn_vm_config *get_vm_config(uint16_t vm_id);

extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];

/**
 * @}
 */

#endif /* VM_CONFIG_H_ */
