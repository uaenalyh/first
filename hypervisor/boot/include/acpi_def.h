/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_DEF_H
#define ACPI_DEF_H

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file declares sub ACPI tables data structure and defines related macro
 *
 * This file declares a minimal set of sub ACPI tables required to boot pre-launched VM (Linux).
 *
 */

#include <types.h>
#include <vm_configurations.h>

#define ACPI_RSDP_CHECKSUM_LENGTH  20U /**< Pre-defined length of RSDP table to calculate checksum value */
#define ACPI_RSDP_XCHECKSUM_LENGTH 36U /**< Pre-defined length of RSDP table to calculate extended checksum value */

#define ACPI_OEM_ID_SIZE 6U /**< Pre-defined bytes size of ACPI OEM ID */

#define ACPI_MADT_TYPE_LOCAL_APIC     0U /**< Pre-defined type to indicate ACPI LAPIC table */
#define ACPI_MADT_TYPE_LOCAL_APIC_NMI 4U /**< Pre-defined type to indicate ACPI LAPIC NMI table */

#define ACPI_SIG_RSDP "RSD PTR " /**< Pre-defined signature for RSDP table */
#define ACPI_SIG_XSDT "XSDT" /**< Pre-defined signature for XSDT table */
#define ACPI_SIG_MADT "APIC" /**< Pre-defined signature for MADT table */

/**
 * @brief Data structure to represent ACPI table header info.
 *
 * This data structure represents all the APCI table header info.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_table_header {

	char signature[4]; /**< ASCII signature of this table */
	uint32_t length; /**< Length of table in bytes, including this header */

	uint8_t revision; /**< ACPI Specification minor version number */
	uint8_t checksum; /**< Checksum of entire table == 0 */

	char oem_id[6]; /**< ASCII OEM identification */
	char oem_table_id[8]; /**< ASCII OEM table identification */

	uint32_t oem_revision; /**< OEM revision number */
	char asl_compiler_id[4]; /**< ASCII ASL compiler vendor ID */

	uint32_t asl_compiler_revision; /**< ASL compiler version */
} __packed;

/**
 * @brief Data structure to represent all the info of RSDP table.
 *
 * This data structure represents all the info of sub table RSDP (Root System Description Pointer).
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_table_rsdp {
	char signature[8]; /**< RSDP table signature, contains "RSD PTR" */
	uint8_t checksum; /**< Checksum of this table */
	char oem_id[ACPI_OEM_ID_SIZE]; /**< OEM identification */

	uint8_t revision; /**< Revision number, must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	uint32_t rsdt_physical_address; /**< 32bits GPA of the RSDT */
	uint32_t length; /**< Table length in bytes, including header (ACPI 2.0+) */

	uint64_t xsdt_physical_address; /**< 64bits GPA of the XSDT (ACPI 2.0+) */
	uint8_t extended_checksum; /**< Checksum of entire table (ACPI 2.0+) */
	uint8_t reserved[3]; /**< Reserved, must be zero */
} __packed;

/**
 * @brief Data structure to represent all the info of XSDT table.
 *
 * This data structure represents all the info of sub table XSDT(eXtended System Description Table).
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_table_xsdt {

	struct acpi_table_header header; /**< Common ACPI table header */
	uint64_t table_offset_entry[1]; /**< Array of pointers to ACPI tables */
} __packed;

/**
 * @brief Data structure to represent all the info of MADT table.
 *
 * This data structure represents all the info of MADT (Multiple APIC Description Table).
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_table_madt {

	struct acpi_table_header header; /**< Common ACPI table header */

	uint32_t address; /**< Physical address of local APIC */
	uint32_t flags; /**< Flag of MADT */
} __packed;

/**
 * @brief Data structure to represent a sub table header.
 *
 * This data structure represents a sub table header: type and length.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_subtable_header {
	uint8_t type; /**< Type of this table */
	uint8_t length; /**< Length of this table */
} __packed;

/**
 * @brief Data structure to represent the info of LAPIC table.
 *
 * This data structure represents the info of LAPIC table.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_madt_local_apic {
	struct acpi_subtable_header header; /**< Header of this table */
	uint8_t processor_id; /**< ACPI processor ID, please refer to ACPI spec */

	uint8_t id; /**< Processor's local APIC ID, please refer to ACPI spec */
	uint32_t lapic_flags; /**< Local APIC flags */
} __packed;

/**
 * @brief Data structure to represent the info of LAPIC NMI table.
 *
 * This data structure represents the info of LAPIC NMI table.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acpi_madt_local_apic_nmi {
	struct acpi_subtable_header header; /**< Header of this table */
	uint8_t processor_id; /**< ACPI processor ID, please refer to ACPI spec */
	uint16_t flags; /**< Flags of MPS INTI (Multiprocessor Specification Interrupt Input), refer to ACPI spec */
	uint8_t lint; /**< Local APIC interrupt input LINTn to which NMI is connected */
} __packed;

/**
 * @}
 */

#endif /* !ACPI_DEF_H */
