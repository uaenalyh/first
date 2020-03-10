/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <console.h>

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the functions related to console operations, including initialization, timer set-up, and
 * task kick-off.
 *
 * This file is decomposed into the following functions:
 *
 * - console_init()         Initialize the hypervisor console which is solely for debugging. No operation in release
 *                          version.
 * - console_setup_timer()  Set up a periodic timer to handle inputs from and outputs to the serial console.
 *                          No operation in release version.
 * - console_kick()         Kick the hypervisor virtual console which is solely for debugging. No operation in release
 *                          version.
 * - shell_init()           Initialize the hypervisor shell which is solely for debugging. No operation in release
 *                          version.
 */

/**
 * @brief Initialize the hypervisor console which is solely for debugging. No operation in release version.
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
void console_init(void)
{
}

/**
 * @brief Set up a periodic timer to handle inputs from and outputs to the serial console. No operation in release
 * version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void console_setup_timer(void)
{
}

/**
 * @brief Kick the hypervisor virtual console which is solely for debugging. No operation in release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void console_kick(void)
{
}

/**
 * @brief Initialize the hypervisor shell which is solely for debugging. No operation in release version.
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
void shell_init(void)
{
}

/**
 * @}
 */
