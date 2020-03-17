/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>
#include <vm.h>

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file implements a function to build e820 table for a VM.
 *
 * This file defines a default local e820 table, which can be used by the guest Linux. Also this file offers a API:
 *
 * - create_prelaunched_vm_e820: called by create_vm
 *
 */

#define VE820_ENTRIES_KBL_NUC_i7 5U /**< Pre-defined total entries of this e820 table */

/**  A pre-defined e820 table for pre-launched VM */
static const struct e820_entry ve820_entry[VE820_ENTRIES_KBL_NUC_i7] = {
	{
		.baseaddr = 0x0UL,  /**< base address of usable RAM under 1MB entry */
		.length = 0xF0000UL, /**< this entry size: 960KB */
		.type = E820_TYPE_RAM }, /**< this entry type: RAM */

	{
		.baseaddr = 0xF0000UL,  /**< base address of a reserved RAM, to save ACPI tables */
		.length = 0x10000UL,  /**< this entry size: 64KB */
		.type = E820_TYPE_RESERVED }, /**< this entry type: reserved */

	{
		.baseaddr = 0x100000UL,  /**< base address of low-mem entry */
		.length = 0x1FF00000UL,  /**< this entry size: 511MB */
		.type = E820_TYPE_RAM },  /**< this entry type: RAM */

	{
		.baseaddr = 0x20000000UL,  /**< base address of PCI hole entry */
		.length = 0xA0000000UL,  /**< this entry size: 2560MB */
		.type = E820_TYPE_RESERVED }, /**< this entry type: reserved */

	{
		.baseaddr = 0xe0000000UL, /**< base address of the entry between PCI hole and 4G */
		.length = 0x20000000UL, /**< this entry size: 512MB */
		.type = E820_TYPE_RESERVED }, /**< this entry type: reserved */
};

/**
 * @brief  Fill a e820 table info into a VM data structure.
 *
 * This function is called to fill a local e820 table info into a VM data structure.
 * The e820 table will be handled further and will be used when the VM boots up.
 *
 * @param[in] vm Pointer to a structure representing the VM whose virtual E820 table is to be created
 *
 * @return N/A
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is called by create_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void create_prelaunched_vm_e820(struct acrn_vm *vm)
{
	/** Set vm->e820_entry_num to VE820_ENTRIES_KBL_NUC_i7, the total number of e820 entries */
	vm->e820_entry_num = VE820_ENTRIES_KBL_NUC_i7;
	/** Set vm->e820_entries to ve820_entry */
	vm->e820_entries = ve820_entry;
}

/**
 * @}
 */
