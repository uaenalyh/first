/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

 /**
 * @file
 * @brief This file defines macros for configurations of VM0 and VM1.
 */

#include <pci_devices.h>
#include <misc_cfg.h>

/**
 * @brief Bits mask of guest flags that can be programmed by device model,
 *		  Other bits are set by hypervisor only.
 *
 **/
#define DM_OWNED_GUEST_FLAG_MASK 0UL /**<  */

#define CONFIG_MAX_VM_NUM 2U /**< Maximum number of guest VMs */


#define VM0_CONFIG_VCPU_AFFINITY \
	{                        \
		AFFINITY_CPU(0U) \
	} /**< Affinity setting of vCPU for VM0 */

#define VM0_CONFIG_MEM_START_HPA      0x100000000UL /**< Start host physical address of VM0 */
#define VM0_CONFIG_MEM_SIZE           0x20000000UL /**< Memory size in bytes of VM0 */


#define VM1_CONFIG_VCPU_AFFINITY \
	{                                                            \
		AFFINITY_CPU(1U), AFFINITY_CPU(2U), AFFINITY_CPU(3U) \
	} /**< Affinity setting of vCPU for VM1 */

#define VM1_CONFIG_MEM_START_HPA      0x120000000UL /**< Start host physical address of VM1 */
#define VM1_CONFIG_MEM_SIZE           0x40000000UL /**< Memory size in bytes of VM1 */
#define VM1_CONFIG_OS_BOOTARG_ROOT    ROOTFS_0 /**< Rootfs in bootargs of VM1 */
#define VM1_CONFIG_OS_BOOTARG_MAXCPUS "maxcpus=3 " /**< maxcpus in bootargs of VM1 */
#define VM1_CONFIG_OS_BOOTARG_CONSOLE "console=ttyS0 " /**< 'console' type in bootargs of VM1 */

#define VM0_NETWORK_CONTROLLER ETHERNET_CONTROLLER /**< Network controller device of VM0 */
#define VM0_CONFIG_PCI_DEV_NUM 2U /**< Number of PCI device for VM0 */

#define VM1_STORAGE_CONTROLLER USB_CONTROLLER /**< Mass Storage controller device of VM1 */
#define VM1_CONFIG_PCI_DEV_NUM 2U /**< Number of PCI device for VM1 */

/**
 * @}
 */

#endif /* VM_CONFIGURATIONS_H */
