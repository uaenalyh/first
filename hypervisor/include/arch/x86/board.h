/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief Declaration of the API building e820 table for a VM.
 *
 * This file declares the create_prelaunched_vm_e820() API which builds a virtual e820 table and fills it into the given
 * VM data structure.
 */

#include <types.h>
#include <misc_cfg.h>

/* forward declarations */
struct acrn_vm;

/* board specific functions */
void create_prelaunched_vm_e820(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* BOARD_H */
