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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

static uint32_t calculate_logical_dest_mask(uint64_t pdmask)
{
	uint32_t dest_mask = 0UL;
	uint64_t pcpu_mask = pdmask;
	uint16_t pcpu_id;

	pcpu_id = ffs64(pcpu_mask);
	while (pcpu_id < MAX_PCPU_NUM) {
		bitmap_clear_nolock(pcpu_id, &pcpu_mask);
		dest_mask |= per_cpu(lapic_ldr, pcpu_id);
		pcpu_id = ffs64(pcpu_mask);
	}
	return dest_mask;
}

static void ptirq_build_physical_msi(
	struct acrn_vm *vm, struct ptirq_msi_info *info, uint16_t virt_bdf, uint16_t phys_bdf, uint32_t vector)
{
	uint64_t vdmask, pdmask;
	uint32_t dest, delmode, dest_mask;
	bool phys;
	union dmar_ir_entry irte;
	union irte_index ir_index;
	struct intr_source intr_src;
	uint16_t index;

	/* get physical destination cpu mask */
	dest = info->vmsi_addr.bits.dest_field;
	phys = (info->vmsi_addr.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS);

	vlapic_calc_dest(vm, &vdmask, false, dest, phys, false);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data.bits.delivery_mode;
	if ((delmode != MSI_DATA_DELMODE_FIXED) && (delmode != MSI_DATA_DELMODE_LOPRI)) {
		delmode = MSI_DATA_DELMODE_LOPRI;
	}

	dest_mask = calculate_logical_dest_mask(pdmask);

	/* Using phys_irq as index in the corresponding IOMMU */
	irte.entry.lo_64 = 0UL;
	irte.entry.hi_64 = 0UL;
	irte.bits.vector = vector;
	irte.bits.delivery_mode = delmode;
	irte.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	irte.bits.rh = MSI_ADDR_RH;
	irte.bits.dest = dest_mask;

	/* Set false since intr_src.is_msi is useless */
	intr_src.src.msi.value = phys_bdf;
	index = ((virt_bdf & 0x3FU) | (uint16_t)(vm->vm_id << 0x6U)) & 0xFFU;
	dmar_assign_irte(intr_src, irte, index);

	/*
	 * Update the MSI interrupt source to point to the IRTE
	 * SHV is set to 0 as ACRN disables MMC (Multi-Message Capable
	 * for MSI devices.
	 */
	info->pmsi_data.full = 0U;
	ir_index.index = index;

	info->pmsi_addr.full = 0UL;
	info->pmsi_addr.ir_bits.intr_index_high = ir_index.bits.index_high;
	info->pmsi_addr.ir_bits.shv = 0U;
	info->pmsi_addr.ir_bits.intr_format = 0x1U;
	info->pmsi_addr.ir_bits.intr_index_low = ir_index.bits.index_low;
	info->pmsi_addr.ir_bits.constant = 0xFEEU;

	dev_dbg(ACRN_DBG_IRQ, "MSI %s addr:data = 0x%lx:%x(V) -> 0x%lx:%x(P)",
		(info->pmsi_addr.ir_bits.intr_format != 0U) ? " Remappable Format" : "Compatibility Format",
		info->vmsi_addr.full, info->vmsi_data.full, info->pmsi_addr.full, info->pmsi_data.full);
}

/* deactive & remove mapping entry of vbdf:entry_nr for vm */
static void remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t entry_nr)
{
	struct intr_source intr_src;
	uint16_t index;

	intr_src.src.msi.value = 0U;
	index = ((virt_bdf & 0x3FU) | (uint16_t)(vm->vm_id << 0x6U)) & 0xFFU;
	dmar_free_irte(intr_src, index);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remove vector mapping vbdf=0x%x idx=%d",
		vm->vm_id, virt_bdf, entry_nr);
}

/* Main entry for PCI device assignment with MSI and MSI-X
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors
 * We use entry_nr to indicate coming vectors
 * entry_nr = 0 means first vector
 * user must provide bdf and entry_nr
 */
void ptirq_msix_remap(
	struct acrn_vm *vm, uint16_t virt_bdf, uint16_t phys_bdf, uint16_t entry_nr, struct ptirq_msi_info *info)
{
	/*
	 * All the vCPUs are in x2APIC mode and LAPIC is Pass-through
	 * Use guest vector to program the interrupt source
	 */
	ptirq_build_physical_msi(vm, info, virt_bdf, phys_bdf, (uint32_t)info->vmsi_data.bits.vector);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remap vbdf=0x%x pbdf=0x%x idx=%d",
		vm->vm_id, virt_bdf, phys_bdf, entry_nr);
}

/*
 * @pre vm != NULL
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t vector_count)
{
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		remove_msix_remapping(vm, virt_bdf, i);
	}
}

/**
 * @}
 */
