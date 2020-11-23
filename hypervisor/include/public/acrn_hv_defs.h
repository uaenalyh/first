/*
 * hypercall definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACRN_HV_DEFS_H
#define ACRN_HV_DEFS_H

/**
 * @addtogroup lib_public
 *
 * @{
 */

/**
 * @file
 * @brief Definition of page table operation types and invalid addresses
 *
 * This file defines the values (in the form of object-like macros) indicating page table operation types and invalid
 * addresses.
 */

#define ACRN_INVALID_HPA (~0UL)  /**< The value indicating an invalid host physical address */

#define MR_DEL    2U  /**< The value indicating the deletion of the mapping of a memory region */
#define MR_MODIFY 3U  /**< The value indicating the modification of mapping attributes of a memory region */

/**
 * @}
 */

#endif /* ACRN_HV_DEFS_H */
