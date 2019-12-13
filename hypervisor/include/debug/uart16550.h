/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UART16550_H
#define UART16550_H

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

/* Register / bit definitions for 16c550 uart */

/*enable/disable receive data read request interrupt*/

/* bit definitions for LSR */

void uart16550_init(bool early_boot);

/**
 * @}
 */

#endif /* !UART16550_H */
