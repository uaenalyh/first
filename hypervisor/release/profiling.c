/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file implements profiling APIs that shall be provided by the debug module.
 *
 * This file is decomposed into the following functions:
 *
 * - profiling_vmenter_handler(vcpu)     Save the information of \a vcpu before a VM entry for debugging and
 *                                       performance profiling. No operation in release version.
 * - profiling_pre_vmexit_handler(vcpu)  Save the information of \a vcpu before a VM exit handler is invoked for
 *                                       debugging and performance profiling. No operation in release version.
 * - profiling_post_vmexit_handler(vcpu) Save the information of \a vcpu after a VM exit handler is invoked for
 *                                       debugging and performance profiling. No operation in release version.
 * - profiling_setup()                   Initialize the profiling utility. No operation in release version.
 *
 */

/**
 * @brief Save the information of \a vcpu before a VM entry for debugging and performance profiling. No operation in
 * release version.
 *
 * @param[in]    vcpu A pointer to the specified vCPU whose information to be logged. Not used in release version.
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
void profiling_vmenter_handler(__unused struct acrn_vcpu *vcpu)
{
}
/**
 * @brief Save the information of \a vcpu before a VM exit handler is invoked for debugging and performance profiling.
 * No operation in release version.
 *
 * @param[in]    vcpu A pointer to the specified vCPU whose information to be logged. Not used in release version.
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
void profiling_pre_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
}
/**
 * @brief Save the information of \a vcpu after a VM exit handler is invoked for debugging and performance profiling.
 * No operation in release version.
 *
 * @param[in]    vcpu A pointer to the specified vCPU whose information to be logged. Not used in release version.
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
void profiling_post_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
}
/**
 * @brief Initializing the profiling utility. No operation in release version.
 *
 * @mode HV_INIT
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void profiling_setup(void)
{
}

/**
 * @}
 */
