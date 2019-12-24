/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SECURITY_H
#define SECURITY_H

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

#ifndef ASSEMBLER
bool check_cpu_security_cap(void);
void cpu_internal_buffers_clear(void);
bool is_ept_force_4k_ipage(void);

#ifdef STACK_PROTECTOR
struct stack_canary {
	/* Gcc generates extra code, using [fs:40] to access canary */
	uint8_t reserved[40];
	uint64_t canary;
};
void set_fs_base(void);
#endif

#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* SECURITY_H */
