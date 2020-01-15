/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_security
 *
 * @{
 */

/**
 * @file
 * @brief this file implements callback function __stack_chk_fail()
 * for stack protector feature of hwmgmt.security module.
 */


/**
 * @brief Callback function for stack protector.
 *
 * GCC will emit extra code to check for buffer overflow in some functions if -fstack-protector-strong/
 * -fstack-protector is enabled when compiling. This function shall be executed if buffer overflow check
 * fails.
 *
 * @return None
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_OPERATIONAL
 *
 * @remark -fstack-protector-strong/-fstack_protector flag shall be enabled when compiling.
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
void __stack_chk_fail(void)
{
	/** Call ASSERT() to print warning message. */
	ASSERT(false, "stack check fails in HV\n");
}

/**
 * @}
 */
