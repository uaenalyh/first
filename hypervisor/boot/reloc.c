/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <reloc.h>
#include <ld_sym.h>

/**
 * @defgroup boot boot
 * @brief The boot component provides entries of the hypervisor on both physical BP and APs, sets up C
 * execution environment and calls the main initialization routine of the hypervisor in the init component.
 *
 * Usage Remarks: Boot module sets up the basic environment for init module and invokes the main initialization
 * routines provided by the init component.This component also provides a few symbols which help locate the
 * location of code and data sections of the hypervisor itself.
 *
 * Dependency Justification: this module locates on the bottom and never depends on any other module.
 *
 * External APIs:
 *  - get_hv_image_delta(): This functions gets the delta between actual load host virtual address(HVA)
 *    and CONFIG_HV_RAM_START.
 *  - get_hv_image_base(): This function gets the actual Hypervisor load host virtual address(HVA).
 *
 * Internal functions are wrappers in assembly code in cpu_primary.S.
 *
 * This module is decomposed into the following files:
 * reloc.c -implements all APIs that shall be provided by the boot
 * cpu_primary.S -set up the initial execution environment
 *
 * @{
 */

/**
 * @file arch/x86/boot/cpu_primary.S
 *
 * @brief This file provides entry point and Multiboot header of the hypervisor
 *
 * This file implements the entry point of ACRN hypervisor, namely cpu_primary_start_32, which sets up a proper C
 * execution environment in 64-bit mode. The GDT and page table are properly set up to establish an identical mapping
 * from the first 4G in the logical address space (in any segment) to the first 4G in the physical address space. Refer
 * to section 11.1.3.3 of the Software Architecture Design Specification for a detailed control flow that specifies its
 * expected behavior.
 *
 * Additionally a Multiboot header is defined with the values defined in section 6.2 of the Software Architecture Design
 * Specification, so that a Multiboot-compliant bootloader can properly load and transfer control to the hypervisor.
 */

/**
 * @file arch/x86/boot/trampoline.S
 *
 * @brief This file provides starting point of application processors
 *
 * This file implements a starting point, namely trampoline_start_16, which sets up a proper C execution environment in
 * 64-bit mode for application processors which start execution from real-address mode. The GDT and page table are
 * properly set up to establish an identical mapping from the first 4G in the logical address space (in any segment) to
 * the first 4G in the physical address space. Refer to section 11.1.3.2 and its related global variables of the
 * Software Architecture Design Specification for a detailed control flow that specifies its expected behavior.
 */

/**
 * @file
 * @brief thie file implements all APIs that shall be provided by the boot module.
 *
 * function definition:
 * get_hv_image_delta()  - get the delta between actual load host virtual address(HVA) and CONFIG_HV_RAM_START
 * get_hv_image_base()   - get the actual Hypervisor load host virtual address(HVA).
 */

/**
 * @brief get the delta between actual load host virtual address(HVA) and CONFIG_HV_RAM_START
 *
 * @return the delta between actual load host virtual address(HVA) and CONFIG_HV_RAM_START.
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
uint64_t get_hv_image_delta(void)
{
	/** Return zero due to hypervisor code is just located in CONFIG_HV_RAM_START without relocation
	 *  in Fusa use scenario */
	return 0UL;
}

/**
 * @brief This function adds the hv delta address with the configured start address to get the actual
 * Hypervisor load host virtual address(HVA).
 *
 * @return the actual Hypervisor load host virtual address(HVA)
 *
 * @pre Paging is enabled and memory region of hypervisor image is 1:1 mapping.
 * @pre The HV code shall only starts from CONFIG_HV_RAM_START(HVA).
 *
 * @post n/a
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
uint64_t get_hv_image_base(void)
{
	/** The calculation method is add the hv delta address with the configed start address.
	 *  Return the actual Hypervisor load host virtual address(HVA).
	 */
	return (get_hv_image_delta() + CONFIG_HV_RAM_START);
}

/**
 * @}
 */
