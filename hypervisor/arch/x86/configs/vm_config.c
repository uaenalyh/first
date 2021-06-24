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
 * @defgroup vp-base_vm-config vp-base.vm-config
 * @ingroup vp-base
 * @brief Static VM configuration data
 *
 * The vm-config module provides static configuration data that specify the number of VMs to be launched, the allocation
 * of physical resources (including CPUs, memory and peripherals) to each VM, the settings of virtualized resources and
 * the boot protocol to launch each VM.
 *
 * Usage:
 * - The vp-base.vboot module depends on this one to prepare for the data for setting up VM initial states according to
 *   the specified boot protocol.
 * - The vp-base.vm module depends on this one to prepare for the resources allocated to each VM.
 * - The vp-dm.vperipheral module depends on this one to initialize the passed-through and virtual PCI functions of each
 *   VM.
 *
 * Dependency:
 * This module does not depend on any other module as it only provides an interface to get the static configuration
 * data.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements get_vm_config() function that shall be provided by the vp-base.vm-config module.
 *
 * External APIs:
 *  - get_vm_config()	This function gets the static configurations for specific VM.
 *			Depends on:
 *			 - N/A.
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
