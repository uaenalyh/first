/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <init.h>
#include <console.h>
#include <per_cpu.h>
#include <profiling.h>
#include <vtd.h>
#include <shell.h>
#include <vmx.h>
#include <vm.h>
#include <logmsg.h>

/**
 * @defgroup init init
 * @brief This module implements all initialization interfaces for primary and secondary CPUs.
 * These functions are called from assembly part.
 *
 * Usage:
 * - The 'boot' module depends on this module to do initialization stuff on all physical CPUs.
 *
 * Dependency:
 * - This module depends on debug module to initialize following things.
 *   - console
 *   - log
 *   - profiling
 *   - shell
 * - This module depends on hwmgmt.vmx to do following things.
 *   - enable VMX.
 * - This module depends on vp-base.vm to do following things.
 *   - create vm.
 * - This module depends on hwmgmt.cpu to do following things.
 *   - initialize physical cpu.
 *   - get current physical cpu id.
 * - This module depends on vp-base.hv_main to do following things.
 *   - run cpu idle loop.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all the interfaces that will be used at boot up time
 * and it also defines the macro and decomposed functions that are used in this file internally.
 *
 * External functions include: init_primary_pcpu and init_secondary_pcpu.
 *
 * Decomposed functions include: init_debug_pre, init_debug_post, init_guest_mode,
 * and init_primary_pcpu_post.
 *
 * The macro for stack switch is SWITCH_TO.
 *
 */

/**
 * @brief This macro is used to call a function with the provided stack.
 *
 * @param[in] rsp The pointer to the new stack.
 * @param[in] to The pointer to the function.
 *
 * @return None
 *
 * @pre rsp != NULL
 * @pre to != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * It first sets 'rsp' register to the provided stack address then pushes a magical guard number
 * specified by SP_BOTTOM_MAGIC onto the new stack and jumps to the given function finally.
 *
 */
#define SWITCH_TO(rsp, to)                                                \
	{                                                                 \
		asm volatile("movq %0, %%rsp\n"                           \
			     "pushq %1\n"                                 \
			     "jmpq *%2\n"                                 \
			     :                                            \
			     : "r"(rsp), "rm"(SP_BOTTOM_MAGIC), "a"(to)); \
	}

/**
 * @brief This function initializes debug related stuff including console and log
 * before switching to runtime stack.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void init_debug_pre(void)
{
	/** Call console_init to initialize console. */
	console_init();

	/** Call init_logmsg with the following parameters, in order to
	 *  initialize the default log behavior.
	 *  - CONFIG_LOG_DESTINATION
	 */
	init_logmsg(CONFIG_LOG_DESTINATION);
}

/**
 * @brief This function initializes debug related stuff including shell if this
 * is the BP and profiling framework after switching to runtime stack.
 *
 * @param[in] pcpu_id Physical CPU id.

 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void init_debug_post(uint16_t pcpu_id)
{
	/** If pcpu_id is BOOT_CPU_ID */
	if (pcpu_id == BOOT_CPU_ID) {
		/** Call shell_init to initialize shell. */
		shell_init();
	}

	/** Call profiling_setup to initialize profiling. */
	profiling_setup();
}

/**
 * @brief This function enable VMX mode on the specific CPU.
 *
 * @param[in] pcpu_id Physical CPU id.

 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void init_guest_mode(uint16_t pcpu_id)
{
	/** Call vmx_on to let the current processor enter VMX operation. */
	vmx_on();

	/** Call launch_vms with the following parameters, in order to
	 *  initialize and launch vm. It will only launch the VM whose
	 *  BP's ID is equal to the given 'pcpu_id'.
	 *  - pcpu_id
	 */
	launch_vms(pcpu_id);
}

/**
 * @brief This function initializes the BP after switching to the runtime stack.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void init_primary_pcpu_post(void)
{
	/** Call init_debug_pre to initialize early part of debug related stuff. */
	init_debug_pre();

	/** Call init_pcpu_post with the following parameters, in order to
	 *  initialize physical BP.
	 *  - BOOT_CPU_ID
	 */
	init_pcpu_post(BOOT_CPU_ID);

	/** Call init_debug_post with the following parameters, in order to
	 *  initialize the rest of the debug related stuff.
	 *  - BOOT_CPU_ID
	 */
	init_debug_post(BOOT_CPU_ID);

	/** Call init_guest_mode with the following parameters, in order to
	 *  enable VMX on the specific BP.
	 *  - BOOT_CPU_ID
	 */
	init_guest_mode(BOOT_CPU_ID);

	/** Call run_idle_thread to run idle loop on the current physical CPU. */
	run_idle_thread();
}

/**
 * @brief This function initializes the BP.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_primary_pcpu(void)
{
	/** Declare the following local variable of type uint64_t
	 *	- 'rsp'	representing the runtime stack pointer, not initialized.
	 */
	uint64_t rsp;

	/** Call init_pcpu_pre with the following parameters, in order to
	 *  initialize physical cpu before switching to the runtime stack.
	 *  - true
	 */
	init_pcpu_pre(true);

	/** Use get_cpu_var with 'stack' parameter to get current physical CPU's stack
	 *  array and set 'rsp' to the address of the last element in that stack array.
	 */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1U]);
	/** Align the rsp with CPU_STACK_ALIGN */
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	/** Call SWITCH_TO with the following parameters, in order to
	 *  jump to the function 'init_primary_pcpu_post' with the
	 *  new stack specified by 'rsp'
	 *  - rsp
	 *  - init_primary_pcpu_post
	 */
	SWITCH_TO(rsp, init_primary_pcpu_post);
}

/**
 * @brief This function initializes the AP.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_secondary_pcpu(void)
{
	/** Declare the following local variable of type uint16_t
	 *  - 'pcpu_id' representing the current physical CPU id, not initialized.
	 */
	uint16_t pcpu_id;

	/** Call init_pcpu_pre with the following parameters, in order to
	 *  initialize physical cpu before switching to the runtime stack.
	 *  - false
	 */
	init_pcpu_pre(false);

	/** Call get_pcpu_id to get current physical cpu id and store the value to pcpu_id */
	pcpu_id = get_pcpu_id();

	/** Call init_pcpu_post with the following parameters, in order to
	 *  initialize physical cpu after switching to the runtime stack.
	 *  - pcpu_id
	 */
	init_pcpu_post(pcpu_id);

	/** Call init_debug_post with the following parameters, in order to
	 *  initialize debug related stuff after switching to the runtime stack.
	 *  - pcpu_id
	 */
	init_debug_post(pcpu_id);

	/** Call init_guest_mode with the following parameters, in order to
	 *  enable VMX on the specific physical cpu.
	 *  - pcpu_id
	 */
	init_guest_mode(pcpu_id);

	/** Call run_idle_thread to run idle loop on the current physical CPU. */
	run_idle_thread();
}

/**
 * @}
 */
