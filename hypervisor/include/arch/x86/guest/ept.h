/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EPT_H
#define EPT_H

/**
 * @addtogroup vp-base_guest_mem
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all EPT related external API and helper functions.
 *
 * This file declares all EPT related external API and helper functions. It also defines
 * a macro for invalid host physical address.
 */
#include <types.h>
#include <vm.h>

/**
 * @brief The callback function type for walking through EPT, it will be called on every table entry.
 */
typedef void (*pge_handler)(uint64_t *pgentry, uint64_t size);

/**
 * @brief The invalid host physical memory address.
 */
#define INVALID_HPA (0x1UL << 52U)

void destroy_ept(struct acrn_vm *vm);

void ept_add_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig);

void ept_modify_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa,
					uint64_t size, uint64_t prot_set, uint64_t prot_clr);

void ept_del_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size);

void ept_flush_leaf_page(uint64_t *pge, uint64_t size);

void walk_ept_table(struct acrn_vm *vm, pge_handler cb);

void *get_ept_entry(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* EPT_H */
