/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

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

#define USB_CONTROLLER .pbdf.bits = { .b = 0x00U, .d = 0x14U, .f = 0x00U }, .vbar_base[0] = 0xdf230000UL

#define ETHERNET_CONTROLLER .pbdf.bits = { .b = 0x00U, .d = 0x1fU, .f = 0x06U }, .vbar_base[0] = 0xdf200000UL

/**
 * @}
 */

#endif /* PCI_DEVICES_H_ */
