/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCR_H
#define VCR_H

/**
 * @addtogroup vp-base_vcr
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the external APIs of the vp-base.vcr module
 *
 * The guest's control registers (VCRs) are emulated by the hypervisor. This file provides the
 * public API interfaces for VCRs interception configuration, public APIs for getting or
 * setting VCRs inside the hypervisor and VM-Exit entry function caused by set to CR0/CR4.
 *
 */

#include <types.h>

struct acrn_vcpu;

void init_cr0_cr4_host_mask(void);
uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu);
void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val, bool is_init);
void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu);
void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val, bool is_init);
int32_t cr_access_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @}
 */

#endif /* VCR_H */
