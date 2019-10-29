/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

/* DRHD of DMAR */

#define DRHD_COUNT 2U

#define DRHD0_DEV_CNT        0x1U
#define DRHD0_SEGMENT        0x0U
#define DRHD0_FLAGS          0x0U
#define DRHD0_REG_BASE       0xFED90000UL
#define DRHD0_IGNORE         true
#define DRHD0_DEVSCOPE0_TYPE 0x1U
#define DRHD0_DEVSCOPE0_ID   0x0U
#define DRHD0_DEVSCOPE0_BUS  0x0U
#define DRHD0_DEVSCOPE0_PATH 0x10U

#define DRHD1_DEV_CNT        0x2U
#define DRHD1_SEGMENT        0x0U
#define DRHD1_FLAGS          0x1U
#define DRHD1_REG_BASE       0xFED90000UL
#define DRHD1_IGNORE         false
#define DRHD1_DEVSCOPE0_TYPE 0x3U
#define DRHD1_DEVSCOPE0_ID   0x2U
#define DRHD1_DEVSCOPE0_BUS  0xf0U
#define DRHD1_DEVSCOPE0_PATH 0xf8U
#define DRHD1_DEVSCOPE1_TYPE 0x4U
#define DRHD1_DEVSCOPE1_ID   0x0U
#define DRHD1_DEVSCOPE1_BUS  0x0U
#define DRHD1_DEVSCOPE1_PATH 0xf8U

#endif /* PLATFORM_ACPI_INFO_H */
