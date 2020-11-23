/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBOOT_INFO_H
#define VBOOT_INFO_H

/**
 * @addtogroup vp-base_vboot
 *
 * @{
 */

/**
 * @file
 * @brief Declaration of the external API provided by the vp-base.vboot module.
 *
 * This file declares the init_vm_boot_info() which is the only external API provided by this module.
 */

#include <vm.h>

void init_vm_boot_info(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* end of include guard: VBOOT_INFO_H */
