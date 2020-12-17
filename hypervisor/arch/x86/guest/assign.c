/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vm.h>
#include <vtd.h>
#include <ptdev.h>
#include <per_cpu.h>
#include <ioapic.h>

/**
 * @defgroup vp-dm_ptirq vp-dm.ptirq
 * @ingroup vp-dm
 * @brief The definition and implementation of passthrough device interrupt remapping related stuff.
 *
 * Usage:
 * - 'vp-dm.vperipheral' depends on this module to build and remove passthrough device interrupt remapping.
 *
 * Dependency:
 * - This module depends on vp-base.vlapic module to calculate the destination vlapic when hypervisor
 *   attempts to deliver a specified MSI interrupt.
 * - This module depends on lib.bits module for bits operations.
 * - This module depends on vp-base.vcpu module to get target pCPUs based on the corresponding vCPUs.
 * - This module depends on hwmgmt.vtd module to configure interrupt remapping.
 *
 * @{
 */

/**
 * @file
 * @brief The definition and implementation of public APIs for passthrough device interrupt remapping.
 *
 * It provides external APIs for building and removing passthrough device interrupt remapping as well as
 * private helper functions to implement those public APIs.
 *
 * This file implements all following external APIs that shall be provided by the vp-dm.ptirq module.
 * - ptirq_msix_remap
 * - ptirq_remove_msix_remapping
 *
 * This file also implements following helper functions to help realize the features of the external APIs.
 * - calculate_logical_dest_mask
 * - ptirq_build_physical_msi
 * - remove_msix_remapping
 */

/**
 * @brief This function calculates the mask used to specify the destination processors
 *        in the logical destination mode.
 *
 * @param[in] pdmask The mask of a MSI request's target pCPUs.
 *
 * @return The mask used to specify the destination processors in the logical destination mode
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static uint32_t calculate_logical_dest_mask(uint64_t pdmask)
{
	/** Declare the following local variable of type uint32_t
	 *  - dest_mask representing the mask of the interrupt destination processors in the
	 *    logical destination mode, initialized as 0H.
	 */
	uint32_t dest_mask = 0UL;
	/** Declare the following local variable of type uint64_t
	 *  - pcpu_mask representing a temporary pCPU mask variable used in dest_mask calculation,
	 *    initialized as pdmask.
	 */
	uint64_t pcpu_mask = pdmask;
	/** Declare the following local variable of type uint16_t
	 *  - pcpu_id representing a temporary pCPU ID variable used in dest_mask calculation,
	 *    not initialized.
	 */
	uint16_t pcpu_id;

	/** Call ffs64 with the following parameters, in order to
	 *  get the CPU ID of the first marked pCPU in pcpu_mask and assign it to
	 *  pcpu_id.
	 *  - pcpu_mask
	 */
	pcpu_id = ffs64(pcpu_mask);
	/** While pcpu_id is less than MAX_PCPU_NUM */
	while (pcpu_id < MAX_PCPU_NUM) {
		/** Call bitmap_clear_nolock with the following parameters, in order to
		 *  clear the corresponding bit of pcpu_id in pcpu_mask.
		 *  - pcpu_id
		 *  - &pcpu_mask
		 */
		bitmap_clear_nolock(pcpu_id, &pcpu_mask);
		/** Set dest_mask to be (dest_mask | per_cpu(lapic_ldr, pcpu_id)),
		 *  where per_cpu(lapic_ldr, pcpu_id) is the content in the LDR register
		 *  corresponding to the pCPU specified by pcpu_id, in order to
		 *  calculate the mask used to specify the destination processors in
		 *  the logical destination mode.
		 */
		dest_mask |= per_cpu(lapic_ldr, pcpu_id);
		/** Call ffs64 with the following parameters, in order to
		 *  get the CPU ID of the first marked pCPU in pcpu_mask and assign it to
		 *  pcpu_id.
		 *  - pcpu_mask
		 */
		pcpu_id = ffs64(pcpu_mask);
	}

	/** Return dest_mask */
	return dest_mask;
}

