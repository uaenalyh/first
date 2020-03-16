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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
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
