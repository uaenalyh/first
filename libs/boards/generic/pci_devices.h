/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define USB_CONTROLLER .pbdf.bits = { .b = 0x00U, .d = 0x03U, .f = 0x00U }, .vbar_base[0] = 0xdf230000UL

#define ETHERNET_CONTROLLER .pbdf.bits = { .b = 0x00U, .d = 0x02U, .f = 0x00U }, .vbar_base[0] = 0xdf200000UL

#endif /* PCI_DEVICES_H_ */
