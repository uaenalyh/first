/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS_H_
#define VMCS_H_

/**
 * @addtogroup vp-base_hv_main
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the relevant macros, internal and external APIs
 *  for VMCS operation.
 *
 */

#define VM_SUCCESS 0    /**< A pre-defined value to indicate the success of VMX instruction executions. */
#define VM_FAIL    -1   /**< A pre-defined value to indicate the fail of VMX instruction executions. */

#ifndef ASSEMBLER
#include <types.h>
#include <vcpu.h>


#define VMX_VMENTRY_FAIL 0x80000000U  /**< Indicating that the VM exit is caused by VM-entry failure. */

void init_vmcs(struct acrn_vcpu *vcpu);
void load_vmcs(const struct acrn_vcpu *vcpu);

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);
#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* VMCS_H_ */
