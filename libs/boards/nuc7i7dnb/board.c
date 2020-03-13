/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <vtd.h>

/**
 * @addtogroup hwmgmt_vtd
 *
 * @{
 */

/**
 * @file
 * @brief This file defines the global variables that store the physical information of the remapping hardware.
 *
 * It is supposed to be used when hypervisor accesses the remapping hardware.
 */

/**
 * @brief A global array of type 'struct dmar_dev_scope' that stores the physical information of
 *        all Device Scope Entries under the scope of DRHD0.
 */
static struct dmar_dev_scope drhd0_dev_scope[DRHD0_DEV_CNT] = {
	{
		.type = DRHD0_DEVSCOPE0_TYPE,
		.id = DRHD0_DEVSCOPE0_ID,
		.bus = DRHD0_DEVSCOPE0_BUS,
		.devfun = DRHD0_DEVSCOPE0_PATH
	},
};

/**
 * @brief A global array of type 'struct dmar_dev_scope' that stores the physical information of
 *        all Device Scope Entries under the scope of DRHD1.
 */
static struct dmar_dev_scope drhd1_dev_scope[DRHD1_DEV_CNT] = {
	{
		.type = DRHD1_DEVSCOPE0_TYPE,
		.id = DRHD1_DEVSCOPE0_ID,
		.bus = DRHD1_DEVSCOPE0_BUS,
		.devfun = DRHD1_DEVSCOPE0_PATH
	},
	{
		.type = DRHD1_DEVSCOPE1_TYPE,
		.id = DRHD1_DEVSCOPE1_ID,
		.bus = DRHD1_DEVSCOPE1_BUS,
		.devfun = DRHD1_DEVSCOPE1_PATH
	},
};

/**
 * @brief A global array of type 'struct dmar_drhd' that stores the physical information of
 *        all DRHD structures.
 */
static struct dmar_drhd drhd_info_array[DRHD_COUNT] = {
	{
		.dev_cnt = DRHD0_DEV_CNT,
		.segment = DRHD0_SEGMENT,
		.flags = DRHD0_FLAGS,
		.reg_base_addr = DRHD0_REG_BASE,
		.ignore = DRHD0_IGNORE,
		.devices = drhd0_dev_scope
	},
	{
		.dev_cnt = DRHD1_DEV_CNT,
		.segment = DRHD1_SEGMENT,
		.flags = DRHD1_FLAGS,
		.reg_base_addr = DRHD1_REG_BASE,
		.ignore = DRHD1_IGNORE,
		.devices = drhd1_dev_scope
	},
};

/**
 * @brief A global variable of type 'struct dmar_info' that stores the physical information of
 *        the remapping hardware.
 */
struct dmar_info plat_dmar_info = {
	.drhd_count = DRHD_COUNT,
	.drhd_units = drhd_info_array,
};

/**
 * @}
 */
