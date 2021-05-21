/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TIMER_H
#define TIMER_H

/**
 * @addtogroup hwmgmt_time
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the hwmgmt.time module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * hwmgmt.time module.
 */

#include <types.h>
#include <list.h>


#define CYCLES_PER_MS us_to_ticks(1000U) /**< Pre-defined total ticks per millisecond. */

void udelay(uint32_t us);
uint64_t us_to_ticks(uint32_t us);
uint64_t rdtsc(void);
void calibrate_tsc(void);
uint32_t get_tsc_khz(void);

/**
 * @}
 */

#endif /* TIMER_H */
