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

/**
 * @brief public APIs for VCRs initialization configuration
 *
 * Initialize the CR0 Guest/Host Masks and CR4 Guest/Host Masks in the current VMCS.
 *
 * @return None
 */
void init_cr0_cr4_host_mask(void);

/**
 * @brief get vcpu CR0 value
 *
 * Get & cache target vCPU's CR0 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 *
 * @return the value of CR0
 */
uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu CR0 value
 *
 * Update target vCPU's CR0 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR0
 * @param[in] is_init indicate if the guest CR0 write is for initializing
 *
 * @return None
 */
void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val, bool is_init);

/**
 * @brief set vcpu CR2 value
 *
 * Update target vCPU's CR2 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR2
 *
 * @return None
 */
void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu CR4 value
 *
 * Get & cache target vCPU's CR4 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 *
 * @return the value of CR4
 */
uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu CR4 value
 *
 * Update target vCPU's CR4 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR4
 * @param[in] is_init indicate if the guest CR4 write is for initializing
 *
 * @return None
 */
void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val, bool is_init);

/**
 * @brief VM-Exit handler for VCRs access
 *
 * The handler for VM-Exit with the reason "control-register accesses".
 * Guest software attempted to write CR0, CR3, CR4, or CR8 using CLTS,
 * LMSW, or MOV CR and the VM-execution control fields indicate that a
 * VM exit should occur. This function shall be called. Currently, it
 * only handle the cases of CR0 and CR4, other cases will be treated
 * as error.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 *
 * @return 0 if no error happened, otherwise return -EINVAL (-22)
 */
int32_t cr_access_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @}
 */

#endif /* VCR_H */
