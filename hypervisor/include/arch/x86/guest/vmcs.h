/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS_H_
#define VMCS_H_

#define VM_SUCCESS 0
#define VM_FAIL    -1

#ifndef ASSEMBLER
#include <types.h>
#include <vcpu.h>

#define VMX_VMENTRY_FAIL 0x80000000U

void init_vmcs(struct acrn_vcpu *vcpu);
void switch_vmcs(const struct acrn_vcpu *vcpu);

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);
#endif /* ASSEMBLER */

#endif /* VMCS_H_ */
