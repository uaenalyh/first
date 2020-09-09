/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSIGN_H
#define ASSIGN_H

/**
 * @addtogroup vp-dm_ptirq
 *
 * @{
 */

/**
 * @file
 * @brief This file declares public APIs of vp-dm.ptirq module.
 */

#include <types.h>
#include <ptdev.h>
#include <vm.h>

/**
 * @brief This function is used to build the MSI remapping for a given virtual machine's
 *        passthrough device.
 *
 * @param[in] vm A pointer to virtual machine whose interrupt remapping entry
 *	         is going to be mapped.
 * @param[in] virt_bdf The virtual BDF associated with the specified passthrough device.
 * @param[in] phys_bdf The physical BDF associated with the specified passthrough device.
 * @param[in] entry_nr A parameter indicating the entry ID of the coming vector.
 *                     If entry_nr equals to 0, it means that it is the first vector.
 * @param[inout] info The ptirq_msi_info structure used to build the MSI remapping.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre entry_nr == 0
 * @pre info != NULL
 * @pre (virt_bdf & FFH) < 3FH
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a phys_bdf is different among parallel invocation.
 */
void ptirq_msix_remap(
	struct acrn_vm *vm, uint16_t virt_bdf, uint16_t phys_bdf, uint16_t entry_nr, struct ptirq_msi_info *info);

/**
 * @brief This function is used to remove the MSI interrupt remapping for the virtual
 *        PCI device with \a virt_bdf.
 *
 * @param[in] vm A pointer to virtual machine whose MSI interrupt remapping
 *               is going to be removed.
 * @param[in] virt_bdf The virtual BDF associated with the specified passthrough device.
 * @param[in] vector_count The number of vectors.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vector_count == 1
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t vector_count);

/**
 * @}
 */

#endif /* ASSIGN_H */
