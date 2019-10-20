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

uint64_t get_microcode_version(void)
{
	uint64_t val;
	uint32_t eax, ebx, ecx, edx;

	msr_write(MSR_IA32_BIOS_SIGN_ID, 0U);
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	val = msr_read(MSR_IA32_BIOS_SIGN_ID);

	return val;
}
