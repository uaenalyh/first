/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 */

#ifndef ARCH_VIRQ_H
#define ARCH_VIRQ_H

/**
 * @addtogroup vp-base_virq
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the vp-base.virq module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * vp-base.virq module.
 *
 */

#include <acrn_common.h>
#include <util.h>
#include <spinlock.h>

struct acrn_vcpu;

void vcpu_queue_exception(struct acrn_vcpu *vcpu, uint32_t vector_arg, uint32_t err_code_arg);

void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code);

void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code);

void vcpu_inject_ud(struct acrn_vcpu *vcpu);

void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid);

int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu);

/**
 * @}
 */

#endif /* ARCH_VIRQ_H */
