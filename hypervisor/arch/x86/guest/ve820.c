/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>
#include <mmu.h>
#include <vm.h>
#include <reloc.h>
#include <logmsg.h>

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

#define ENTRY_HPA1		2U   /**< Index of the ve820 entry that presents the guest memory region below 4G */
#define VE820_ENTRIES		3U   /**< Number of entries in a ve820 table. */

/** A static array for the ve820 tables for all VMs. */
static struct e820_entry pre_vm_e820[CONFIG_MAX_VM_NUM][E820_MAX_ENTRIES];

/** A pre-defined e820 table template for pre-launched VM */
static const struct e820_entry pre_ve820_template[E820_MAX_ENTRIES] = {
	{
		.baseaddr = 0x0UL,              /**< base address of usable RAM under 1MB entry */
		.length   = 0xF0000UL,		/**< this entry size: 960KB */
		.type     = E820_TYPE_RAM       /**< this entry type: RAM */
	},
	{
		.baseaddr = 0xF0000UL,		/**< base address of a reserved RAM, to save ACPI tables */
		.length   = 0x10000UL,		/**< this entry size: 64KB */
		.type     = E820_TYPE_RESERVED  /**< this entry type: reserved */
	},
	{
		.baseaddr = 0x100000UL,		/**< base address of low-mem entry */
		.length   = 0UL,                /**< this entry size: undefined and shall be filled in at runtime */
		.type     = E820_TYPE_RAM       /**< this entry type: RAM */
	},
};

/**
 * @brief  Fill a e820 table info into a VM data structure.
 *
 * This function is called to instantiate and fill a local e820 table info into
 * a VM data structure.  The e820 table will be handled further and will be used
 * when the VM boots up.
 *
 * The ve820 layout for pre-launched VM is as follows.
 *
 * - entry0: usable under 1MB
 * - entry1: reserved for ACPI Table from 0xf0000 to 0xfffff
 * - entry2: usable from 0x100000 up to the available RAM assigned to the VM
 *
 * @param[inout] vm Pointer to a structure representing the VM whose virtual E820 table is to be created
 *
 * @return None
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
 * @threadsafety When \a vm is different among parallel invocation
 */
void create_prelaunched_vm_e820(struct acrn_vm *vm)
{
	/** Declare the following local variable of type 'struct acrn_vm_config *'.
	 *  - vm_config representing the static configurations of the given VM. */
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/** Call memcpy_s with the following parameters, in order to copy the
	 *  ve820 template to where the actual ve820 for the given VM is
	 *  maintained, and discard its return value.
	 *  - pre_vm_e820[vm->vm_id]
	 *  - E820_MAX_ENTRIES * sizeof(struct e820_entry)
	 *  - pre_ve820_template
	 *  - E820_MAX_ENTRIES * sizeof(struct e820_entry)
	 */
	(void)memcpy_s((void *)pre_vm_e820[vm->vm_id], E820_MAX_ENTRIES * sizeof(struct e820_entry),
		(const void *)pre_ve820_template, E820_MAX_ENTRIES * sizeof(struct e820_entry));

	/** Set pre_vm_e820[vm->vm_id][ENTRY_HPA1].length to
	 *  vm_config->memory.size - MEM_1M, which is the size of available
	 *  memory above 1M while below 4G. */
	pre_vm_e820[vm->vm_id][ENTRY_HPA1].length = vm_config->memory.size - MEM_1M;

	/** Set vm->e820_entries to pre_vm_e820[vm->vm_id] */
	vm->e820_entries = pre_vm_e820[vm->vm_id];

	/** Set vm->e820_entry_num to VE820_ENTRIES, which is the number of entries in the ve820 table. */
	vm->e820_entry_num = VE820_ENTRIES;
}

/**
 * @}
 */
