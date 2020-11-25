/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <cpu.h>
#include <msr.h>
#include <cpuid.h>
#include <ucode.h>
#include <guest_memory.h>
#include <irq.h>
#include <logmsg.h>

/**
 * @addtogroup vp-base_vmsr
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the function related to the microcode MSR IA32_BIOS_SIGN_ID.
 *
 * The function defined in this file is supposed to be called by 'rdmsr_vmexit_handler' defined in vmsr.c.
 * It is supposed to be used when hypervisor attempts to handle the VM exit caused by RDMSR instruction executed from
 * guest software and the associated MSR is IA32_BIOS_SIGN_ID.
 *
 */

/**
 * @brief Return the microcode update signature of the physical platform.
 *
 * Return the microcode update signature of the physical platform, which is indicated by the MSR IA32_BIOS_SIGN_ID.
 *
 * It is supposed to be called by 'rdmsr_vmexit_handler' defined in vmsr.c.
 *
 * @return The microcode update signature of the physical platform.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
uint64_t get_microcode_version(void)
{
	/** Declare the following local variables of type uint64_t.
	 *  - val representing the microcode update signature of the physical platform, not initialized. */
	uint64_t val;
	/** Declare the following local variables of type uint32_t.
	 *  - eax representing the contents of EAX register after execution of a CPUID instruction, not initialized.
	 *  - ebx representing the contents of EBX register after execution of a CPUID instruction, not initialized.
	 *  - ecx representing the contents of ECX register after execution of a CPUID instruction, not initialized.
	 *  - edx representing the contents of EDX register after execution of a CPUID instruction, not initialized.
	 */
	uint32_t eax, ebx, ecx, edx;

	/** Call msr_write with the following parameters, in order to write 0H into the native MSR IA32_BIOS_SIGN_ID.
	 *  - MSR_IA32_BIOS_SIGN_ID
	 *  - 0H
	 */
	msr_write(MSR_IA32_BIOS_SIGN_ID, 0U);
	/** Call cpuid with the following parameters, in order to update native MSR IA32_BIOS_SIGN_ID with
	 *  installed microcode revision number, per CPUID, Vol. 2, SDM.
	 *  - CPUID_FEATURES
	 *  - &eax
	 *  - &ebx
	 *  - &ecx
	 *  - &edx
	 */
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	/** Set 'val' to the return value of 'msr_read(MSR_IA32_BIOS_SIGN_ID)', which is the value in the native
	 *  MSR IA32_BIOS_SIGN_ID. */
	val = msr_read(MSR_IA32_BIOS_SIGN_ID);

	/** Return 'val' */
	return val;
}

/**
 * @}
 */
