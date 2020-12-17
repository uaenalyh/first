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
 * @brief This file implements BAR operation APIs which are used within vPCI component
 *
 * This file implements BAR operation APIs which are used within vPCI component. The main parts focus on
 * the BAR EPT remapping and unmapping between its MMIO GPA and HPA space. It also defines one helper function to
 * implement the features that are commonly used in this file. In addition, it defines two decomposed functions to
 * improve the readability of the code.
 *
 * Following functions are internal APIs used by other source files within vPCI:
 * - init_vdev_pt: initialize the BAR registers of the vPCI device associated with a physical PCI device
 * - vdev_pt_write_vbar: write a BAR register of the vPCI device associated with a physical PCI device
 *
 * Helper functions:
 * - pci_get_bar_type: get the type of a BAR according to the given register value, called by init_vdev_pt
 *
 * Decomposed functions:
 * - vdev_pt_unmap_mem_vbar: unmap the MMIO GPA of the BAR, called by vdev_pt_write_vbar
 * - vdev_pt_map_mem_vbar: map the MMIO GPA of the BAR to its HPA, called by vdev_pt_write_vbar
 */

/**
 * @brief Get the type of a BAR
 *
 * This function is called to get the type of a BAR according to the given BAR register value. Its low 4bits indicates
 * the BAR type, which includes: IO BAR and MMIO BAR. And there are two types of MMIO BAR: 32bits and 64bits BAR.
 *
 * @param[in] val A BAR register value to check type
 *
 * @return An enum value to indicate the BAR type
 *
 * @pre None
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by init_vdev_pt
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static inline enum pci_bar_type pci_get_bar_type(uint32_t val)
{
	/** Declare the following local variables of type 'enum pci_bar_type'.
	 *  - type representing the type of a BAR, initialized as PCIBAR_NONE. */
	enum pci_bar_type type = PCIBAR_NONE;

	/** If bit0 of 'val' is not 1, which means it is a MMIO BAR */
	if ((val & PCIM_BAR_SPACE) != PCIM_BAR_IO_SPACE) {
		/** Depending on the bit1:2 of 'val' */
		switch (val & PCIM_BAR_MEM_TYPE) {
		/** bit 2:1 is 00, which means it is a 32bits MMIO BAR. */
		case PCIM_BAR_MEM_32:
			/** Set type to PCIBAR_MEM32 */
			type = PCIBAR_MEM32;
			/** End of case */
			break;

		/** bit 2:1 is 10, which means it is a 64bits MMIO BAR. */
		case PCIM_BAR_MEM_64:
			/** Set type to PCIBAR_MEM64 */
			type = PCIBAR_MEM64;
			/** End of case */
			break;

		/** Otherwise */
		default:
			/** End of case */
			break;
		}
	}

	/** Return the BAR type, if not matched, return the default value: PCIBAR_NONE */
	return type;
}

/**
 * @brief Unmap a BAR MMIO space between GPA and HPA
 *
 * This function is called to unmap a BAR MMIO space between GPA and HPA. The BAR info is got from the given vPCI
 * device and the given index. It will call ept_del_mr to do the unmapping in EPT table.
 *
 * @param[inout] vdev A vPCI device whose BAR space is to unmap.
 * @param[in] idx The BAR index.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by vdev_pt_write_vbar.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vdev_pt_unmap_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing a pointer to the BAR info, initialized as &vdev->bar[idx]. */
	struct pci_bar *vbar = &vdev->bar[idx];
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM that the given vPCI device belongs to, initialized as vdev->vpci->vm. */
	struct acrn_vm *vm = vdev->vpci->vm;

	/** If vbar->base is not 0, which means it is a valid GPA. */
	if (vbar->base != 0UL) {
		/** Call ept_del_mr with the following parameters, in order to do the unmapping of the BAR
		 *  space (BAR GPA base and its BAR size) in the EPT table.
		 *  - vm
		 *  - vm->arch_vm.nworld_eptp
		 *  - vbar->base
		 *  - vbar->size
		 */
		ept_del_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp), vbar->base, /* GPA (old vbar) */
			vbar->size);
	}
}

