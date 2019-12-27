/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H

/**
 * @addtogroup vp-dm_ptirq
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */
#include <list.h>
#include <spinlock.h>
#include <timer.h>

union irte_index {
	uint16_t index;
	struct {
		uint16_t index_low : 15;
		uint16_t index_high : 1;
	} bits __packed;
};

/*
 * Macros for bits in union msi_addr_reg
 */

#define MSI_ADDR_RH               0x1U /* Redirection Hint */
#define MSI_ADDR_DESTMODE_LOGICAL 0x1U /* Destination Mode: Logical*/
#define MSI_ADDR_DESTMODE_PHYS    0x0U /* Destination Mode: Physical*/

union msi_addr_reg {
	uint64_t full;
	struct {
		uint32_t rsvd_1 : 2;
		uint32_t dest_mode : 1;
		uint32_t rh : 1;
		uint32_t rsvd_2 : 8;
		uint32_t dest_field : 8;
		uint32_t addr_base : 12;
		uint32_t hi_32;
	} bits __packed;
	struct {
		uint32_t rsvd_1 : 2;
		uint32_t intr_index_high : 1;
		uint32_t shv : 1;
		uint32_t intr_format : 1;
		uint32_t intr_index_low : 15;
		uint32_t constant : 12;
		uint32_t hi_32;
	} ir_bits __packed;
};

/*
 * Macros for bits in union msi_data_reg
 */

#define MSI_DATA_DELMODE_FIXED 0x0U /* Delivery Mode: Fixed */
#define MSI_DATA_DELMODE_LOPRI 0x1U /* Delivery Mode: Low Priority */

union msi_data_reg {
	uint32_t full;
	struct {
		uint32_t vector : 8;
		uint32_t delivery_mode : 3;
		uint32_t rsvd_1 : 3;
		uint32_t level : 1;
		uint32_t trigger_mode : 1;
		uint32_t rsvd_2 : 16;
	} bits __packed;
};

/* entry per guest virt vector */
struct ptirq_msi_info {
	union msi_addr_reg vmsi_addr; /* virt msi_addr */
	union msi_data_reg vmsi_data; /* virt msi_data */
	union msi_addr_reg pmsi_addr; /* phys msi_addr */
	union msi_data_reg pmsi_data; /* phys msi_data */
};

/**
 * @}
 */

#endif /* PTDEV_H */
