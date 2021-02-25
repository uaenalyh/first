/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <types.h>
#include <rtl.h>
#include "acpi.h"
#include <pgtable.h>
#include <ioapic.h>
#include <logmsg.h>
#include <acrn_common.h>
#include <util.h>

/**
 * @defgroup hwmgmt_acpi hwmgmt.acpi
 * @ingroup hwmgmt
 * @brief Implementation of APIs setting physical local APIC and physical IOAPIC information.
 *
 * This module implements a API to set local APIC IDs and to return the number of physical local APICs
 * on current platform, and one more API to set phycical IOAPIC information and get the number of physical
 * IOAPICs on current platform.
 *
 * Usage Remark: hwmgmt.cpu module uses current module to set physical local APIC IDs and get the number of
 *		  local APICs on current platform.
 *		  hwmgmt.apic module also uses current module to set physical IOAPIC information and get
 *		  the number of the physical IOAPICs on current platform.
 *
 * Dependency n/a
 *
 * External APIs:
 * - parse_madt()		This function sets physical local APIC IDs and returns the number
 *				of physical local APICs on current platform.
 *				Depends on:
 *				- N/A.
 *
 * - parse_madt_ioapic()	This function sets physical IOAPIC information and returns the number
 *				of the physical IOAPICs on current platform.
 *				Depends on:
 *				- N/A.
 * @{
 */

/**
 * @file
 * @brief This file implements APIs that shall be provided by the hwmgmt.acpi module.
 */


/**
 * @brief This function sets physical local APIC IDs and returns the number
 *	  of physical local APICs on current platform.
 *
 * @param [out] lapic_id_array  Pointer to an array, which stores the IDs of physical local APICs
 *				after exiting of this function.
 *
 * @return Number of physical local APICs on current platform.
 *
 * @retval 4 There are 4 physical local APICs on current platform.
 *
 * @pre lapic_id_array != NULL
 * @pre Size of lapic_id_array shall be 16 bytes at least.
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy unspecified
 *
 * @threadsafety  unspecified
 *
 */
uint16_t parse_madt(uint32_t lapic_id_array[MAX_PCPU_NUM])
{

	/** Set lapic_id_array[0] to 0H */
	lapic_id_array[0] = 0U;
	/** Set lapic_id_array[1] to 2H */
	lapic_id_array[1] = 2U;
	/** Set lapic_id_array[2] to 4H */
	lapic_id_array[2] = 4U;
	/** Set lapic_id_array[3] to 6H */
	lapic_id_array[3] = 6U;

	/** Return 4H, which means the number of physical local APICs on current platform. */
	return 4U;
}

/**
 * @brief This function sets physical IOAPIC information and returns the number
 *	  of IOAPICs on current platform.
 *
 *  This function sets physical IOAPICs information on current platform (IOAPIC ID,
 *  IOAPIC Register address, GSI where this IO-APIC's interrupt input start) and
 *  returns the number of the physical IOAPICs.
 *
 * @param [out] ioapic_id_array  A pointer to an IOAPIC information data structure
 *				 whose information is to be updated.
 *
 *
 * @return Number of physical IOAPICs on current platform.
 *
 * @retval 1 There is 1 physical IOAPIC on current platform.
 *
 * @pre ioapic_id_array != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy unspecified
 *
 * @threadsafety  unspecified
 *
 */
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array)
{

	/** Set ioapic_id_array->id to 2H */
	ioapic_id_array->id = 2U;
	/**
	 * Set ioapic_id_array->addr to FEC00000H,
	 * which is the default IOAPIC MMIO base address according to IOAPIC specification.
	 */
	ioapic_id_array->addr = 0xFEC00000U;
	/** Set ioapic_id_array->gsi_base to 0H */
	ioapic_id_array->gsi_base = 0U;
	/** Set ioapic_id_array->nr_pins to 0H */
	ioapic_id_array->nr_pins = 0U;

	/** Return 1H, which means the number of physical IOAPICs on current platform. */
	return 1U;
}

/**
 * @}
 */