/**
 * @brief Remap a BAR MMIO space between GPA and HPA
 *
 * This function is called to remap a BAR MMIO space between GPA and HPA. The BAR info is got from the given vPCI
 * device and the given index. It will call ept_add_mr to do the remapping in EPT table.
 *
 * @param[inout] vdev A vPCI device whose BAR space is to remap.
 * @param[in] idx The BAR index.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by vdev_pt_write_vbar.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vdev_pt_map_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing a pointer to the BAR info, initialized as &vdev->bar[idx]. */
	struct pci_bar *vbar = &vdev->bar[idx];
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM that the given vPCI device belongs to, initialized as vdev->vpci->vm. */
	struct acrn_vm *vm = vdev->vpci->vm;

	/** If vbar->base is not 0, which means it is a valid GPA. */
	if (vbar->base != 0UL) {
		/** Call ept_del_mr with the following parameters, in order to do the unmapping of the BAR
		 *  space (BAR GPA base and its BAR size) in the EPT table.
		 *  - vm
		 *  - vm->arch_vm.nworld_eptp
		 *  - vbar->base
		 *  - vbar->size
		 */
		ept_del_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp), vbar->base, /* GPA (new vbar) */
			vbar->size);
		/** Call ept_add_mr with the following parameters, in order to do the remapping of the BAR
		 *  space (BAR base and its BAR size) between GPA and HPA in the EPT table.
		 *  - vm
		 *  - vm->arch_vm.nworld_eptp
		 *  - vbar->base_hpa
		 *  - vbar->base
		 *  - vbar->size
		 *  - EPT_WR | EPT_RD | EPT_UNCACHED
		 */
		ept_add_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp), vbar->base_hpa, /* HPA (pbar) */
			vbar->base, /* GPA (new vbar) */
			vbar->size, EPT_WR | EPT_RD | EPT_UNCACHED);
	}
}

/**
 * @brief Write a BAR register of the given vPCI device associated with a physical PCI device.
 *
 * This function is called to write a BAR register of the given vPCI device associated with a physical PCI device.
 * The BAR info is got from the given vPCI device and the given index. According to the given 'val' to write, it could
 * do unmapping / remapping for the BAR MMIO space between GPA and HPA.
 *
 * @param[inout] vdev A vPCI device whose BAR register is to write.
 * @param[in] idx The BAR index.
 * @param[in] val The value to write to the BAR register.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
void vdev_pt_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val)
{
	/** Declare the following local variables of type uint32_t.
	 *  - update_idx representing the index of the BAR whose base address is to update, initialized as 'idx'. */
	uint32_t update_idx = idx;
	/** Declare the following local variables of type uint32_t.
	 *  - offset representing the BAR position in the configuration space, initialized as the value returned by
	 *  pci_bar_offset with idx being the parameter, which gets the offset according to the given BAR index.
	 */
	uint32_t offset = pci_bar_offset(idx);
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing a pointer to the BAR info, initialized as &vdev->bar[idx]. */
	struct pci_bar *vbar = &vdev->bar[idx];

	/** Depending on vbar->type, the type of the BAR */
	switch (vbar->type) {

	/** BAR type  is PCIBAR_NONE, not need handle */
	case PCIBAR_NONE:
		/** End of case */
		break;

	/** Otherwise */
	default:
		/** If the BAR type is PCIBAR_MEM64HI, the high 32bits of the 64bits BAR */
		if (vbar->type == PCIBAR_MEM64HI) {
			/** Decrement update_idx by 1, for BAR base updating need start from low 32bits */
			update_idx -= 1U;
		}
		/** Call vdev_pt_unmap_mem_vbar with the following parameters, in order to unmap the BAR space first.
		 *  - vdev
		 *  - update_idx
		 */
		vdev_pt_unmap_mem_vbar(vdev, update_idx);

		/** Call pci_vdev_write_bar with the following parameters, in order to write the BAR register
		 *  in its virtual configuration space and update its BAR base info.
		 *  - vdev
		 *  - idx
		 *  - val
		 */
		pci_vdev_write_bar(vdev, idx, val);
		/** Call vdev_pt_map_mem_vbar with the following parameters, in order to remap the BAR space
		 *  between the new GPA of BAR base and its HPA.
		 *  - vdev
		 *  - update_idx
		 */
		vdev_pt_map_mem_vbar(vdev, update_idx);

		/** End of case */
		break;
	}
}

