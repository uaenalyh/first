/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci_devices.h>
#include <vpci.h>

/**
 * @addtogroup vp-base_vm-config
 *
 * @{
 */

/**
 * @file
 * @brief This file defines global variables 'vm0_pci_devs[]' and 'vm1_pci_devs[]' which
 *	  contain information of PCI devices for VM0 and VM1 respectively.
 *
 * Usage Remarks: Global table 'vm_configs[]' of VM configurations references these variables.
 *
 */

/**
 * The vbar_base info of pt devices is included in device MACROs which defined in
 *	   arch/x86/configs/$(CONFIG_BOARD)/pci_devices.h.
 * The memory range of vBAR should exactly match with the e820 layout of VM.
 */

/**
 * @brief Global array variable defines PCI devices for VM0.
 **/
struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL, /**< Emulated by hypervisor */
		.vbdf.bits = { .b = 0x00U, .d = 0x00U, .f = 0x00U }, /**< Virtual address of host bridge */
		.vdev_ops = &vhostbridge_ops, /**< Callback functions for PCI host bridge */
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV, /**< Pass-through device */
		/**
		 * @brief Virtual address of network controller for VM0.
		 **/
		.vbdf.bits = { .b = 0x00U, .d = 0x01U, .f = 0x00U},
		/**
		 * @brief Physical address and virtual BAR configuration of network controller for VM0.
		 **/
		VM0_NETWORK_CONTROLLER
	},
};

/**
 * @brief Global array variable defines PCI devices for VM1.
 *
 **/
struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL, /**< Emulated by hypervisor */
		.vbdf.bits = { .b = 0x00U, .d = 0x00U, .f = 0x00U }, /**< Virtual address of host bridge */
		.vdev_ops = &vhostbridge_ops, /**< Callback functions for PCI host bridge */
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV, /**< Pass-through device */
		/**
		 * @brief Virtual address of storage controller for VM1.
		 **/
		.vbdf.bits = { .b = 0x00U, .d = 0x01U, .f = 0x00U },
		/**
		 * @brief Physical address and virtual BAR configuration of storage controller for VM1.
		 **/
		VM1_STORAGE_CONTROLLER
	},
};

/**
 * @}
 */
