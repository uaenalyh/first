/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

/**
 * @addtogroup hwmgmt_acpi
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the APIs of the hwmgmt.acpi module.
 */

#include <types.h>
#include <vm_configurations.h>

struct ioapic_info;

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
uint16_t parse_madt(uint32_t lapic_id_array[MAX_PCPU_NUM]);

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
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

/**
 * @}
 */

#endif /* !ACPI_H */
