/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

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

#include <vm_configurations.h>

#define ACPI_RSDP_CHECKSUM_LENGTH  20U
#define ACPI_RSDP_XCHECKSUM_LENGTH 36U

#define ACPI_OEM_ID_SIZE 6U

#define ACPI_MADT_TYPE_LOCAL_APIC     0U
#define ACPI_MADT_TYPE_LOCAL_APIC_NMI 4U

#define ACPI_SIG_RSDP "RSD PTR " /* Root System Description Ptr */
#define ACPI_SIG_XSDT "XSDT" /* Extended  System Description Table */
#define ACPI_SIG_MADT "APIC" /* Multiple APIC Description Table */

struct acpi_table_header {
	/* ASCII table signature */
	char signature[4];
	/* Length of table in bytes, including this header */
	uint32_t length;
	/* ACPI Specification minor version number */
	uint8_t revision;
	/* To make sum of entire table == 0 */
	uint8_t checksum;
	/* ASCII OEM identification */
	char oem_id[6];
	/* ASCII OEM table identification */
	char oem_table_id[8];
	/* OEM revision number */
	uint32_t oem_revision;
	/* ASCII ASL compiler vendor ID */
	char asl_compiler_id[4];
	/* ASL compiler version */
	uint32_t asl_compiler_revision;
} __packed;

struct acpi_table_rsdp {
	/* ACPI signature, contains "RSD PTR " */
	char signature[8];
	/* ACPI 1.0 checksum */
	uint8_t checksum;
	/* OEM identification */
	char oem_id[ACPI_OEM_ID_SIZE];
	/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	uint8_t revision;
	/* 32-bit physical address of the RSDT */
	uint32_t rsdt_physical_address;
	/* Table length in bytes, including header (ACPI 2.0+) */
	uint32_t length;
	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	uint64_t xsdt_physical_address;
	/* Checksum of entire table (ACPI 2.0+) */
	uint8_t extended_checksum;
	/* Reserved, must be zero */
	uint8_t reserved[3];
} __packed;

struct acpi_table_xsdt {
	/* Common ACPI table header */
	struct acpi_table_header header;
	/* Array of pointers to ACPI tables */
	uint64_t table_offset_entry[1];
} __packed;

struct acpi_table_madt {
	/* Common ACPI table header */
	struct acpi_table_header header;
	/* Physical address of local APIC */
	uint32_t address;
	uint32_t flags;
} __packed;

struct acpi_subtable_header {
	uint8_t type;
	uint8_t length;
} __packed;

struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	/* ACPI processor id */
	uint8_t processor_id;
	/* Processor's local APIC id */
	uint8_t id;
	uint32_t lapic_flags;
} __packed;

struct acpi_madt_local_apic_nmi {
	struct acpi_subtable_header header;
	uint8_t processor_id;
	uint16_t flags;
	uint8_t lint;
} __packed;

struct ioapic_info;
uint16_t parse_madt(uint32_t lapic_id_array[MAX_PCPU_NUM]);
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

/**
 * @}
 */

#endif /* !ACPI_H */
