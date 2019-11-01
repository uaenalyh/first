/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>
#include <misc_cfg.h>
#include <host_pm.h>

/* forward declarations */
struct acrn_vm;

extern struct dmar_info plat_dmar_info;

/* board specific functions */
void create_prelaunched_vm_e820(struct acrn_vm *vm);

#endif /* BOARD_H */
