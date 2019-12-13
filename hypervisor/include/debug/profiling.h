/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

void profiling_vmenter_handler(struct acrn_vcpu *vcpu);
void profiling_pre_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_post_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_setup(void);

/**
 * @}
 */

#endif /* PROFILING_H */
