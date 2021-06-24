/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <logmsg.h>

/**
 * @defgroup lib lib
 * @brief Implementation of lib APIS to do do string operation, memory operation, bitmap operation and lock operation.
 *
 * This module is decomposed into the following sub-component:
 *
 * - lib.public: Definition of public values of data structures.
 * - lib.bits: Implementation of bit APIs to do bitmap operating.
 * - lib.lock: Implementation of lock APIs to do lock operating.
 * - lib.util: Implementation of utility APIs to do string operating and memory operation.
 *
 * Refer to section 10.1 of Software Architecture Design Specification for the detailed decomposition of this component
 * and section 11.2 for the external APIs of each module inside this component.
 */

/**
 * @addtogroup lib_util
 *
 * @{
 */

/**
 * @file
 * @brief This file implements string operation APIs that shall be provided by the lib.util.
 *
 * This file is decomposed into the following functions:
 *
 * - strncmp: Compare at most n_arg characters between two string operands.
 * - strnlen_s: Calculate the length of the string str_arg, excluding the terminating null byte.
 */

/**
 * @brief Calculate the length of the string str_arg, excluding the terminating null byte.
 *
 * @param[in] str_arg Pointer to the string whose length is calculated
 * @param[in] maxlen_arg The maximum number of characters to examine
 *
 * @return String length, excluding the NULL character.
 *
 * @retval 0 if \a str_arg is NULL
 *
 * @pre [\a str_arg , \a str_arg + \a maxlen_arg ) maps to memory with read privilege.
 * @pre \a str_arg != NULL
 *
 * @mode HV_INIT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
size_t strnlen_s(const char *str_arg, size_t maxlen_arg)
{
	/** Declare the following local variables of type const char *.
	 *  - str representing the address of the character to be checked, initialized as str_arg. */
	const char *str = str_arg;
	/** Declare the following local variables of type size_t.
	 *  - count representing the count of ASCII characters encountered, initialized as 0. */
	size_t count = 0U;

	/** Declare the following local variables of type size_t.
	 *  - maxlen representing the number of remaining characters to examine, initialized as maxlen_arg. */
	size_t maxlen = maxlen_arg;
	/** While *str is not a string terminator */
	while ((*str) != '\0') {
		/** If the maxlen is equal to 0 */
		if (maxlen == 0U) {
			/** Terminate the loop */
			break;
		}

		/** Increment count by 1 */
		count++;
		/** Decrement maxlen by 1 */
		maxlen--;
		/** Increment str by 1 */
		str++;
	}

	/** Return the count of ASCII characters encountered */
	return count;
}

/**
 * @brief Compare at most n_arg characters between two string operands.
 *
 * The string are compared in ASCII sort order.
 *
 * @param[in]    s1_arg The string to compare with.
 * @param[in]    s2_arg The string to compare with.
 * @param[in]    n_arg The maximum number of characters to be compared
 *
 * @return An integer less than, equal to, or greater than zero if
 * \a s1_arg is found, respectively, to be less than, to match, or be greater
 * than \a s2.
 *
 * @retval <0 \a s1_arg is less than \a s2_arg
 * @retval >0 \a s1_arg is greater than \a s2_arg
 * @retval 0 \a s1_arg is same with \a s2_arg
 *
 * @pre s1_arg != NULL
 * @pre s2_arg != NULL
 * @pre n_arg > 0
 * @pre The host logical address range [\a s1_arg , \a s1_arg + \a n_arg ) maps to memory with read privilege.
 * @pre The host logical address range [\a s2_arg , \a s2_arg + \a n_arg ) maps to memory with read privilege.
 *
 * @mode HV_INIT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
int32_t strncmp(const char *s1_arg, const char *s2_arg, size_t n_arg)
{
	/** Declare the following local variables of type const char *.
	 *  - str1 representing the address of the character to be compared, initialized as s1_arg. */
	const char *str1 = s1_arg;
	/** Declare the following local variables of type const char *.
	 *  - str2 representing the address of the character to be compared, initialized as s2_arg. */
	const char *str2 = s2_arg;
	/** Declare the following local variables of type size_t.
	 *  - n representing the number of remaining characters to be compared, initialized as n_arg. */
	size_t n = n_arg;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the result of string comparison operation, initialized as 0. */
	int32_t ret = 0;

	/** While n is not equal to 1 and
	 * *str1 is not a string terminator and
	 * *str2 is not a string terminator and
	 * the ASCII character resident in str1 is same with the ASCII character resident in str2  */
	while (((n - 1U) != 0U) && ((*str1) != '\0') && ((*str2) != '\0') && ((*str1) == (*str2))) {
		/** Increment str1 by 1 */
		str1++;
		/** Increment str2 by 1 */
		str2++;
		/** Decrement n by 1 */
		n--;
	}
	/** Set ret to *str1 - *str2 */
	ret = (int32_t)(*str1 - *str2);

	/** Return the result of string comparison operation */
	return ret;
}

/**
 * @}
 */
