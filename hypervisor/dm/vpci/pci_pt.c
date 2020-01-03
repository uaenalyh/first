/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <vm.h>
#include <ept.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */


static inline enum pci_bar_type pci_get_bar_type(uint32_t val)
{
	enum pci_bar_type type = PCIBAR_NONE;

	if ((val & PCIM_BAR_SPACE) != PCIM_BAR_IO_SPACE) {
		switch (val & PCIM_BAR_MEM_TYPE) {
		case PCIM_BAR_MEM_32:
			type = PCIBAR_MEM32;
			break;

		case PCIM_BAR_MEM_64:
			type = PCIBAR_MEM64;
			break;

		default:
			/*no actions are required for other cases.*/
			break;
		}
	}

	return type;
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static void vdev_pt_unmap_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct pci_bar *vbar;
	struct acrn_vm *vm = vdev->vpci->vm;

	vbar = &vdev->bar[idx];

	if (vbar->base != 0UL) {
		ept_del_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp), vbar->base, /* GPA (old vbar) */
			vbar->size);
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static void vdev_pt_map_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct pci_bar *vbar;
	struct acrn_vm *vm = vdev->vpci->vm;

	vbar = &vdev->bar[idx];

	if (vbar->base != 0UL) {
		ept_add_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp), vbar->base_hpa, /* HPA (pbar) */
			vbar->base, /* GPA (new vbar) */
			vbar->size, EPT_WR | EPT_RD | EPT_UNCACHED);
	}
}

/**
 * @pre vdev != NULL
 */
void vdev_pt_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val)
{
	uint32_t update_idx = idx;
	uint32_t offset = pci_bar_offset(idx);
	struct pci_bar *vbar = &vdev->bar[idx];

	switch (vbar->type) {

	case PCIBAR_NONE:
		/* Nothing to do */
		break;

	default:
		if (vbar->type == PCIBAR_MEM64HI) {
			update_idx -= 1U;
		}
		vdev_pt_unmap_mem_vbar(vdev, update_idx);
		if (val != ~0U) {
			pci_vdev_write_bar(vdev, idx, val);
			vdev_pt_map_mem_vbar(vdev, update_idx);
		} else {
			pci_vdev_write_cfg_u32(vdev, offset, val);
			vdev->bar[update_idx].base = 0UL;
		}
		break;
	}
}

/**
 * PCI base address register (bar) virtualization:
 *
 * Virtualize the PCI bars (up to 6 bars at byte offset 0x10~0x24 for type 0 PCI device,
 * 2 bars at byte offset 0x10-0x14 for type 1 PCI device) of the PCI configuration space
 * header.
 *
 * pbar: bar for the physical PCI device, the value of pbar (hpa) is assigned
 * by platform firmware during boot. It is assumed a valid hpa is always assigned to a
 * mmio pbar, hypervisor shall not change the value of a pbar.
 *
 * vbar: for each PCI device, it has a virtual PCI device (pci_vdev) counterpart. pci_vdev
 * virtualizes all the bars (called vbars). a vbar can be initialized by hypervisor by
 * assigning a gpa to it; if vbar has a value of 0 (unassigned), guest may assign
 * and program a gpa to it. The guest only sees the vbars, it will not see and can
 * never change the pbars.
 *
 * Hypervisor traps guest changes to the mmio vbar (gpa) to establish ept mapping
 * between vbar(gpa) and pbar(hpa). pbar should always align on 4K boundary.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->pdev != NULL
 */
void init_vdev_pt(struct pci_vdev *vdev)
{
	enum pci_bar_type type;
	uint32_t idx;
	struct pci_bar *vbar;
	uint16_t pci_command;
	uint32_t size32, offset, lo, hi = 0U;
	union pci_bdf pbdf;
	uint64_t mask;

	vdev->nr_bars = PCI_BAR_COUNT;
	pbdf.value = vdev->pbdf.value;

	for (idx = 0U; idx < vdev->nr_bars; idx++) {
		vbar = &vdev->bar[idx];
		offset = pci_bar_offset(idx);
		lo = pci_pdev_read_cfg(pbdf, offset, 4U);

		type = pci_get_bar_type(lo);
		if (type == PCIBAR_NONE) {
			continue;
		}
		mask = PCI_BASE_ADDRESS_MEM_MASK;
		vbar->base_hpa = (uint64_t)lo & mask;

		if (type == PCIBAR_MEM64) {
			hi = pci_pdev_read_cfg(pbdf, offset + 4U, 4U);
			vbar->base_hpa |= ((uint64_t)hi << 32U);
		}

		if (vbar->base_hpa != 0UL) {
			pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
			size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
			pci_pdev_write_cfg(pbdf, offset, 4U, lo);

			vbar->type = type;
			vbar->mask = size32 & mask;
			vbar->fixed = lo & (~mask);
			vbar->size = (uint64_t)size32 & mask;

			lo = (uint32_t)vdev->pci_dev_config->vbar_base[idx];

			if (type == PCIBAR_MEM64) {
				idx++;
				offset = pci_bar_offset(idx);
				pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
				size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
				pci_pdev_write_cfg(pbdf, offset, 4U, hi);

				vbar->size |= ((uint64_t)size32 << 32U);
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				vbar->size = round_page_up(vbar->size);

				vbar = &vdev->bar[idx];
				vbar->mask = size32;
				vbar->type = PCIBAR_MEM64HI;

				hi = (uint32_t)(vdev->pci_dev_config->vbar_base[idx - 1U] >> 32U);
				pci_vdev_write_bar(vdev, idx - 1U, lo);
				pci_vdev_write_bar(vdev, idx, hi);
			} else {
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				if (type == PCIBAR_MEM32) {
					vbar->size = round_page_up(vbar->size);
				}
				pci_vdev_write_bar(vdev, idx, lo);
			}
		}
	}

	pci_command = (uint16_t)pci_pdev_read_cfg(vdev->pbdf, PCIR_COMMAND, 2U);

	/* Disable INTX */
	pci_command |= 0x400U;
	pci_pdev_write_cfg(vdev->pbdf, PCIR_COMMAND, 2U, pci_command);
}

/**
 * @}
 */
