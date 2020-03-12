/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       vacpi.h
 *
 *   DESCRIPTION
 *
 *       This file defines API and extern variable for virtual ACPI
 *
 ************************************************************************/
/**********************************/
/* EXTERNAL VARIABLES	     */
/**********************************/
#ifndef VACPI_H
#define VACPI_H

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#include "acpi_def.h"

/*
 *
 * Create the minimal set of ACPI tables required to boot pre-launched VM
 *
 *  The tables are placed in the guest's ROM area just below 1MB physical,
 * above the MPTable.
 *
 *  Layout
 *  ------
 *   RSDP  ->   0xf2400    (36 bytes fixed)
 *     XSDT  ->   0xf2480    (36 bytes + 8*7 table addrs, 4 used)
 *       MADT  ->   0xf2500  (depends on #CPUs)
 */
#define ACPI_BASE 0xf2400U

#define ACPI_RSDP_ADDR (ACPI_BASE + 0x0U)
#define ACPI_XSDT_ADDR (ACPI_BASE + 0x080U)
#define ACPI_MADT_ADDR (ACPI_BASE + 0x100U)

#define ACPI_OEM_ID               "ACRN  "
#define ACPI_ASL_COMPILER_ID      "INTL"
#define ACPI_ASL_COMPILER_VERSION 0x20190802U

struct acrn_vm;
struct acpi_table_info {
	struct acpi_table_rsdp rsdp;
	struct acpi_table_xsdt xsdt;

	struct {
		struct acpi_table_madt madt;
		struct acpi_madt_local_apic_nmi lapic_nmi;
		struct acpi_madt_local_apic lapic_array[MAX_PCPU_NUM];
	} __packed;
};

void build_vacpi(struct acrn_vm *vm);

/**
 * @pre buf != NULL
 */
static inline uint8_t calculate_sum8(const void *buf, uint32_t length)
{
	uint32_t i;
	uint8_t sum = 0U;

	for (i = 0U; i < length; i++) {
		sum += *((const uint8_t *)buf + i);
	}

	return sum;
}

/**
 * @pre buf != NULL
 */
static inline uint8_t calculate_checksum8(const void *buf, uint32_t len)
{
	return (uint8_t)(0x100U - calculate_sum8(buf, len));
}
/**
 * @}
 */

#endif /* VACPI_H */
