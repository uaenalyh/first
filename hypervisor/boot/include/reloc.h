/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

/**
 * @addtogroup boot
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

extern void relocate(void);
extern uint64_t get_hv_image_delta(void);
extern uint64_t get_hv_image_base(void);

/**
 * @}
 */

#endif /* RELOCATE_H */
