/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRAMPOLINE_H
#define TRAMPOLINE_H

/**
 * @addtogroup boot
 *
 * @{
 */

/**
 * @file
 * @brief Declare trampoline APIs used to initialize the environment before start APs.
 */

#include <types.h>


/**
 * @brief CS segment selector that the trampoline jumps to
 *
 * The trampoline code first conducts a far jump (after disabling external interrupts) with new values to the CS segment
 * selector and instruction pointer register to mitigate non-deterministic initial value in the IP register. This
 * variable holds the new CS segment selector the trampoline code will jump to, and shall be initialized together with
 * trampoline_fixup_ip to point to the physical address of trampoline_fixup_target in the relocated trampoline section
 * before the trampoline code is executed.
 */
extern uint8_t trampoline_fixup_cs;

/**
 * @brief EIP that the trampoline jumps to
 *
 * The trampoline code first conducts a far jump (after disabling external interrupts) with new values to the CS segment
 * selector and instruction pointer register to mitigate non-deterministic initial value in the IP register. This
 * variable holds the new instruction pointer the trampoline code will jump to, and shall be initialized together with
 * trampoline_fixup_cs to point to the physical address of trampoline_fixup_target in the relocated trampoline section
 * before the trampoline code is executed.
 */
extern uint8_t trampoline_fixup_ip;

/**
 * @brief Offset to the code that the trampoline jumps to
 *
 * This assembly label is an offset (based on the address of ld_trampoline_start) to a code snippet that sets an C
 * execution environment on a real-address mode physical processor by switching to 64-bit mode with an appropriate
 * stack, global descriptor table and paging structure.
 *
 * Refer to section 11.1.3.1 in Software Architecture Design Specification for details.
 */
extern uint8_t trampoline_fixup_target;

/**
 * @brief PML4 page of the initial page table
 *
 * This variable holds the host physical address of the PML4 table of the initial paging structure and shall be
 * initialized properly in the relocated trampoline section before the trampoline code is executed.
 *
 * Refer to section 11.1.1.3 in Software Architecture Design Specification for details.
 */
extern uint8_t cpu_boot_page_tables_start;

/**
 * @brief Pointer to the initial page table
 *
 * This variable holds the host physical address of the paging structure that the trampoline code shall use after
 * switching to 64-bit mode and shall be initialized properly in the relocated trampoline section before the trampoline
 * code is executed.
 *
 * Refer to section 11.1.1.2 in Software Architecture Design Specification for details.
 */
extern uint8_t cpu_boot_page_tables_ptr;

/**
 * @brief Limit and base of the initial GDT
 *
 * This variable holds the limit and base (as a host linear address) of the global descriptor table (GDT) that the
 * trampoline code shall use after switching to 64-bit mode and shall be initialized properly in the relocated
 * trampoline section before the trampoline code is executed
 *
 * Refer to section 11.1.1.14 in Software Architecture Design Specification for details.
 */
extern uint8_t trampoline_gdt_ptr;

/**
 * @brief RIP that the trampoline jumps to after entering 64-bit mode
 *
 * The trampoline code conducts a far jump to a 64-bit code segment after entering 64-bit mode. This variable holds the
 * new instruction pointer to the 64-bit code segment the trampoline will jump to, and shall be initialized to the
 * physical address of trampoline_start64 in the relocated trampoline section before the trampoline code is executed.
 *
 * Refer to section 11.1.1.16 in Software Architecture Design Specification for details.
 */
extern uint8_t trampoline_start64_fixup;

/**
 * @brief Host linear address of init_secondary_pcpu()
 *
 * The trampoline code jumps to init_secondary_pcpu() after setting up the C execution environment. This variable holds
 * the host linear address of init_secondary_pcpu(), and shall be initialized properly in the relocated trampoline
 * section.
 *
 * Refer to section 11.1.1.10 in Software Architecture Design Specification for details.
 */
extern uint64_t main_entry[1];

/**
 * @brief Host linear address of the stack used by the trampoline
 *
 * This variable holds the host linear address of the stack that the trampoline code shall use after switching to 64-bit
 * mode and shall be initialized properly in the relocated trampoline section before the trampoline code is executed.
 *
 * Refer to section 11.1.1.11 in Software Architecture Design Specification for details.
 */
extern uint64_t secondary_cpu_stack[1];

/**
 * @}
 */

#endif /* TRAMPOLINE_H */
