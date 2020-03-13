/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template header file for nuc7i7dnb platform ACPI info definition
 * works when Kconfig of ENFORCE_VALIDATED_ACPI_INFO is disabled.
 * When ENFORCE_VALIDATED_ACPI_INFO is enabled, we should use
 * ./misc/acrn-config/target/board_parser.py running on target
 * to generate nuc7i7dnb specific acpi info file named as nuc7i7dnb_acpi_info.h
 * and put it in hypervisor/arch/x86/configs/nuc7i7dnb/.
 */
#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

/*
 * BIOS Information
 * Vendor: Intel Corp.
 * Version: DNKBLi7v.86A.0065.2019.0611.1424
 * Release Date: 06/11/2019
 * BIOS Revision: 5.6
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: NUC7i7DNB
 * Version: J83500-204
 */

/* DRHD of DMAR */

/**
 * @addtogroup hwmgmt_vtd
 *
 * @{
 */

/**
 * @file
 * @brief This file defines the macros related to the remapping hardware.
 *
 * It is supposed to be used when hypervisor accesses the remapping hardware.
 */

/**
 * @brief The number of DRHD structures present in the platform.
 */
#define DRHD_COUNT 2U

/**
 * @brief The number of Device Scope Entries supported by DRHD0.
 */
#define DRHD0_DEV_CNT        0x1U
/**
 * @brief The PCI Segment associated with DRHD0.
 */
#define DRHD0_SEGMENT        0x0U
/**
 * @brief Flags associated with DRHD0.
 *
 * 0 indicates that DRHD0 has under its scope only devices
 * in the specified Segment that are explicitly identified through the 'Device Scope' field.
 */
#define DRHD0_FLAGS          0x0U
/**
 * @brief Base address of remapping hardware register-set for DRHD0.
 */
#define DRHD0_REG_BASE       0xFED90000UL
/**
 * @brief A flag to indicate whether DRHD0 is ignored by hypervisor or not.
 *
 * 'true' indicates that DRHD0 is ignored by hypervisor.
 */
#define DRHD0_IGNORE         true
/**
 * @brief The type of the Device Scope Entry 0 under DRHD0.
 *
 * 1 indicates that the device identified by the Path field is a PCI endpoint device.
 */
#define DRHD0_DEVSCOPE0_TYPE 0x1U
/**
 * @brief The enumeration ID associated with the Device Scope Entry 0 under DRHD0.
 */
#define DRHD0_DEVSCOPE0_ID   0x0U
/**
 * @brief The start bus number associated with the Device Scope Entry 0 under DRHD0.
 */
#define DRHD0_DEVSCOPE0_BUS  0x0U
/**
 * @brief The path associated with the Device Scope Entry 0 under DRHD0.
 */
#define DRHD0_DEVSCOPE0_PATH 0x10U


/**
 * @brief The number of Device Scope Entries supported by DRHD1.
 */
#define DRHD1_DEV_CNT        0x2U
/**
 * @brief The PCI Segment associated with DRHD1.
 */
#define DRHD1_SEGMENT        0x0U
/**
 * @brief Flags associated with DRHD1.
 *
 * 1 indicates that DRHD1 has under its scope all PCI
 * compatible devices in the specified Segment, except devices reported under the scope of other
 * remapping hardware units for the same Segment.
 */
#define DRHD1_FLAGS          0x1U
/**
 * @brief Base address of remapping hardware register-set for DRHD1.
 */
#define DRHD1_REG_BASE       0xFED91000UL
/**
 * @brief A flag to indicate whether DRHD1 is ignored by hypervisor or not.
 *
 * 'false' indicates that DRHD1 is not ignored by hypervisor.
 */
#define DRHD1_IGNORE         false
/**
 * @brief The type of the Device Scope Entry 0 under DRHD1.
 *
 * 3 indicates that the device identified by the Path field is an I/O APIC (or I/O SAPIC) device.
 */
#define DRHD1_DEVSCOPE0_TYPE 0x3U
/**
 * @brief The enumeration ID associated with the Device Scope Entry 0 under DRHD1.
 */
#define DRHD1_DEVSCOPE0_ID   0x2U
/**
 * @brief The start bus number associated with the Device Scope Entry 0 under DRHD1.
 */
#define DRHD1_DEVSCOPE0_BUS  0xf0U
/**
 * @brief The path associated with the Device Scope Entry 0 under DRHD1.
 */
#define DRHD1_DEVSCOPE0_PATH 0xf8U
/**
 * @brief The type of the Device Scope Entry 1 under DRHD1.
 *
 * 4 indicates that the device identified by the Path field is an HPET Timer Block capable of generating MSI.
 */
#define DRHD1_DEVSCOPE1_TYPE 0x4U
/**
 * @brief The enumeration ID associated with the Device Scope Entry 1 under DRHD1.
 */
#define DRHD1_DEVSCOPE1_ID   0x0U
/**
 * @brief The start bus number associated with the Device Scope Entry 1 under DRHD1.
 */
#define DRHD1_DEVSCOPE1_BUS  0x0U
/**
 * @brief The path associated with the Device Scope Entry 1 under DRHD1.
 */
#define DRHD1_DEVSCOPE1_PATH 0xf8U

/**
 * @}
 */

#endif /* PLATFORM_ACPI_INFO_H */
