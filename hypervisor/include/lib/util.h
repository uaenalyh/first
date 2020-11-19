/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UTIL_H
#define UTIL_H

/**
 * @addtogroup lib_utils
 *
 * @{
 */

/**
 * @file
 * @brief This file implements utils APIs including arithmetic operation and alignment checking operation.
 *
 * This file is decomposed into the following functions and macros:
 *
 *     INT_DIV_ROUNDUP(x, y)     - Calculate \a x divided by \a y, round up.
 *     max                       - Get the maximum value from the 2 parameters.
 *     STRINGIFY(x)              - Expand to a constant string with \a x being its content.
 *     mem_aligned_check         - Check if a value is aligned to the required boundary.
 */
#include <types.h>

/**
 * @brief Calculate \a x divided by \a y, round up.
 *
 * @param[in]    x The dividend.
 * @param[in]    y The divisor.
 *
 * @return The result of calculation.
 *
 * @pre \a x and \a y shall have the same integer type.
 * @pre \a y != 0.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to a value calculated by dividing z by \a y where z is \a x plus \a y minus 1.
 */
#define INT_DIV_ROUNDUP(x, y) ((((x) + (y)) - 1U) / (y))

/**
 * @brief Get the maximum value from the 2 parameters.
 *
 * @param[in]    x An integer value or an immediate integer
 * @param[in]    y An integer value or an immediate integer
 *
 * @return The maximum value from \a x and \a y.
 *
 * @pre \a x and \a y shall be integers of the same type.
 *
 * @mode HV_INIT
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the value by select the maximum value from \a x and \a y.
 */
#define max(x, y) ((x) < (y)) ? (y) : (x)

/**
 * @brief Expand to a constant string with \a x being its content.
 *
 * @param[in]    x A symbol to be converted to a string
 *
 * @return A constant string with \a x being its content.
 *
 * @mode NOT_APPLICABLE
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define STRINGIFY(x) #x

/**
 * @brief Check if a value is aligned to the required boundary.
 *
 * @param[in]    value The integer value that used to be checked whether is aligned to the required boundary
 * @param[in]	 req_align The integer value that used as boundary to check the value whether is aligned to.
 *
 * @return Whether value is aligned to the required boundary
 *
 * @retval true \a value is \a req_align byte aligned.
 * @retval false \a value is not \a req_align byte aligned.
 *
 * @pre req_align shall be a power of 2.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool mem_aligned_check(uint64_t value, uint64_t req_align)
{
	/** Return whether value is aligned to the required boundary */
	return ((value & (req_align - 1UL)) == 0UL);
}

/**
 * @}
 */

#endif /* UTIL_H */
