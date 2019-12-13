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
#include <vboot.h>
#include "acpi.h"
#include <pgtable.h>
#include <ioapic.h>
#include <logmsg.h>
#include <acrn_common.h>
#include <util.h>

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/* The lapic_id info gotten from madt will be returned in lapic_id_array */
uint16_t parse_madt(uint32_t lapic_id_array[MAX_PCPU_NUM])
{

	lapic_id_array[0] = 0U;
	lapic_id_array[1] = 2U;
	lapic_id_array[2] = 4U;
	lapic_id_array[3] = 6U;

	return 4;
}

uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array)
{

	ioapic_id_array->id = 0U;
	ioapic_id_array->addr = 0xFEC00000U;
	ioapic_id_array->gsi_base = 0U;
	ioapic_id_array->nr_pins = 0U;

	return 1;
}

/**
 * @}
 */
