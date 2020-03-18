/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H

#include <list.h>
#include <spinlock.h>
#include <timer.h>

/**
 * @addtogroup vp-dm_ptirq
 *
 * @{
 */

/**
 * @file
 * @brief This file defines data structures and macros of vp-dm.ptirq module.
 */

/**
 * @brief This data structure defines the interrupt remapping index.
 *
 * @consistency N/A
 * @alignment 2
 *
 * @remark N/A
 */
union irte_index {
	uint16_t index; /**< The 16 bit interrupt remapping index */
	/**
	 * @brief Bits decode of the 16 bits index
	 *
	 * @consistency N/A
	 * @alignment 2
	 *
	 * @remark N/A
	 */
	struct {
		uint16_t index_low : 15; /**< The low 15 bits of index */
		uint16_t index_high : 1; /**< The highest bit of index */
	} bits __packed; /**< Bits decode of index */
};

/**
 * @brief The value for setting of Redirection Hint field in the MSI Address Register.
 *        It means to utilize destination mode to determine message recipient.
 */
#define MSI_ADDR_RH               0x1U
#define MSI_ADDR_DESTMODE_LOGICAL 0x1U /**< Destination Mode: Logical */
#define MSI_ADDR_DESTMODE_PHYS    0x0U /**< Destination Mode: Physical */

/**
 * @brief This data structure defines the MSI Address Register format.
 *
 * @consistency N/A
 * @alignment 8
 *
 * @remark N/A
 */
union msi_addr_reg {
	uint64_t full;
	/**
	 * @brief Bits decode of the 64 bits MSI Address Register as Compatibility Format
	 *
	 * @consistency N/A
	 * @alignment 8
	 *
	 * @remark N/A
	 */
	struct {
		uint32_t rsvd_1 : 2; /**< The Reserved field */
		uint32_t dest_mode : 1; /**< The Destination Mode field */
		uint32_t rh : 1; /** The Redirection Hint field */
		uint32_t rsvd_2 : 8; /**< The Reserved field */
		uint32_t dest_field : 8; /**< The Destination field */
		uint32_t addr_base : 12; /**< The Address Base field */
		uint32_t hi_32; /**< The high 32 bits of full */
	} bits __packed; /**< Bits of full as Compatibility Format */
	/**
	 * @brief Bits decode of the 64 bits MSI Address Register as Remappable Format
	 *
	 * @consistency N/A
	 * @alignment 8
	 *
	 * @remark N/A
	 */
	struct {
		uint32_t rsvd_1 : 2; /**< The Reserved field */
		/**
		 * @brief The most significant bit of the 16-bit index.
		 *
		 * This field along with 'intr_index_low' provides
		 * a 16-bit interrupt index.
		 */
		uint32_t intr_index_high : 1;
		uint32_t shv : 1; /**< SubHandle Valid field */
		uint32_t intr_format : 1; /**< Interrupt Format which must have a value of 1b */
		/**
		 * @brief The low 15-bits of the interrupt index.
		 *
		 * This field along with the filed 'intr_index_high'
		 * provides a 16-bit interrupt index.
		 */
		uint32_t intr_index_low : 15;
		uint32_t constant : 12; /**< Interrupt Identifier which must have a value of FEEH */
		uint32_t hi_32; /**< The high 32 bits of full */
	} ir_bits __packed; /**< Bits of full as Remappable Format */
};

#define MSI_DATA_DELMODE_FIXED 0x0U /**< Delivery Mode: Fixed */
#define MSI_DATA_DELMODE_LOPRI 0x1U /**< Delivery Mode: Low Priority */

/**
 * @brief This data structure defines the the MSI Data Register format.
 *
 * @consistency N/A
 * @alignment 4
 *
 * @remark N/A
 */
union msi_data_reg {
	uint32_t full; /**< The 32 bit the MSI Data Register */
	/**
	 * @brief Bits decode of the 32 bits the MSI Data Register
	 *
	 * @consistency N/A
	 * @alignment 4
	 *
	 * @remark N/A
	 */
	struct {
		uint32_t vector : 8; /**< The vector field in the MSI Data Register */
		uint32_t delivery_mode : 3; /**< The delivery mode field in the MSI Data Register */
		uint32_t rsvd_1 : 3; /**< The reserved field in the MSI Data Register */
		uint32_t level : 1;  /**< The level field in the MSI Data Register */
		uint32_t trigger_mode : 1; /**< The level field in the MSI Data Register */
		uint32_t rsvd_2 : 16; /**< The reserved field in the MSI Data Register */
	} bits __packed; /**< Bits decode of full */
};

/**
 * @brief This data structure contains the physical and virtual MSI information for a
 *        passthrough device.
 *
 * @consistency N/A
 * @alignment 8
 *
 * @remark N/A
 */
struct ptirq_msi_info {
	union msi_addr_reg vmsi_addr; /**< The value of virtual MSI Address Register */
	union msi_data_reg vmsi_data; /**< The value of virtual MSI Data Register */
	union msi_addr_reg pmsi_addr; /**< The value of physical MSI Address Register */
	union msi_data_reg pmsi_data; /**< The value of physical MSI Data Register */
};

/**
 * @}
 */

#endif /* PTDEV_H */
