/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <per_cpu.h>
#include <vacpi.h>
#include <pgtable.h>

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file implements a function to build ACPI tables for a VM.
 *
 * This file defines a local ACPI template, which includes some default information of a ACPI table.
 * The APCI table can be used by the guest Linux. Also this file offers a API:
 *
 * - build_vacpi: called by prepare_vm
 *
 * After the APCI tables built, they will be used as a kind of resource for a VM.
 *
 */

/**  ACPI tables for pre-launched VM */
static struct acpi_table_info acpi_table_template[CONFIG_MAX_VM_NUM] = {
	[0U ... (CONFIG_MAX_VM_NUM - 1U)] = {
		/** Root System Description Pointer  ('RSD PTR') */
		.rsdp = {
			.signature = ACPI_SIG_RSDP, /**< ACPI signature, contains "RSD PTR " */
			.oem_id = ACPI_OEM_ID,  /**<  OEM identification  */
			.revision = 0x2U,  /**< Must be 2 for ACPI 2.0+  */
			.length = ACPI_RSDP_XCHECKSUM_LENGTH, /**< Table length in bytes */
			.xsdt_physical_address = ACPI_XSDT_ADDR, /**< 64-bit physical address of the XSDT */
		},
		/** Extended System Description Table  ('XSDT') */
		.xsdt = {
			/**< Currently XSDT table only pointers to 1 ACPI table entry (MADT) */
			.header.length = sizeof(struct acpi_table_header) + sizeof(uint64_t),
				/**< Length of table in bytes, including this header */
			.header.revision = 0x1U,  /**< ACPI Specification minor version number */
			.header.oem_revision = 0x1U, /**< OEM revision number */
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION, /**< ASL compiler version */
			.header.signature = ACPI_SIG_XSDT, /**< table signature */
			.header.oem_id = ACPI_OEM_ID, /**< OEM identification */
			.header.oem_table_id = "VIRTNUC7", /**< OEM table identification */
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID, /**< ASL compiler vendor ID */

			.table_offset_entry[0] = ACPI_MADT_ADDR, /**< Array of pointers to ACPI tables */
		},
		/** Multipile ACPI Description Table  ('APIC') */
		.madt = {
			.header.revision = 0x4U, /**< ACPI Specification minor version number */
			.header.oem_revision = 0x1U, /**< OEM revision number */
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION, /**< ASL compiler version */
			.header.signature = ACPI_SIG_MADT, /**< table signature */
			.header.oem_id = ACPI_OEM_ID, /**< OEM identification */
			.header.oem_table_id = "VIRTNUC7", /**< OEM table identification */
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID, /**< ASL compiler vendor ID */

			.address = 0xFEE00000U, /**< Local APIC Address */
			.flags = 0x0U, /**<  PC-AT Compatibility=1 */
		},
		/** LAPIC array: one physical CPU matches one LAPIC */
		.lapic_array = {
			[0U ... (MAX_PCPU_NUM - 1U)] = {
				.header.type = ACPI_MADT_TYPE_LOCAL_APIC, /**< Sub table type */
				.header.length = sizeof(struct acpi_madt_local_apic), /**< Length of table */
				.lapic_flags = 0x1U, /**< LAPIC flag: Processor Enabled=1 */
			}
		},
	}
};

/**
 * @brief Calculate the checksum value of the bytes in a buffer data.
 *
 * This function is called to calculate checksum value of a buffer data.
 *
 * @param[in] buf A pointer to a data buffer
 * @param[in] length The length of the data buffer
 *
 * @return the checksum value
 *
 * @pre buf != NULL
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark It is called by build_vacpi.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint8_t calculate_checksum8(const void *buf, uint32_t length)
{
	/** Declare the following local variable of type uint32_t.
	 *  - i representing the loop counter. not initialized.
	 */
	uint32_t i;

	/** Declare the following local variable of type uint8_t.
	 *  - sum representing the sum of bytes in the given buffer, initialized as 0.
	 */
	uint8_t sum = 0U;

	/** For each i ranging from 0 to (length - 1) [with a step of 1] */
	for (i = 0U; i < length; i++) {
		/** Increment 'sum' by the (i+1)-th byte in buf. */
		sum += *((const uint8_t *)buf + i);
	}

	/** Return the value that can be added to 'sum' (as a byte) to get a result of 0. */
	return (uint8_t)(0x100U - sum);
}

