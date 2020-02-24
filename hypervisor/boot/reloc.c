/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <reloc.h>
#include <ld_sym.h>

/**
 * @defgroup boot boot
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/* get the delta between CONFIG_HV_RAM_START and the actual load address */
uint64_t get_hv_image_delta(void)
{
	return 0UL;
}

/* get the actual Hypervisor load address (HVA) */
uint64_t get_hv_image_base(void)
{
	return (get_hv_image_delta() + CONFIG_HV_RAM_START);
}

/**
 * @}
 */
