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

/**
 * @brief Check the security system software interfaces for underlying platform
 *
 * Check if system software interfaces are supported by physical platfrom for
 * known CPU vulnerabilities mitigation.
 *
 * @return A Boolean value indicating whether software interfaces are sufficient to mitigate
 * known CPU vulnerabilities
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
bool check_cpu_security_cap(void);

/**
 * @brief Flush L1 data cache if it is required on VM entry.
 *
 * This function flushes L1 data cache if such flush is required on VM entry(current processor is
 * potentially affected by L1TF CPU vulnerability),and flushing L1 data cache also clears CPU
 * internal buffers if this processor is affected by MDS vulnerability.
 *
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety yes
 */
void cpu_l1d_flush(void);

/**
 * @brief Clear CPU internal buffers.
 *
 *  This function clears CPU internal buffers if it is required, by calling
 *  verw_buffer_overwriting().
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
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

/**
 * @brief Initialize the per-CPU stack canary structure for the current physical CPU
 *
 * Initialize the per-CPU stack canary structure for the current physical CPU by
 * assigning a random value to the canary. Also set the IA32_FS_BASE MSR of the current
 * physical CPU to the base address of the per-CPU stack canary structure for the
 * current physical CPU.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_INIT
 *
 * @reentrancy unspecified
 * @threadsafety unspecified
 */
void set_fs_base(void);
#endif

#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* SECURITY_H */