/**
 * @brief Build ACPI tables for the given VM.
 *
 * This function is called to build ACPI tables for the given VM. It uses the local ACPI template as a base to generate
 * the tables and copy them to the VM's guest memory (fixed address), which will be used when the VM booting up.
 *
 * @param[in] vm Pointer to a VM data structure which the virtual ACPI tables will be copied to.
 *
 * @return N/A
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is called by prepare_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void build_vacpi(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct acpi_table_rsdp *'.
	 *  - rsdp representing a pointer to RSDP table, not initialized.
	 */
	struct acpi_table_rsdp *rsdp;
	/** Declare the following local variables of type 'struct acpi_table_xsdt *'.
	 *  - xsdt representing a pointer to XSDT table, not initialized.
	 */
	struct acpi_table_xsdt *xsdt;
	/** Declare the following local variables of type 'struct acpi_table_madt *'.
	 *  - madt representing a pointer to MADT table, not initialized.
	 */
	struct acpi_table_madt *madt;
	/** Declare the following local variables of type 'struct acpi_madt_local_apic *'.
	 *  - lapic representing a pointer to LAPIC table, not initialized.
	 */
	struct acpi_madt_local_apic *lapic;
	/** Declare the following local variables of type uint16_t.
	 *  - i representing a index used in a loop, not initialized.
	 */
	uint16_t i;

	/** Set rsdp to &acpi_table_template[vm->vm_id].rsdp */
	rsdp = &acpi_table_template[vm->vm_id].rsdp;
	/** Set rsdp->checksum to the return value of calculate_checksum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH) */
	rsdp->checksum = calculate_checksum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
	/** Set rsdp->extended_checksum to the return value of calculate_checksum8(rsdp, ACPI_RSDP_XCHECKSUM_LENGTH) */
	rsdp->extended_checksum = calculate_checksum8(rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
	/** Call copy_to_gpa with the following parameters, in order to copy the RSDP table to guest fixed memory
	 *  space which is for ACPI table.
	 *  - vm
	 *  - rsdp
	 *  - ACPI_RSDP_ADDR
	 *  - ACPI_RSDP_XCHECKSUM_LENGTH
	 */
	copy_to_gpa(vm, rsdp, ACPI_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);

	/** Set xsdt to &acpi_table_template[vm->vm_id].xsdt */
	xsdt = &acpi_table_template[vm->vm_id].xsdt;
	/** Set xsdt->header.checksum to the return value of calculate_checksum8(xsdt, xsdt->header.length) */
	xsdt->header.checksum = calculate_checksum8(xsdt, xsdt->header.length);
	/** Call copy_to_gpa with the following parameters, in order to copy the XSDT table to guest fixed memory
	 *  space which is for ACPI table.
	 *  - vm
	 *  - xsdt
	 *  - ACPI_XSDT_ADDR
	 *  - xsdt->header.length
	 */
	copy_to_gpa(vm, xsdt, ACPI_XSDT_ADDR, xsdt->header.length);

	/** For each i ranging from 0 to vm->hw.created_vcpus - 1 [with a step of 1] */
	for (i = 0U; i < vm->hw.created_vcpus; i++) {
		/** Set lapic to &acpi_table_template[vm->vm_id].lapic_array[i] */
		lapic = &acpi_table_template[vm->vm_id].lapic_array[i];
		/** Set lapic->processor_id to i, to fix up MADT LAPIC subtables */
		lapic->processor_id = (uint8_t)i;
		/** Set lapic->id to i, to fix up MADT LAPIC subtables */
		lapic->id = (uint8_t)i;
	}

	/** Set madt to &acpi_table_template[vm->vm_id].madt */
	madt = &acpi_table_template[vm->vm_id].madt;
	/** Set madt->header.length to the total MADT table length */
	madt->header.length = sizeof(struct acpi_table_madt) + (sizeof(struct acpi_madt_local_apic) *
		(size_t)vm->hw.created_vcpus);
	/** Set madt->header.checksum to the return value of calculate_checksum8(madt, madt->header.length) */
	madt->header.checksum = calculate_checksum8(madt, madt->header.length);

	/** Call copy_to_gpa with the following parameters, in order to copy MADT table/its sub tables to
	 *  guest fixed memory space which is for ACPI table.
	 *  - vm
	 *  - madt
	 *  - ACPI_MADT_ADDR
	 *  - madt->header.length
	 */
	copy_to_gpa(vm, madt, ACPI_MADT_ADDR, madt->header.length);
}

/**
 * @}
 */
