/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vm_config.h>
#include <vuart.h>

/**
 * @addtogroup vp-base_vm-config
 *
 * @{
 */

  /**
  * @file
  * @brief This file defines global array variable 'vm_configs[]' containing static configurations
  *	for all guest VMs for hypervisor logical partition mode.
  *
  * Usage Remarks: Function 'get_vm_config()' references 'vm_configs[]'.
  */

extern struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM];
extern struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM];

/**
 * @brief Global array variable defines configurations for guest VMs.
 *
 *  Each guest VM takes a place in this array, and configurations of guest VMs include:
 *	- Name of guest VM
 *	- Number of vCPU
 *	- Affinity setting of vCPU
 *	- Flags setting of guest VM
 *	- Memory information of guest VM
 *	- Configurations of guest OS kernel
 *	- Configurations of virtual UART device
 *	- PCI devices of guest VM
 **/
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		.name = "ACRN PRE-LAUNCHED VM0", /**< Name of first guest VM */
		.vcpu_num = 1U, /**< Number of virtual CPUs */
		.vcpu_affinity = VM0_CONFIG_VCPU_AFFINITY, /**< Bitmap of vCPU affinity */
		.guest_flags = GUEST_FLAG_HIGHEST_SEVERITY, /**< Flags setting of guest VM */
		.memory = { /**< Memory information of guest VM */
			.start_hpa = VM0_CONFIG_MEM_START_HPA, /**< Start host physical address */
			.size = VM0_CONFIG_MEM_SIZE, /**< Size of memory in bytes */
		},
		.os_config = { /**< Configurations of guest kernel */
			.name = "Zephyr", /**< Name of guest OS */
			.kernel_type = KERNEL_ZEPHYR, /**< Image format of Zephyr kernel */
			.kernel_mod_tag = "zephyr", /**< Module tag of Zephyr */
			.bootargs = "", /**< No Boogargs is specified */
			.kernel_load_addr = 0x100000UL, /**< Load physical address of Zephyr kernel */
			.kernel_entry_addr = 0x100000UL, /**< Physical address of kernel entry */
		},
		.vuart[0] = { /**< Configurations of first virtual UART device */
			.type = VUART_LEGACY_PIO, /**< Port I/O based type */
			.addr.port_base = COM1_BASE, /**< Base I/O port address */
			.irq = COM1_IRQ, /**< IRQ number */
		},
		.pci_dev_num = VM0_CONFIG_PCI_DEV_NUM, /**< Number of PCI devices for first guest VM */
		.pci_devs = vm0_pci_devs, /**< PCI devices list for first guest VM */
	},
	{	/* VM1 */
		.name = "ACRN PRE-LAUNCHED VM1", /**< Name of second guest VM */
		.vcpu_num = 3U, /**< Number of virtual CPU */
		.vcpu_affinity = VM1_CONFIG_VCPU_AFFINITY, /**< Bitmap of vCPU affinity */
		.memory = { /**< Memory information of guest VM */
			.start_hpa = VM1_CONFIG_MEM_START_HPA, /**< Start host physical address */
			.size = VM1_CONFIG_MEM_SIZE, /**< Size of memory in bytes */
		},
		.os_config = { /**< Configurations of guest kernel */
			.name = "ClearLinux", /**< Name of guest OS */
			.kernel_type = KERNEL_BZIMAGE, /**< Image format of Linux kernel */
			.kernel_mod_tag = "linux", /**< Module tag of Linux */
			.bootargs = VM1_CONFIG_OS_BOOTARG_CONSOLE	\
				VM1_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM1_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet console=hvc0 \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable xapic_phys intel_iommu=off panic=0"
		},
		.vuart[0] = { /**< Configurations of first virtual UART device */
			.type = VUART_LEGACY_PIO, /**< Port I/O based type */
			.addr.port_base = COM1_BASE, /**< Base I/O port address */
			.irq = COM1_IRQ, /**< IRQ number */
		},
		.pci_dev_num = VM1_CONFIG_PCI_DEV_NUM, /**< Number of PCI devices for first guest VM */
		.pci_devs = vm1_pci_devs, /**< PCI devices list for first guest VM */
	},
};

/**
 * @}
 */