/**
 * @brief This function is for building the physical MSI remapping for a given virtual machine's
 *        passthrough device.
 *
 * @param[in] vm A pointer to the virtual machine whose interrupt remapping is to be built.
 * @param[inout] info A pointer to the ptirq_msi_info structure used for MSI remapping.
 * @param[in] virt_bdf The virtual BDF associated with the specified passthrough device.
 * @param[in] phys_bdf The physical BDF associated with the specified passthrough device.
 * @param[in] vector The interrupt vector.
 *
 * @return None
 *
 * @pre vm != NULL
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
static void ptirq_build_physical_msi(
	struct acrn_vm *vm, struct ptirq_msi_info *info, uint16_t virt_bdf, uint16_t phys_bdf, uint32_t vector)
{
	/** Declare the following local variable of type uint64_t
	 *  - vdmask representing the mask of the MSI target vCPU, not initialized.
	 *  - pdmask representing the mask of the MSI target pCPU, not initialized.
	 */
	uint64_t vdmask, pdmask;
	/** Declare the following local variable of type uint32_t
	 *  - dest representing the MSI destination in virtual MSI address, not initialized.
	 *  - delmode representing the MSI delivery mode in virtual MSI data, not initialized.
	 *  - dest_mask representing the MSI destination in physical MSI address, not initialized.
	 */
	uint32_t dest, delmode, dest_mask;
	/** Declare the following local variable of type bool
	 *  - phys representing the destination mode is physical or not, not initialized */
	bool phys;
	/** Declare the following local variable of type 'union dmar_ir_entry'
	 *  - irte representing the interrupt remapping entry to be configured, not initialized */
	union dmar_ir_entry irte;
	/** Declare the following local variable of type 'union irte_index'
	 *  - ir_index representing the interrupt remapping index to be configured, not initialized */
	union irte_index ir_index;
	/** Declare the following local variable of type 'struct intr_source'
	 *  - intr_src representing the interrupt source, not initialized */
	struct intr_source intr_src;
	/** Declare the following local variable of type uint16_t
	 *  - index representing the interrupt remapping index to be configured, not initialized */
	uint16_t index;

	/** Set dest to be info->vmsi_addr.bits.dest_field */
	dest = info->vmsi_addr.bits.dest_field;
	/** Set phys to be true if info->vmsi_addr.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS,
	 *  otherwise set phys to be false */
	phys = (info->vmsi_addr.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS);

	/** Call vlapic_calc_dest with the following parameters, in order to
	 *  populates the vdmask with the set of vCPUs that match the addressing specified
	 *  by the (dest, phys, false) tuple.
	 *  - vm
	 *  - &vdmask
	 *  - false
	 *  - dest
	 *  - phys
	 *  - false
	 */
	vlapic_calc_dest(vm, &vdmask, false, dest, phys, false);
	/** Call vcpumask2pcpumask with the following parameters, in order to
	 *  calculate the mask of the MSI target pCPUs from the mask of the MSI target vCPUs
	 *  and assign the return value to pdmask.
	 *  - vm
	 *  - vdmask
	 */
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/** Set delmode to be info->vmsi_data.bits.delivery_mode */
	delmode = info->vmsi_data.bits.delivery_mode;
	/** If delmode does not equal to MSI_DATA_DELMODE_FIXED and MSI_DATA_DELMODE_LOPRI */
	if ((delmode != MSI_DATA_DELMODE_FIXED) && (delmode != MSI_DATA_DELMODE_LOPRI)) {
		/** Set delmode to be MSI_DATA_DELMODE_LOPRI */
		delmode = MSI_DATA_DELMODE_LOPRI;
	}

	/** Call calculate_logical_dest_mask with the following parameters, in order to
	 *  calculate the mask of the destination processors in the logical destination mode
	 *  from the mask of the MSI target pCPUs and assign it to dest_mask.
	 *  - pdmask
	 */
	dest_mask = calculate_logical_dest_mask(pdmask);

	/** Set irte.entry.lo_64 to be 0H */
	irte.entry.lo_64 = 0UL;
	/** Set irte.entry.hi_64 to be 0H */
	irte.entry.hi_64 = 0UL;
	/** Set irte.bits.vector to be vector */
	irte.bits.vector = vector;
	/** Set irte.bits.delivery_mode to be delmode */
	irte.bits.delivery_mode = delmode;
	/** Set irte.bits.dest_mode to be MSI_ADDR_DESTMODE_LOGICAL */
	irte.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	/** Set irte.bits.rh to be MSI_ADDR_RH */
	irte.bits.rh = MSI_ADDR_RH;
	/** Set irte.bits.dest to be dest_mask */
	irte.bits.dest = dest_mask;

	/** Set intr_src.src.msi.value to be phys_bdf */
	intr_src.src.msi.value = phys_bdf;
	/** Set low 6-bits of 'index' to be low 6-bits of 'virt_bdf' and
	 *  set high 10-bits of 'index' to be low 10-bits of 'vm->vm_id' */
	index = ((virt_bdf & 0x3FU) | (uint16_t)(vm->vm_id << 0x6U)) & 0xFFU;
	/** Call dmar_assign_irte with the following parameters, in order to
	 *  assign an interrupt remapping entry to the given interrupt request source.
	 *  - intr_src
	 *  - irte
	 *  - index
	 */
	dmar_assign_irte(intr_src, irte, index);

	/** Set info->pmsi_data.full to be 0H */
	info->pmsi_data.full = 0U;
	/** Set ir_index.index to be index */
	ir_index.index = index;

	/** Set info->pmsi_addr.full to be 0H */
	info->pmsi_addr.full = 0UL;
	/** Set info->pmsi_addr.ir_bits.intr_index_high to be ir_index.bits.index_high */
	info->pmsi_addr.ir_bits.intr_index_high = ir_index.bits.index_high;
	/** Set info->pmsi_addr.ir_bits.shv to be 0H */
	info->pmsi_addr.ir_bits.shv = 0U;
	/** Set info->pmsi_addr.ir_bits.intr_format to be 1H */
	info->pmsi_addr.ir_bits.intr_format = 0x1U;
	/** Set info->pmsi_addr.ir_bits.intr_index_low to be ir_index.bits.index_low */
	info->pmsi_addr.ir_bits.intr_index_low = ir_index.bits.index_low;
	/** Set info->pmsi_addr.ir_bits.constant to be FEEH */
	info->pmsi_addr.ir_bits.constant = 0xFEEU;

	/** Logging the following information with a log level of ACRN_DBG_IRQ.
	 *  - (info->pmsi_addr.ir_bits.intr_format != 0U) ? " Remappable Format" : "Compatibility Format"
	 *  - info->vmsi_addr.full
	 *  - info->vmsi_data.full
	 *  - info->pmsi_addr.full
	 *  - info->pmsi_data.full
	 */
	dev_dbg(ACRN_DBG_IRQ, "MSI %s addr:data = 0x%lx:%x(V) -> 0x%lx:%x(P)",
		(info->pmsi_addr.ir_bits.intr_format != 0U) ? " Remappable Format" : "Compatibility Format",
		info->vmsi_addr.full, info->vmsi_data.full, info->pmsi_addr.full, info->pmsi_data.full);
}

