/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vm_config.h>
#include <vuart.h>

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

extern struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM];
extern struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		.name = "ACRN PRE-LAUNCHED VM0",
		.vcpu_num = 1U,
		.vcpu_affinity = VM0_CONFIG_VCPU_AFFINITY,
		.guest_flags = GUEST_FLAG_HIGHEST_SEVERITY,
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "Zephyr",
			.kernel_type = KERNEL_ZEPHYR,
			.kernel_mod_tag = "zephyr",
			.bootargs = "",
			.kernel_load_addr = 0x100000UL,
			.kernel_entry_addr = 0x100000UL,
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.pci_dev_num = VM0_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm0_pci_devs,
	},
	{	/* VM1 */
		.name = "ACRN PRE-LAUNCHED VM1",
		.vcpu_num = 3U,
		.vcpu_affinity = VM1_CONFIG_VCPU_AFFINITY,
		.memory = {
			.start_hpa = VM1_CONFIG_MEM_START_HPA,
			.size = VM1_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ClearLinux",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "linux",
			.bootargs = VM1_CONFIG_OS_BOOTARG_CONSOLE	\
				VM1_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM1_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet console=hvc0 \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable xapic_phys intel_iommu=off panic=0"
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.pci_dev_num = VM1_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm1_pci_devs,
	},
};

/**
 * @}
 */
