/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bits.h>
#include <vm_config.h>
#include <logmsg.h>
#include <pgtable.h>

/**
 * @defgroup hwmgmt_configs hwmgmt.configs
 * @ingroup hwmgmt
 * @brief Implementation of APIs to manage physical E820 table, and to get platform memory
 *	  information or static configurations for specific VM.
 *
 * Usage Remark: hwmgmt.cpu module uses this module to manage physical E820 table and to
 *		 request memory buffer for trampoline code.
 *		 hwmgmt.mmu module uses this module to fetch E820 table information and hypervisor
 *		 memory range.
 *		 vp-base.vm, vp-base.vboot and vp-dm.vperipheral modules use this module to get
 *		 static configurations of specific VM.
 *
 * Dependency Justification: This module uses round_page_up() and round_page_down() provided
 *				by hwmgmt.mmu module, and uses debug module to dump information.
 *
 * Dependencies among Files:
 *	- Definitions of miscellaneous macros for hypervisor:
 *		config.h
 *
 *	- Definitions of miscellaneous macros for current physical platform:
 *		misc_cfg.h
 *
 *	- Definitions of Multiboot and E820 related functions and data structures for hypervisor:
 *		multiboot.h
 *		e820.h
 *		e820.c
 *
 *	- Definitions of functions and data structures for guest configurations:
 *		vm_config.h
 *		vm_config.c
 *		pci_dev.c
 *		vm_configurations.c
 *		vm_configurations.h
 *		pci_devices.h
 *
 * @{
 */

/**
 * @file
 * @brief This file implements get_vm_config() function that shall be provided by the hwmgmt.configs module.
 *
 * External APIs:
 *  - get_vm_config()     This function gets the static configurations for specific VM.
 *				   Depends on:
 *					- N/A.
 */

 /**
 * @brief This function gets the static configurations for specific VM.
 *
 * @param[in] vm_id  Target VM ID.
 *
 * @return Pointer to context containing static configurations for a given VM.
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 *
 * @post Return value shall not be NULL.
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	/** Return &vm_configs[vm_id] */
	return &vm_configs[vm_id];
}

/**
 * @}
 */