/**
 * @brief The internal function for removing the MSI remapping.
 *
 * The public API ptirq_remove_msix_remapping calls this function to remove the MSI interrupt
 * remapping for the virtual PCI device with \a virt_bdf.
 *
 * @param[in] vm A pointer to virtual machine whose interrupt remapping
 *               is going to be removed.
 * @param[in] virt_bdf The virtual BDF associated with the specified passthrough device.
 * @param[in] entry_nr A parameter indicating the entry ID of the coming vector.
 *                     If entry_nr equals to 0, it means that it is the first vector.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre entry_nr == 0
 * @pre (virt_bdf & FFH) < 3FH
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
static void remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t entry_nr)
{
	/** Declare the following local variable of type 'struct intr_source'
	 *  - intr_src representing the interrupt source, not initialized */
	struct intr_source intr_src;
	/** Declare the following local variable of type uint16_t
	 *  - index representing the index of the interrupt remapping entry to be freed,
	 *    not initialized */
	uint16_t index;

	/** Set the intr_src.src.msi.value to be 0H */
	intr_src.src.msi.value = 0U;
	/** Set low 6-bits of 'index' to low 6-bits of 'virt_bdf' and set high
	 *  10-bits of 'index' to low 10-bits of 'vm->vm_id' */
	index = ((virt_bdf & 0x3FU) | (uint16_t)(vm->vm_id << 0x6U)) & 0xFFU;
	/** Call dmar_free_irte with the following parameters, in order to
	 *  free the interrupt remapping entry associated with the given 'index'.
	 *  - intr_src
	 *  - index
	 */
	dmar_free_irte(intr_src, index);
	/** Logging the following information with a log level of ACRN_DBG_IRQ.
	 *  - vm->vm_id
	 *  - virt_bdf
	 *  - entry_nr
	 */
	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remove vector mapping vbdf=0x%x idx=%d",
		vm->vm_id, virt_bdf, entry_nr);
}

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
	struct acrn_vm *vm, uint16_t virt_bdf, uint16_t phys_bdf, uint16_t entry_nr, struct ptirq_msi_info *info)
{
	/** Call ptirq_build_physical_msi with the following parameters, in order to
	 *  build the physical MSI remapping for a given virtual machine's passthrough device.
	 *  All the vCPUs are in x2APIC mode and LAPIC is pass-through, so use guest
	 *  vector to program the interrupt source.
	 *  - vm
	 *  - info
	 *  - virt_bdf
	 *  - phys_bdf
	 *  - (uint32_t)info->vmsi_data.bits.vector
	 */
	ptirq_build_physical_msi(vm, info, virt_bdf, phys_bdf, (uint32_t)info->vmsi_data.bits.vector);

	/** Logging the following information with a log level of ACRN_DBG_IRQ.
	 *  - vm->vm_id
	 *  - virt_bdf
	 *  - phys_bdf
	 *  - entry_nr
	 */
	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remap vbdf=0x%x pbdf=0x%x idx=%d",
		vm->vm_id, virt_bdf, phys_bdf, entry_nr);
}

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
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t vector_count)
{
	/** Declare the following local variable of type uint32_t
	 *  - i representing the ith vector, not initialized */
	uint32_t i;

	/** For each i ranging from 0H to 'vector_count - 1' with a step of 1 */
	for (i = 0U; i < vector_count; i++) {
		/** Call remove_msix_remapping with the following parameters, in order to
		 *  remove the interrupt remapping entry for MSI of the virtual PCI device
		 *  with \a virt_bdf.
		 *  - vm
		 *  - virt_bdf
		 *  - i
		 */
		remove_msix_remapping(vm, virt_bdf, i);
	}
}

/**
 * @}
 */
