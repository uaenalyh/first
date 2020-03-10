/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
 * @brief This file implements trace APIs that shall be provided by the debug module.
 *
 * This file is decomposed into the following functions:
 *
 * - TRACE_2L(evid, e, f)       Trace an event specified by \a evid with 2 64-bit integers as parameters for debugging.
 *                              No operation in release version.
 * - TRACE_4I(evid, a, b, c, d) Trace an event specified by \a evid with 4 32-bit integers as parameters for debugging.
 *                              No operation in release version.
 */

/**
 * @brief Trace an event specified by \a evid with 2 64-bit integers as parameters for debugging. No operation in
 * release version.
 *
 * @param[in]    evid An integer expresses event ID. Not used in release version.
 * @param[in]    e    The first 64-bit data to be traced. Not used in release version.
 * @param[in]    f    The second 64-bit data to be traced. Not used in release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void TRACE_2L(__unused uint32_t evid, __unused uint64_t e, __unused uint64_t f)
{
}

/**
 * @brief Trace an event specified by \a evid with 4 32-bit integers as parameters for debugging. No operation in
 * release version.
 *
 * @param[in]    evid An integer express event ID. Not used in release version.
 * @param[in]    a    The first 32-bit data to be traced. Not used in release version.
 * @param[in]    b    The second 32-bit data to be traced. Not used in release version.
 * @param[in]    c    The third 32-bit data to be traced. Not used in release version.
 * @param[in]    d    The fourth 32-bit data to be traced. Not used in release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void TRACE_4I(
	__unused uint32_t evid, __unused uint32_t a, __unused uint32_t b, __unused uint32_t c, __unused uint32_t d)
{
}

/**
 * @}
 */
