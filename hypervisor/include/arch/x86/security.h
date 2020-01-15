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
 * @brief this file declares the external data structures and APIs of the hwmgmt.security module.
 */

#ifndef ASSEMBLER
bool check_cpu_security_cap(void);
void cpu_internal_buffers_clear(void);
bool is_ept_force_4k_ipage(void);

#ifdef STACK_PROTECTOR
/**
 * @brief Structure to define stack proctor canary type.
 *
 * GCC compiler will generate extra code and use IA32_FS_BASE+28H to access canary data.
 * The base address of instance with this type will be set to IA32_FS_BASE MSR.
 *
 * @consistency n/a
 *
 * @alignment 8
 *
 * @remark Canary should be initialized with a random value.
 */
struct stack_canary {
	uint8_t reserved[40]; /**< Reserved. */
	uint64_t canary; /**< Canary variable. */
};
void set_fs_base(void);
#endif

#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* SECURITY_H */