/**
 * @brief Initialize BAR registers of the given vPCI device associated with a physical PCI device.
 *
 * This function is called to initialize BAR registers of the given vPCI device associated with a physical
 * PCI device. It is focused on the BAR virtualization. Following is BAR related information:
 *
 * PCI BARs number: up to 6 bars at byte offset 0x10~0x24 for type 0 PCI device, 2 bars at byte offset 0x10-0x14 for
 * type 1 PCI device; all of them are located in the PCI configuration space header.
 *
 * Physical BAR (pbar): BAR for the physical PCI device, the value of pbar (HPA) is assigned by platform firmware
 * during boot. It is assumed a valid HPA is always assigned to a MMIO pbar, hypervisor shall not change the value
 * of a pbar.
 *
 * Virtual BAR (vbar): for each physical PCI device, it has a virtual PCI device (pci_vdev) counterpart. It is used to
 * virtualize all the BARs (called vbars). A vbar can be initialized by hypervisor to assign a GPA to it; or if a vbar
 * has a value of 0 (unassigned), guest OS may assign and program a GPA to it. The guest can only see the vbars,
 * it can't see and change the pbars.
 *
 * A BAR size: it can be get by writing 0FFFF FFFFH to a physical BAR register first, then read it. Assume that the
 * register's value to read is A, the BAR size is:  ~(A & ~0FH) + 1
 *
 * When a guest OS changes the MMIO vbar (GPA), hypervisor will trap it and establish EPT mapping between vbar
 * (GPA) and pbar(HPA). Also vbar and pbar should always align on 4K boundary.
 *
 * @param[inout] vdev A vPCI device associated with a physical PCI device
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->pdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
void init_vdev_pt(struct pci_vdev *vdev)
{
	/** Declare the following local variables of type 'enum pci_bar_type'.
	 *  - type representing the type of a BAR, not initialized. */
	enum pci_bar_type type;
	/** Declare the following local variables of type uint32_t.
	 *  - idx representing the index of a BAR, not initialized. */
	uint32_t idx;
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing a pointer to a BAR's info structure, not initialized. */
	struct pci_bar *vbar;
	/** Declare the following local variables of type uint32_t.
	 *  - size32 representing the size of a BAR, not initialized.
	 *  - offset representing the offset of a BAR in PCI configuration space, not initialized.
	 *  - lo representing the low 32bits base address value of a BAR if it is a 64bits BAR, not initialized.
	 *  - hi representing the high 32bits base address value of a BAR if it is a 64bits BAR, initialized as 0.
	 */
	uint32_t size32, offset, lo, hi = 0U;
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - pbdf representing the BDF of a physical PCI device associated with the given vPCI device,
	 *  not initialized.
	 */
	union pci_bdf pbdf;
	/** Declare the following local variables of type uint64_t.
	 *  - mask representing the mask bits to get the BAR base address from its register value, not initialized. */
	uint64_t mask;

	/** Set vdev->nr_bars to 6, for type 0 PCI device has 6 BAR registers in total. */
	vdev->nr_bars = PCI_BAR_COUNT;
	/** Set pbdf.value to vdev->pbdf.value */
	pbdf.value = vdev->pbdf.value;

	/** For each 'idx' ranging from 0 to 5 [with a step of 1], which is to probe each BAR register */
	for (idx = 0U; idx < vdev->nr_bars; idx++) {
		/** Set vbar to &vdev->bar[idx] */
		vbar = &vdev->bar[idx];
		/** Set offset to the value returned by pci_bar_offset with idx being the parameter, which gets the
		 *  BAR offset in PCI configuration space.
		 */
		offset = pci_bar_offset(idx);
		/** Set lo to the value returned by pci_pdev_read_cfg with pbdf, offset and 4H being the parameters,
		 *  which reads the BAR's register value from its corresponding physical PCI devices.
		 */
		lo = pci_pdev_read_cfg(pbdf, offset, 4U);

		/** Set type to the value returned by pci_get_bar_type with lo being the parameter, which gets the
		 *  BAR type according to its register value.
		 */
		type = pci_get_bar_type(lo);
		/** If type is PCIBAR_NONE, not need handle it */
		if (type == PCIBAR_NONE) {
			/** Continue to next iteration */
			continue;
		}
		/** Set mask to PCI_BASE_ADDRESS_MEM_MASK */
		mask = PCI_BASE_ADDRESS_MEM_MASK;
		/** Set vbar->base_hpa to 'lo & mask', which gets the BAR base address (low 32bits) from its register
		 *  value.
		 */
		vbar->base_hpa = (uint64_t)lo & mask;

		/** If type is PCIBAR_MEM64, which means this BAR is a 64bits BAR. */
		if (type == PCIBAR_MEM64) {
			/** Set hi to the value returned by pci_pdev_read_cfg with pbdf, offset + 4 and 4H being the
			 *  parameters, which reads the value of the high 32bits BAR register from its corresponding
			 *  physical PCI devices.
			 */
			hi = pci_pdev_read_cfg(pbdf, offset + 4U, 4U);
			/** Bitwise OR vbar->base_hpa by hi << 32, to form the 64bits base address combined with
			 *  the low 32bits value.
			 */
			vbar->base_hpa |= ((uint64_t)hi << 32U);
		}

		/** If vbar->base_hpa is not 0, which means this physical BAR is valid and hypervisor need remap the
		 *  virtual BAR to it.
		 */
		if (vbar->base_hpa != 0UL) {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write 0FFFF FFFFH to its
			 *  physical BAR register and to get the BAR size.
			 *  - pbdf
			 *  - offset
			 *  - 4
			 *  - ~0
			 */
			pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
			/** Set size32 to the value returned by pci_pdev_read_cfg with pbdf, offset and 4H being the
			 *  parameters, which reads the physical BAR register value.
			 */
			size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
			/** Call pci_pdev_write_cfg with the following parameters, in order to restore the former value
			 *  to the physical BAR register
			 *  - pbdf
			 *  - offset
			 *  - 4
			 *  - lo
			 */
			pci_pdev_write_cfg(pbdf, offset, 4U, lo);

			/** Set vbar->type to type, to save the BAR type info */
			vbar->type = type;
			/** Set vbar->mask to 'size32 & mask', which clears the low 4bits of the BAR size. */
			vbar->mask = size32 & mask;
			/** Set vbar->fixed to lo & (~mask), which keeps the low 4bits of the BAR register. */
			vbar->fixed = lo & (~mask);
			/** Set vbar->size to 'size32 & mask', which clears the low 4bits of the BAR size and needs
			 *  further calculation to get its BAR size. */
			vbar->size = (uint64_t)size32 & mask;

			/** Set lo to the GPA of the vbar base address in the guest VM configuration. */
			lo = (uint32_t)vdev->pci_dev_config->vbar_base[idx];

			/** If the BAR is 64bits BAR */
			if (type == PCIBAR_MEM64) {
				/** Increment 'idx' by 1 to get high 32bits info of the BAR */
				idx++;
				/** Set offset to the value returned by pci_bar_offset with idx being the parameter,
				 *  which gets the offset of the high 32bits of the BAR register in PCI configuration
				 *  space.
				 */
				offset = pci_bar_offset(idx);
				/** Call pci_pdev_write_cfg with the following parameters, in order to write
				 *  0FFFF FFFFH to its physical BAR register and to get the BAR size of high 32bits
				 *  - pbdf
				 *  - offset
				 *  - 4
				 *  - ~0
				 */
				pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
				/** Set size32 to the value returned by pci_pdev_read_cfg with pbdf, offset and 4H
				 *  being the parameters, which reads the high 32bits physical BAR register value.
				 */
				size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
				/** Call pci_pdev_write_cfg with the following parameters, in order to restore the
				 *  former value to the physical BAR register
				 *  - pbdf
				 *  - offset
				 *  - 4
				 *  - hi
				 */
				pci_pdev_write_cfg(pbdf, offset, 4U, hi);

				/** Bitwise OR vbar->size by (size32 << 32), to form the 64bits value */
				vbar->size |= ((uint64_t)size32 << 32U);
				/** Set vbar->size to (vbar->size & ~(vbar->size - 1)) to get the BAR size. */
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				/** Set vbar->size to the value returned by round_page_up with vbar->size being the
				 *  parameter, to make the size 4K aligned.
				 */
				vbar->size = round_page_up(vbar->size);

				/** Set vbar to &vdev->bar[idx], used to save high32 bits BAR info */
				vbar = &vdev->bar[idx];
				/** Set vbar->mask to size32 */
				vbar->mask = size32;
				/** Set vbar->type to PCIBAR_MEM64HI, to mark it's the high 32bits BAR. */
				vbar->type = PCIBAR_MEM64HI;

				/** Set hi to the GPA of the high 32bits vbar base in the guest VM configuration. */
				hi = (uint32_t)(vdev->pci_dev_config->vbar_base[idx - 1U] >> 32U);
				/** Call pci_vdev_write_bar with the following parameters, in order to update the
				 *  low 32bits vbar of the given vPCI device.
				 *  - vdev
				 *  - idx - 1
				 *  - lo
				 */
				pci_vdev_write_bar(vdev, idx - 1U, lo);
				/** Call pci_vdev_write_bar with the following parameters, in order to update the
				 *  high 32bits vbar of the given vPCI device.
				 *  - vdev
				 *  - idx
				 *  - hi
				 */
				pci_vdev_write_bar(vdev, idx, hi);

				/** Call vdev_pt_map_mem_vbar with the following parameters, in order to remap
				 *  the 64bits vbar space of the given vPCI device.
				 *  - vdev
				 *  - idx - 1
				 */
				vdev_pt_map_mem_vbar(vdev, idx - 1U);
			} else {
				/** Set vbar->size to (vbar->size & ~(vbar->size - 1)) to get the BAR size. */
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				/** If the BAR is 32bits BAR */
				if (type == PCIBAR_MEM32) {
					/** Set vbar->size to the value returned by round_page_up with vbar->size
					 *  being the parameter, to make the size 4K aligned.
					 */
					vbar->size = round_page_up(vbar->size);
				}
				/** Call pci_vdev_write_bar with the following parameters, in order to update the
				 *  vbar of the given vPCI device.
				 *  - vdev
				 *  - idx
				 *  - lo
				 */
				pci_vdev_write_bar(vdev, idx, lo);
				/** Call vdev_pt_map_mem_vbar with the following parameters, in order to remap
				 *  the vbar space of the given vPCI device.
				 *  - vdev
				 *  - idx
				 */
				vdev_pt_map_mem_vbar(vdev, idx);
			}
		}
	}
}

/**
 * @}
 */
