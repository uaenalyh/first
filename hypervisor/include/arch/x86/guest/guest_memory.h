/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef GUEST_H
#define GUEST_H

/**
 * @addtogroup vp-base_guest_mem
 *
 * @{
 */

/**
 * @file
 * @brief This file declares external APIs for memory operations between HV and VM.
 *
 * The memory operations contain copy from/to VM and address translation from VM to HV.
 */

#ifndef ASSEMBLER

#include <types.h>

struct acrn_vcpu;
struct acrn_vm;

int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);

int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);

void *gpa2hva(struct acrn_vm *vm, uint64_t x);

uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa);

#endif /* !ASSEMBLER */

/**
 * @}
 */

#endif /* GUEST_H*/
