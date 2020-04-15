/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

 /**
 * @file
 * @brief This file declares the external data structures provided by hwmgmt.configs module
 *		  for parsing Multiboot information.
 */

#include <types.h>
#define MULTIBOOT_INFO_MAGIC    0x2BADB002U /**< Magic number indicates a Multiboot-compliant boot loader */

#define MULTIBOOT_INFO_HAS_MODS 0x00000008U /**< Flag to be set in the mi_flags for boot modules */

#define MULTIBOOT_INFO_HAS_MMAP 0x00000040U /**< Flag to be set in the mi_flags for memory map */

#define MAX_BOOTARGS_SIZE 2048U /**< Maximum length of the guest OS's command line parameter string */

/**
 * @brief Definition of the format of the Multiboot information structure
 *	which is compliant with Multiboot specification version 0.6.96.
 *
 * @consistency N/A
 *
 * @alignment 4
 *
 * @remark N/A
 */
struct multiboot_info {
	uint32_t mi_flags; /**< The presence and validity of other fields in the Multiboot information structure */

	uint32_t mi_mem_lower; /**< Amount of lower memory in kilobytes, valid if mi_flags sets bit 0 */
	uint32_t mi_mem_upper; /**< Amount of upper memory in kilobytes, valid if mi_flags sets bit 0 */

	uint8_t mi_boot_device_part3; /**< Boot sub-partition, valid if mi_flags sets bit 1 */
	uint8_t mi_boot_device_part2; /**< Sub-partition in the top-level partition, valid if mi_flags sets bit 1 */
	uint8_t mi_boot_device_part1; /**< Top-level boot partition number, valid if mi_flags sets bit 1 */
	uint8_t mi_boot_device_drive; /**< Indicates which BIOS disk device the boot loader loaded the OS image from */

	uint32_t mi_cmdline; /**< Physical address of the command line to be passed to the kernel */

	uint32_t mi_mods_count; /**< Number of modules loaded, valid if mi_flags sets bit 3 */
	uint32_t mi_mods_addr; /**< Physical address of the first module structure, valid if mi_flags sets bit 3 */

	uint32_t mi_elfshdr_num; /**< Number of entries of section header table, valid if mi_flags sets bit 5 */
	uint32_t mi_elfshdr_size; /**< Size of section header table entry, valid if mi_flags sets bit 5 */
	uint32_t mi_elfshdr_addr; /**< Physical address of the ELF section header, valid if mi_flags sets bit 5 */
	uint32_t mi_elfshdr_shndx; /**< Index of names in string table, valid if mi_flags sets bit 5 */

	uint32_t mi_mmap_length; /**< Total size of the buffer containing a memory map, valid if mi_flags sets bit 6 */
	uint32_t mi_mmap_addr; /**< Address of the buffer containing a memory map, valid if mi_flags sets bit 6 */

	uint32_t mi_drives_length; /**< Total size of the first drive structure, valid if mi_flags sets bit 7 */
	uint32_t mi_drives_addr; /**< Physical address of the first drive structure, valid if mi_flags sets bit 7 */

	uint32_t unused_mi_config_table; /**< Unused */

	uint32_t mi_loader_name; /**< Physical address of the name of a boot loader booting the kernel */

	uint32_t unused_mi_apm_table; /**< Unused */

	uint32_t unused_mi_vbe_control_info;  /**< Unused */
	uint32_t unused_mi_vbe_mode_info;     /**< Unused */
	uint32_t unused_mi_vbe_interface_seg; /**< Unused */
	uint32_t unused_mi_vbe_interface_off; /**< Unused */
	uint32_t unused_mi_vbe_interface_len; /**< Unused */
};

/**
 * @brief Definition of the format of memory map structure which is compliant with
 *		  Multiboot specification version 0.6.96.
 *
 * @consistency N/A
 *
 * @alignment 4
 *
 * @remark N/A
 */
struct multiboot_mmap {
	uint32_t size; /**< Size of the this structure in bytes */
	uint64_t baseaddr; /**< Starting address */
	uint64_t length; /**< Size of the memory region in bytes */
	/**
	 * @brief Variety of address range represented.
	 *
	 *		1 indicates available RAM,
	 *		3 indicates usable memory holding ACPI information,
	 *		4 indicates reserved memory,
	 *		5 indicates a memory which is occupied by defective RAM modules,
	 *		all other values currently indicated a reserved area
	 */
	uint32_t type;
} __packed;

/**
 * @brief Definition of the format of module structure following Multiboot specification.
 *
 * @consistency N/A
 *
 * @alignment 4
 *
 * @remark N/A
 */
struct multiboot_module {
	uint32_t mm_mod_start; /**< Start addresses of the boot module itself */
	uint32_t mm_mod_end; /**< End addresses of the boot module itself */
	uint32_t mm_string; /**< An arbitrary string to be associated with that particular boot module */
	uint32_t mm_reserved; /**< Reserved and must be set to 0 by the boot loader */
};

/**
 * @brief Declaration of the Multiboot header physical address.
 *
 * This global array shall be defined externally and be used to store below information
 * when the boot loader invokes the operating system:
 *	boot_regs[0] Stores the value of 'EAX' which shall be 'MULTIBOOT_INFO_MAGIC'.
 *	boot_regs[1] Stores the value of 'EBX' which shall be 32-bit physical address
 *				 of the Multiboot information structure.
 *
 * @consistency N/A
 *
 * @alignment 4
 *
 * @remark N/A
 */
extern uint32_t boot_regs[2];

/**
 * @}
 */

#endif
