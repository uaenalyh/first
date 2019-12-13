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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

void __stack_chk_fail(void);

void __stack_chk_fail(void)
{
	ASSERT(false, "stack check fails in HV\n");
}

/**
 * @}
 */
