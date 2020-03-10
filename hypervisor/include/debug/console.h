/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONSOLE_H
#define CONSOLE_H

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the functions related to console operations, including initialization, timer set-up, and
 * task kick-off.
 */

/** Initializes the console module.
 *
 */
void console_init(void);

void console_setup_timer(void);

void console_kick(void);

/**
 * @}
 */

#endif /* CONSOLE_H */
