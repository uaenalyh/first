/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file implements an 16550 UART initialization API that shall be provided by the debug module.
 *
 * This file is decomposed into the following functions:
 *
 * - uart16550_init(early_boot)   Initialize the 16550 UART which is solely for debugging. No operation in release
 *                                version.
 */

/**
 * @brief Initialize the 16550 UART which is solely for debugging. No operation in release version.
 *
 * @param[in]    early_boot Describes whether hypervisor initializes the PCI memory region once before do
 *                          initialization for physical UART. The physical UART of target validation platform is
 *                          accessed by port IO. \a early_boot will be useless in target validation platform.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void uart16550_init(__unused bool early_boot)
{
}

/**
 * @}
 */
