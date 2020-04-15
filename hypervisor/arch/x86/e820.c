/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <page.h>
#include <e820.h>
#include <mmu.h>
#include <multiboot.h>
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

 /**
  * @file
  * @brief This file implements APIs provided by hwmgmt.configs module to initialize physical E820
  *	   table and to get information of E820 entries, also implements APIs to allocate memory
  *	   from low memory region and get information of memory region that are managed by hypervisor.
  *
  * Usage Remark: hwmgmt.cpu module uses init_e820() function to initialize physical E820 table,
  *		  and uses e820_alloc_low_memory() to allocate buffer for trampoline code.
  *		  hwmgmt.mmu module uses APIs in this file to get information of E820 table and hypervisor
  *		  memory range.
  *
  * Dependency justification: This file uses round_page_up() and round_page_down() provided by hwmgmt.mmu
  *			      module, and uses debug module to dump information.
  *
  * External APIs:
  * - init_e820()		This function initializes physical E820 entries in hypervisor.
  *				Depends on:
  *				 - dev_dbg()
  *				 - panic()
  *
  * - get_e820_entry()		This function returns the pointer to E820 table in hypervisor
  *				Depends on:
  *				 - N/A.
  *
  * - get_e820_entries_count()	This function returns the number of E820 entries in hypervisor.
  *				Depends on:
  *				 - N/A.
  *
  * - get_mem_range_info()	This function returns the host(hypervisor) memory range information.
  *				Depends on:
  *				 - N/A.
  *
  * - e820_alloc_low_memory()	This function allocates size_arg bytes memory in below
  *				1MB memory region in E820 entries.
  *				Depends on:
  *				 - round_page_up()
  *				 - round_page_down()
  *				 - pr_fatal()
  *
  * Internal functions:
  * - obtain_mem_range_info()	This function updates the memory region for hypervisor.
  *				Depends on:
  *				 - N/A.
  */

/**
 * @brief Global variable indicating number of E820 entries in hypervisor.
 *
 *  The value of this global variable shall be no greater than E820_MAX_ENTRIES.
 **/
static uint32_t hv_e820_entries_nr;

/**
 * @brief Global array containing E820 entries in hypervisor.
 *
 *  The number of valid entries in this table is recorded in global variable hv_e820_entries_nr.
 **/
static struct e820_entry hv_e820[E820_MAX_ENTRIES];

/**
 * @brief Global variable indicating the range information of memory managed in hypervisor.
 *
 *  The information recorded in this variable includes top address, bottom address and bytes
 *  size of the memory range.
 **/
static struct mem_range hv_mem_range;

#define ACRN_DBG_E820 6U /**< Log level of E820 debugging messages */

/**
 * @brief This function updates the memory region for hypervisor.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static void obtain_mem_range_info(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing loop counter, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type struct e820_entry *.
	 *  - entry representing pointer to an E820 entry, not initialized. */
	struct e820_entry *entry;

	/** Set hv_mem_range.mem_bottom to UINT64_MAX */
	hv_mem_range.mem_bottom = UINT64_MAX;
	/** Set hv_mem_range.mem_top to 0H */
	hv_mem_range.mem_top = 0x0UL;
	/** Set hv_mem_range.total_mem_size to 0H */
	hv_mem_range.total_mem_size = 0UL;

	/** For each i ranging from 0 to (hv_e820_entries_nr - 1) with a step of 1 */
	for (i = 0U; i < hv_e820_entries_nr; i++) {
		/** Set entry to &hv_e820[i] */
		entry = &hv_e820[i];

		/** If hv_mem_range.mem_bottom > entry->baseaddr */
		if (hv_mem_range.mem_bottom > entry->baseaddr) {
			/** Set hv_mem_range.mem_bottom to entry->baseaddr */
			hv_mem_range.mem_bottom = entry->baseaddr;
		}

		/** If (entry->baseaddr + entry->length) > hv_mem_range.mem_top */
		if ((entry->baseaddr + entry->length) > hv_mem_range.mem_top) {
			/** Set hv_mem_range.mem_top to entry->baseaddr + entry->length */
			hv_mem_range.mem_top = entry->baseaddr + entry->length;
		}

		/** If entry->type == E820_TYPE_RAM */
		if (entry->type == E820_TYPE_RAM) {
			/** Increment hv_mem_range.total_mem_size by entry->length */
			hv_mem_range.total_mem_size += entry->length;
		}
	}
}

