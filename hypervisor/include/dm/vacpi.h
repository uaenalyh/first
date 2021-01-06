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
 * @brief This file declares ACPI tables data structure and defines related macro
 *
 * This file defines a minimal set of ACPI tables required to boot pre-launched VM (Linux).
 * The tables are placed in the guest's 'ROM'(reserved RAM) area just below 1MB physical:
 *
 *  Layout
 *  ------
 *   RSDP  ->   0xf2400    (36 bytes fixed)
 *     XSDT  ->   0xf2480    (36 bytes + 8*7 table addrs, 4 used)
 *       MADT  ->   0xf2500  (depends on the number of CPUs)
 */

#include <types.h>
#include "acpi_def.h"
#include <acpi.h>


#define ACPI_BASE 0xf2400U  /**< Pre-defined ACPI base GPA */

#define ACPI_RSDP_ADDR (ACPI_BASE + 0x0U)  /**< Pre-defined ACPI RSDP table base GPA */
#define ACPI_XSDT_ADDR (ACPI_BASE + 0x080U) /**< Pre-defined ACPI XSDT table base GPA */
#define ACPI_MADT_ADDR (ACPI_BASE + 0x100U) /**< Pre-defined ACPI MADT table base GPA */

#define ACPI_OEM_ID               "ACRN  " /**< Pre-defined ACPI OEM ID */
#define ACPI_ASL_COMPILER_ID      "INTL"  /**< Pre-defined ACPI ASL compiler ID */
#define ACPI_ASL_COMPILER_VERSION 0x0U /**< Pre-defined ACPI ASL compiler version */

struct acrn_vm; /**< Declare the VM data structure, which is used in this file */

/**
 * @brief Data structure to represent all ACPI sub tables.
 *
 * The instance of this data structure caches preliminary ACPI tables and sub-tables exposed to the VMs built.
 * It includes RSDP, XSDT, MADT and LAPIC.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_table_info {
	struct acpi_table_rsdp rsdp; /**< data structure of RSDP table  */
	struct acpi_table_xsdt xsdt; /**< data structure of XSDT table  */

	struct {
		struct acpi_table_madt madt; /**< data structure of MADT table  */
		struct acpi_madt_local_apic lapic_array[MAX_PCPU_NUM]; /**< data structure of LAPIC ACPI array */
	} __packed;
};

void build_vacpi(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* VACPI_H */
