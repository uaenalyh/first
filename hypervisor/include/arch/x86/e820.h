/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef E820_H
#define E820_H

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the external data structures and APIs provided by
 *	  hwmgmt.configs module for E820 operations.
 */

#include <types.h>

#define E820_TYPE_RAM      1U	/**< RAM memory type for E820 entry */
#define E820_TYPE_RESERVED 2U	/**< RESERVED memory type for E820 entry */

#define E820_MAX_ENTRIES 32U	/**< Maximum number of E820 entries supported by hypervisor */

/**
 * @brief Definition of a single entry in an E820 memory map.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct e820_entry {
	uint64_t baseaddr;/**< Base address of the memory range for this entry. */
	uint64_t length;  /**< Length of the memory range */
	uint32_t type;	  /**< Type of memory region */
} __packed;

/**
 * @brief Definition of memory range information.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct mem_range {
	uint64_t mem_bottom;	/**< Bottom address of memory range */
	uint64_t mem_top;	/**< Top address of memory range */
	uint64_t total_mem_size;/**< Bytes size of memory range */
};

void init_e820(void);

uint64_t e820_alloc_low_memory(uint32_t size_arg);

uint32_t get_e820_entries_count(void);

const struct e820_entry *get_e820_entry(void);

const struct mem_range *get_mem_range_info(void);

/**
 * @}
 */

#endif
