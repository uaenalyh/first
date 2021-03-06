/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HV_KCONFIG
#define HV_KCONFIG

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

 /**
 * @file
 * @brief This file defines macros for hypervisor configurations.
 */

#define CONFIG_LOGICAL_PARTITION         1 /**< Set hypervisor to logical partition mode */
#define CONFIG_SCHED_NOOP                1 /**< Set NOOP attribute for CPU scheduling */
#define CONFIG_BOARD                     "nuc7i7dnb" /**< Set string name of board */
#define CONFIG_RELEASE                   1 /**< Set release version when compiling */
#define CONFIG_STACK_SIZE                0x2000U /**< Stack size in bytes */
#define CONFIG_LOG_DESTINATION           7U /**< Bitmap setting for destination of log messages */
#define CONFIG_LOW_RAM_SIZE              0x00010000U /**< Memory size of low memory */
#define CONFIG_HV_RAM_START              0x00400000UL /**< Start memory address of hypervisor */
#define CONFIG_HV_RAM_SIZE               0x0e800000UL /**< Size of the memory allocated to hypervisor */
#define CONFIG_PLATFORM_RAM_SIZE         0x400000000UL /**< Memory size of platform */
#define CONFIG_UOS_RAM_SIZE              0x200000000UL /**< Size of the memory allocated to a User VM */
#define CONFIG_MAX_IOAPIC_NUM            1U /**< Number of physical IOAPICs on current platform */
#define CONFIG_MAX_IOAPIC_LINES          120U /**< Number of input line of IOAPICs */
#define CONFIG_MAX_IR_ENTRIES            256U /**< Maximum number of Interrupt Remapping entries */
#define CONFIG_IOMMU_BUS_NUM             0x100U /**< Maximum PCI bus number supported by IOMMU */
#define CONFIG_MAX_PCI_DEV_NUM           96U /**< Maximum PCI device number supported by hypervisor */
#define CONFIG_UEFI_OS_LOADER_NAME       "\\EFI\\org.clearlinux\\bootloaderx64.efi" /**< Name of bootloader */

/**
 * @}
 */

#endif
