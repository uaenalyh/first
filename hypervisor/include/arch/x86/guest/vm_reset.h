/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_RESET_H_
#define VM_RESET_H_

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the APIs of requesting and handling VM shutdown.
 */

#include <acrn_common.h>
#include <vcpu.h>

void shutdown_vm_from_idle(uint16_t pcpu_id);
void fatal_error_shutdown_vm(struct acrn_vcpu *vcpu);

/**
 * @}
 */

#endif /* VM_RESET_H_ */
