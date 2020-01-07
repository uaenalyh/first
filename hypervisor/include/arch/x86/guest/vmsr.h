/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMSR_H_
#define VMSR_H_

#include <vcpu.h>

/**
 * @addtogroup vp-base_vmsr
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

void init_msr_emulation(struct acrn_vcpu *vcpu);
uint32_t vmsr_get_guest_msr_index(uint32_t msr);

/**
 * @}
 */

#endif /* VMSR_H_ */
