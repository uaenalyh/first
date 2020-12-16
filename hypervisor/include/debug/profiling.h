/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_H
#define PROFILING_H

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the functions related to profiling operations, including saving the information of the
 * specific virtual CPU, initializing the profiling utility.
 */

#include <vcpu.h>

void profiling_vmenter_handler(struct acrn_vcpu *vcpu);
void profiling_pre_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_post_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_setup(void);

/**
 * @}
 */

#endif /* PROFILING_H */
