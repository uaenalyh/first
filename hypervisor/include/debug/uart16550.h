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
 * @brief This file declares the 16550 UART initialization API that shall be provided by the debug module.
 */

#include <types.h>

void uart16550_init(bool early_boot);

/**
 * @}
 */

#endif /* !UART16550_H */
