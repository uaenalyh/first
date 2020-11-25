/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UCODE_H
#define UCODE_H

/**
 * @addtogroup vp-base_vmsr
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the function related to the microcode MSR IA32_BIOS_SIGN_ID.
 */

#include <types.h>

uint64_t get_microcode_version(void);

/**
 * @}
 */

#endif /* UCODE_H */
