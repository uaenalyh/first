/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <irq.h>

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file declares a function related to exception information dumping.
 *
 * This file is decomposed into the following functions:
 *
 * - dump_exception(ctx, pcpu_id)  Print the context of an exception. No operation in release version.
 */

/**
 * @brief Print the context of an exception. No operation in release version.
 *
 * @param[in]    ctx     A structure pointer expresses the stack frame layout. Not used in release version.
 * @param[in]    pcpu_id The ID of the physical CPU, specifying where the exception is generated. Not used in release
 *                       version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void dump_exception(__unused struct intr_excp_ctx *ctx, __unused uint16_t pcpu_id)
{
}

/**
 * @}
 */
