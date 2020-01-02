/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <io.h>
#include <msr.h>
#include <apicreg.h>
#include <cpuid.h>
#include <cpu_caps.h>
#include <softirq.h>
#include <trace.h>

/**
 * @defgroup hwmgmt_time hwmgmt.time
 * @ingroup hwmgmt
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define CAL_MS 10U

static uint32_t tsc_khz = 0U;

uint64_t rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32U) | lo;
}

void calibrate_tsc(void)
{
	uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;

	cpuid(0x16U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
	tsc_khz = eax_base_mhz * 1000U;

	printf("%s, tsc_khz=%lu\n", __func__, tsc_khz);
}

uint32_t get_tsc_khz(void)
{
	return tsc_khz;
}

/**
 * Frequency of TSC in KHz (where 1KHz = 1000Hz). Only valid after
 * calibrate_tsc() returns.
 */

uint64_t us_to_ticks(uint32_t us)
{
	return (((uint64_t)us * (uint64_t)tsc_khz) / 1000UL);
}

uint64_t ticks_to_us(uint64_t ticks)
{
	return (ticks * 1000UL) / (uint64_t)tsc_khz;
}

void udelay(uint32_t us)
{
	uint64_t dest_tsc, delta_tsc;

	/* Calculate number of ticks to wait */
	delta_tsc = us_to_ticks(us);
	dest_tsc = rdtsc() + delta_tsc;

	/* Loop until time expired */
	while (rdtsc() < dest_tsc) {
	}
}

/**
 * @}
 */
