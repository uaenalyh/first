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
#include <trace.h>

/**
 * @defgroup hwmgmt_time hwmgmt.time
 * @ingroup hwmgmt
 * @brief  Implementation of all external APIs about time/CPU ticks related operations.
 *
 * This module implements the TSC (Time Stamp Counter) read/calibration, time and ticks conversion, and a delay
 * function. All of these APIs are used within hypervisor, called by other componments.
 *
 * Usage:
 * - 'hwmgmt.cpu' module depends on this module to get TSC used to check boot time for performance; and udelay also
 *    is used in hwmgmt.cpu physical CPU operations.
 * - 'hwmgmt.iommu' module depends on this module to check DMAR operation time usage.
 * - 'vp-base.vmsr' module depends on this module to set guest TSC.
 * - 'vp-base.vperipheral' module depends on this module to do some delay in vRTC.
 *
 * Remark:
 * - Some functions in this module only can be called after calibrate_tsc is called by the module 'hwmgmt.cpu".
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the hwmgmt.time module.
 *
 * External APIs:
 *  - rdtsc()     This function returns current "Time Stamp Counter".
 *  - calibrate_tsc() This function is called when hwmgmt.cpu init; it's used to get TSC frequency (KHz).
 *
 *  - get_tsc_khz()  This function returns current CPU frequency as KHz. It must be called after calibrate_tsc.
 *
 *  - us_to_ticks()  This function converts the unit of a time interval from microseconds to TSC ticks.
 *				     It must be called after calibrate_tsc.
 *
 *  - ticks_to_us()  This function converts the unit of a time interval from TSC ticks to microseconds.
 *				     It must be called after calibrate_tsc.
 *
 *  - udelay()  This function is to delay a specific number of microseconds before return.
 *                   It must be called after calibrate_tsc.
 *
 */


/**
 * @brief Global uint32_t variable used to store TSC frequency.
 *
 * This global variable is set when calibrate_tsc is called by hwmgmt.cpu.
 **/
static uint32_t tsc_khz = 0U;

/**
 * @brief Return the time-stamp counter of the current physical processor.
 *
 * @return the TSC value of current physical processor.
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety true
 */
uint64_t rdtsc(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - lo representing TSC low 32bits value, not initialized.
	 *  - hi representing TSC high 32bits value, not initialized.
	 */
	uint32_t lo, hi;

	/** Execute "rdtsc" in order to get TSC of current physical processor.
	 *  - Input operands: none
	 *  - Output operands: lo, hi
	 *  - Clobbers: rax rdx
	 */
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	/** Return the TSC value */
	return ((uint64_t)hi << 32U) | lo;
}

/**
 * @brief Initialize the frequency of TSC by getting information from CPUID.
 *
 * @return None
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_INIT
 *
 * @remark The frequency of TSC is stored in tsc_khz used by other APIs.
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
void calibrate_tsc(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - eax_base_mhz representing the contents of EAX register, not initialized.
	 *  - ebx_max_mhz representing the contents of EBX register, not initialized.
	 *  - ecx_bus_mhz representing the contents of ECX register, not initialized.
	 *  - edx representing the contents of EDX register, not initialized.
	 */
	uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;

	/** Call cpuid with the following parameters, in order to get TSC frequency.
	 *  -  0x16U, which is the index of processor frequency information leaf
	 *  -  &eax_base_mhz
	 *  -  &ebx_max_mhz
	 *  -  &ecx_bus_mhz
	 *  -  &edx
	 */
	cpuid(0x16U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
	/** Set tsc_khz to eax_base_mhz * 1000 */
	tsc_khz = eax_base_mhz * 1000U;

	/** Logging the following information with a log level of LOG_INFO.
	 *  - name of this function
	 *  - tsc_khz
	 */
	pr_info("%s, tsc_khz=%lu", __func__, tsc_khz);
}

/**
 * @brief Return the frequency of TSC in KHz (where 1KHz = 1000Hz), rounded down.
 *
 * @return The frequency of TSC in KHz
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @remark calibrate_tsc() has been called once on any processor.
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
uint32_t get_tsc_khz(void)
{
	/** Return the frequency of TSC in KHz */
	return tsc_khz;
}


/**
 * @brief Convert the unit of a time interval from microseconds to TSC ticks.
 *
 * @param[in] us the to-be-converted time interval in microseconds
 *
 * @return The converted TSC ticks
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @remark calibrate_tsc() has been called once on any processor. The input argument a 32-bit integer while the returned
 *         value is 64-bit. Due to the difference in bit width, no given value in microseconds can cause overflow of the
 *         result even with a time stamp counter frequency of 5GHz
 *
 * @reentrancy unspecified
 *
 * @threadsafety true
 */
uint64_t us_to_ticks(uint32_t us)
{
	/** Return the converted TSC ticks: us * tsc_khz / 1000. */
	return (((uint64_t)us * (uint64_t)tsc_khz) / 1000UL);
}

/**
 * @brief Delay \a us microseconds before return.
 *
 * @param[in] us: Number of microseconds to delay before returning.
 *
 * @return None
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @remark calibrate_tsc() has been called once on any processor.
 *
 * @reentrancy unspecified
 *
 * @threadsafety true
 */
void udelay(uint32_t us)
{
	/** Declare the following local variables of type uint64_t.
	 *  - dest_tsc representing the target TSC after the delay, no initialized.
	 *  - delta_tsc representing the delta TSC from now to the target, no initialized.
	 */
	uint64_t dest_tsc, delta_tsc;

	/** Set delta_tsc by calling us_to_ticks to calculate number of ticks to wait */
	delta_tsc = us_to_ticks(us);
	/** Set dest_tsc to the sum of the current TSC and delta_tsc */
	dest_tsc = rdtsc() + delta_tsc;

	/** Until time expired: current TSC is less than target TSC */
	while (rdtsc() < dest_tsc) {
	}
}

/**
 * @}
 */
