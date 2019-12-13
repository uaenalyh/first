/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef DUMP_H
#define DUMP_H

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

struct intr_excp_ctx;

void dump_exception(struct intr_excp_ctx *ctx, uint16_t pcpu_id);

/**
 * @}
 */

#endif /* DUMP_H */