/**
 * @brief This function allocates size_arg bytes memory in below 1MB memory
 *  region in E820 entries.
 *
 * @param[in]	size_arg    Number of bytes to allocate.
 *
 * @return Host physical address(HPA) of the beginning of newly allocated memory.
 *
 * @retval ACRN_INVALID_HPA  On failure of allocation.
 *
 * @retval Other-than-ACRN_INVALID_HPA Allocation succeed.
 *
 * @pre size_arg & 0FFFH == 0H
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
uint64_t e820_alloc_low_memory(uint32_t size_arg)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing loop counter, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type uint32_t.
	 *  - size representing size of memory region, initialized as size_arg. */
	uint32_t size = size_arg;
	/** Declare the following local variables of type uint64_t.
	 *  - ret representing base address of allocated memory, initialized as ACRN_INVALID_HPA. */
	uint64_t ret = ACRN_INVALID_HPA;
	/** Declare the following local variables of type struct e820_entry *.
	 *  - entry representing pointer to E820 entry, not initialized. */
	struct e820_entry *entry;

	/** Set size to (((size + PAGE_SIZE) - 1U) >> PAGE_SHIFT) << PAGE_SHIFT,
	 *  means size shall be integral multiple of PAGE_SIZE */
	size = (((size + PAGE_SIZE) - 1U) >> PAGE_SHIFT) << PAGE_SHIFT;

	/** For each i ranging from 0 to (hv_e820_entries_nr - 1) with a step of 1 */
	for (i = 0U; i < hv_e820_entries_nr; i++) {
		/** Set entry to &hv_e820[i] */
		entry = &hv_e820[i];
		/** Declare the following local variables of type uint64_t.
		 *  - start representing start address of memory region, not initialized.
		 *  - end representing end address of memory region, not initialized.
		 *  - length representing size of memory region, not initialized.
		 */
		uint64_t start, end, length;

		/** Set start to round_page_up(entry->baseaddr) */
		start = round_page_up(entry->baseaddr);
		/** Set end to round_page_down(entry->baseaddr + entry->length) */
		end = round_page_down(entry->baseaddr + entry->length);
		/** If 'end' is greater than 'start', set 'length' to 'end - start',
		 *  otherwise, set 'length' to 0.
		 */
		length = (end > start) ? (end - start) : 0U;

		/** If (entry->type != E820_TYPE_RAM) or (length < size) or ((start + size) > MEM_1M) */
		if ((entry->type != E820_TYPE_RAM) || (length < size) || ((start + size) > MEM_1M)) {
			/** Continue to next iteration */
			continue;
		}

		/** If length == size */
		if (length == size) {
			/** Set ret to 'start' */
			ret = start;
			/** Terminate the loop */
			break;
		}

		/** Set ret to (end - size) */
		ret = end - size;
		/** Terminate the loop */
		break;
	}

	/** If ret == ACRN_INVALID_HPA, which means no memory region is allocated. */
	if (ret == ACRN_INVALID_HPA) {
		/** Logging the following information with a log level of LOG_FATAL.
		 *  - Can't allocate memory under 1M from E820\n" */
		pr_fatal("Can't allocate memory under 1M from E820\n");
	}
	/** Return ret, which is the base address of allocated memory region
	 *  or ACRN_INVALID_HPA if allocation failed. */
	return ret;
}

/**
 * @brief This function initializes physical E820 entries in hypervisor.
 *
 * This function parses the E820 table information on current physical platform
 * and updates memory range information for hypervisor.
 *
 * @return None
 *
 * @pre boot_regs[0] == MULTIBOOT_INFO_MAGIC
 * @pre (((struct multiboot_info *)boot_regs[1])->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark While in HV_INIT mode, ACRN hypervisor shall keep the physical Multiboot
 *		   information structure unchanged.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
void init_e820(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing loop counter, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type uint64_t.
	 *  - top_addr_space representing top address for memory address space,
	 *	  initialized as CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE. */
	uint64_t top_addr_space = CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE;

	/** If boot_regs[0] == MULTIBOOT_INFO_MAGIC */
	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		/** Declare the following local variables of type uint64_t.
		 *  - hpa representing host physical address, initialized as (uint64_t)boot_regs[1].
		 * Before installing new PML4 table in enable_paging(), HPA->HVA is always 1:1 mapping
		 * and hpa2hva() can't be used to do the conversion. Here we simply treat boot_reg[1] as HPA.
		 */
		uint64_t hpa = (uint64_t)boot_regs[1];
		/** Declare the following local variables of type struct multiboot_info *.
		 *  - mbi representing base address to instance of struct multiboot_info,
		 *	  which is multiboot information data structure defined by multiboot specification,
		 *	  through this structure the boot loader communicates vital information to hypervisor.
		 *	  mbi is initialized as (struct multiboot_info *)hpa. */
		struct multiboot_info *mbi = (struct multiboot_info *)hpa;

		/** Logging the following information with a log level of LOG_INFO.
		 *  - "Multiboot info detected\n" */
		pr_info("Multiboot info detected\n");
		/** If (mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) is not 0,
		 *	means MULTIBOOT_INFO_HAS_MMAP bit is set in mbi->mi_flags */
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) {
			/* HPA->HVA is always 1:1 mapping at this moment */
			/** Set hpa to (uint64_t)mbi->mi_mmap_addr */
			hpa = (uint64_t)mbi->mi_mmap_addr;
			/** Declare the following local variables of type struct multiboot_mmap *.
			 *  - mmap representing pointer to struct multiboot_mmap instance,
			 *	  initialized as (struct multiboot_mmap *)hpa.
			 *	  which indicates the address and length of a buffer containing
			 *	  a memory map of the machine provided by the platform firmware.*/
			struct multiboot_mmap *mmap = (struct multiboot_mmap *)hpa;

			/** Set hv_e820_entries_nr to mbi->mi_mmap_length / sizeof(struct multiboot_mmap) */
			hv_e820_entries_nr = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
			/** If hv_e820_entries_nr > E820_MAX_ENTRIES */
			if (hv_e820_entries_nr > E820_MAX_ENTRIES) {
				/** Logging the following information with a log level of LOG_ERROR.
				 *  - hv_e820_entries_nr
				 */
				pr_err("Too many E820 entries %d\n", hv_e820_entries_nr);
				/** Set hv_e820_entries_nr to E820_MAX_ENTRIES */
				hv_e820_entries_nr = E820_MAX_ENTRIES;
			}

			/** Logging the following information with a log level of ACRN_DEBUG_E820:
			 *  - mbi->mi_mmap_length
			 *  - mbi->mi_mmap_addr
			 *  - hv_e820_entries_nr
			 */
			dev_dbg(ACRN_DBG_E820, "mmap length 0x%x addr 0x%x entries %d\n", mbi->mi_mmap_length,
				mbi->mi_mmap_addr, hv_e820_entries_nr);

			/** For each i ranging from 0 to (hv_e820_entries_nr -1) with a step of 1 */
			for (i = 0U; i < hv_e820_entries_nr; i++) {
				/** If mmap[i].baseaddr >= top_addr_space */
				if (mmap[i].baseaddr >= top_addr_space) {
					/** Set mmap[i].length to 0 */
					mmap[i].length = 0UL;
				} else {
					/** If (mmap[i].baseaddr + mmap[i].length) > top_addr_space */
					if ((mmap[i].baseaddr + mmap[i].length) > top_addr_space) {
						/** Set mmap[i].length to top_addr_space - mmap[i].baseaddr */
						mmap[i].length = top_addr_space - mmap[i].baseaddr;
					}
				}

				/** Set hv_e820[i].baseaddr to mmap[i].baseaddr */
				hv_e820[i].baseaddr = mmap[i].baseaddr;
				/** Set hv_e820[i].length to mmap[i].length */
				hv_e820[i].length = mmap[i].length;
				/** Set hv_e820[i].type to mmap[i].type */
				hv_e820[i].type = mmap[i].type;

				/** Logging the following information with a log level of ACRN_DEBUG_E820:
				 *  - mmap[i].type.
				 **/
				dev_dbg(ACRN_DBG_E820, "mmap table: %d type: 0x%x\n", i, mmap[i].type);

				/** Logging the following information with a log level of ACRN_DEBUG_E820:
				 *  - mmap[i].baseaddr
				 *  - mmap[i].length */
				dev_dbg(ACRN_DBG_E820, "Base: 0x%016lx length: 0x%016lx", mmap[i].baseaddr,
					mmap[i].length);
			}
		} else {
			/** Call panic() with the following parameters, in order to print fatal message and halt system.
			 *  - "no memory map found from multiboot info" */
			panic("no memory map found from multiboot info");
		}

		/** Call obtain_mem_range_info() with the following parameters,
		 *  in order to update memory range information for hypervisor.
		 *  - N/A */
		obtain_mem_range_info();
	} else {
		/** Call panic() with the following parameters, in order to  print fatal message and halt system.
		 *  - "no multiboot info found" */
		panic("no multiboot info found");
	}
}

/**
 * @brief This function returns the number of E820 entries in hypervisor.
 *
 *  Global variable hv_e820_entries_nr presents the number of
 *  the E820 entries in hypervisor.
 *
 * @return Number of E820 entries.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
uint32_t get_e820_entries_count(void)
{
	/** Return hv_e820_entries_nr */
	return hv_e820_entries_nr;
}

/**
 * @brief This function returns the pointer to E820 table in hypervisor.
 *
 *  Global array hv_e820[] contains the E820 entries in hypervisor.
 *
 * @return A pointer to the E820 table in hypervisor, which is the host virtual address of hv_e820[].
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 *
 * @remark All the size fields in the memory map buffer have a value of 20.
 *		   The memory map buffer is located at the host physical address in the
 *		   mmap_addr field in the physical Multiboot information structure.
 * @remark This API can be called only after init_e820 has been called once on any processor.
 */
const struct e820_entry *get_e820_entry(void)
{
	/** Return hv_e820 */
	return hv_e820;
}

/**
 * @brief This function returns the host(hypervisor) memory range information.
 *
 *  This functions returns the pointer to the global variable hv_mem_range which
 *  describes the range information of memory that hypervisor manages.
 *
 * @return Address of hv_mem_range.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 *
 * @remark This API can be called only after init_e820 has been called once on any processor.
 */
const struct mem_range *get_mem_range_info(void)
{
	/** Return the address of global variable hv_mem_range */
	return &hv_mem_range;
}

/**
 * @}
 */
