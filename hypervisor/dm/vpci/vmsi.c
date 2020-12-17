/*
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
#include <ptdev.h>
#include <assign.h>
#include <vpci.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file implements MSI operation APIs which are used within vPCI component
 *
 * This file implements MSI operation APIs which are used within vPCI component. The main parts focus on
 * the MSI interrupt remapping implementation. It also defines one helper function to implement the features that
 * are commonly used in this file. In addition, it defines one decomposed function to improve the readability of the
 * code.
 *
 * About MSI, refer the PCI spec:
 * - Message signalled interrupts (MSI) is an optional feature that enables PCI devices to request service by writing a
 * system-specified message to a system-specified address (PCI DWORD memory write transaction). The transaction
 * address specifies the message destination while the transaction data specifies the message. System software is
 * expected to initialize the message destination and message during device configuration, allocating one or more
 * non-shared messages to each MSI capable function.
 *
 * About MSI remapping:
 * - The 'system-specified address' (MSI address) should be located in a physical APIC (within a physical CPU). And
 * different APIC has different physical address. So for the physical PCI device which is passthrough to a guest VMs,
 * its MSI address need be remapped (routed) to a different physical CPU which the guest VM runs on.
 *
 * Following functions are internal APIs used by other source files within vPCI:
 * - init_vmsi: initialize the virtual MSI info of the vPCI device which is associated with a physical PCI device
 * - deinit_vmsi: remove the MSI remapping of the vPCI device which is associated with a physical PCI device
 * - vmsi_write_cfg: write the MSI capability structure, which will invoke the MSI remapping function
 *
 * Helper functions:
 * - enable_disable_msi: used to disable/enable the MSI of the physical PCI device assocated with the vPCI device
 *
 * Decomposed functions:
 * - remap_vmsi: do the MSI remapping for the given vPCI device which is associated with a physical PCI device.
 */


/**
 * @brief Enable or disable the MSI of the given vPCI device associated with a physical PCI device.
 *
 * This function is called to enable or disable the MSI of the given vPCI device which is associated with a physical
 * PCI device. It will write the MSI control register to do the work.
 *
 * @param[in] vdev A vPCI device associated with a physical PCI device whose MSI control register is to write
 * @param[in] enable A boolean value to indicate the operation type: enable or disable MSI
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->pbdf != 0
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by remap_vmsi and vmsi_write_cfg
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static inline void enable_disable_msi(const struct pci_vdev *vdev, bool enable)
{
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - pbdf representing the physical BDF of the physical PCI device associated with the given vPCI device,
	 *  initialized as vdev->pbdf.
	 */
	union pci_bdf pbdf = vdev->pbdf;
	/** Declare the following local variables of type uint32_t.
	 *  - capoff representing the offset of the given vPCI device's MSI capabilit structure, initialized as
	 *  vdev->msi.capoff.
	 */
	uint32_t capoff = vdev->msi.capoff;
	/** Declare the following local variables of type uint32_t.
	 *  - msgctrl representing the value of the MSI control register, initialized as the value returned by
	 *  pci_pdev_read_cfg with pbdf, capoff + PCIR_MSI_CTRL and 2 being the parameters, which reads
	 *  from the physical configuration space.
	 */
	uint32_t msgctrl = pci_pdev_read_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U);

	/** If 'enable' is true, which is to enable MSI */
	if (enable) {
		/** Bitwise OR msgctrl by PCIM_MSICTRL_MSI_ENABLE */
		msgctrl |= PCIM_MSICTRL_MSI_ENABLE;
	} else {
		/** Bitwise AND msgctrl by '~PCIM_MSICTRL_MSI_ENABLE' */
		msgctrl &= ~PCIM_MSICTRL_MSI_ENABLE;
	}
	/** Call pci_pdev_write_cfg with the following parameters, in order to enable or disable MSI of the physical
	 *  PCI device by writing its physical MSI control register.
	 *  - pbdf
	 *  - capoff + PCIR_MSI_CTRL
	 *  - 2
	 *  - msgctrl
	 */
	pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U, msgctrl);
}

/**
 * @brief Remap MSI virtual address and data to MSI physical address and data
 *
 * This function is called to remap MSI virtual address and data to MSI physical address and data of the given vPCI
 * device which is associated with a physical PCI device. It can only be called when the physical MSI is disabled.
 *
 * @param[in] vdev A vPCI device associated with a physical PCI device whose MSI control register is to write
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->pbdf != 0
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by remap_vmsi and vmsi_write_cfg
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void remap_vmsi(const struct pci_vdev *vdev)
{
	/** Declare the following local variables of type 'struct ptirq_msi_info'.
	 *  - info representing a data structure of the physical and virtual MSI information, not initialized. */
	struct ptirq_msi_info info;
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - pbdf representing the physical BDF corresponding to the given vPCI, initialized as vdev->pbdf. */
	union pci_bdf pbdf = vdev->pbdf;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the pointer to the VM instance which the given vPCI belongs to, initialized as
	 *  vdev->vpci->vm.
	 */
	struct acrn_vm *vm = vdev->vpci->vm;
	/** Declare the following local variables of type uint32_t.
	 *  - capoff representing the offset of the given vPCI device's MSI capabilit structure, initialized as
	 *  vdev->msi.capoff.
	 */
	uint32_t capoff = vdev->msi.capoff;
	/** Declare the following local variables of type uint32_t.
	 *  - vmsi_msgdata representing the MSI virtual message data, not initialized.
	 *  - vmsi_addrlo representing the low 32bits of the MSI virtual message address, not initialized.
	 *  - vmsi_addrhi representing the high 32bits of the MSI virtual message address, initialized as 0.
	 */
	uint32_t vmsi_msgdata, vmsi_addrlo, vmsi_addrhi = 0U;

	/** Set vmsi_addrlo to the value returned by pci_vdev_read_cfg_u32 with vdev and capoff + PCIR_MSI_ADDR
	 *  being the parameters, which reads the MSI address register from its capability structure in its virtual
	 *  configuration space.
	 */
	vmsi_addrlo = pci_vdev_read_cfg_u32(vdev, capoff + PCIR_MSI_ADDR);
	/** If MSI address is 64bits */
	if (vdev->msi.is_64bit) {
		/** Set vmsi_addrhi to the value returned by pci_vdev_read_cfg_u32 with vdev and
		 *  capoff + PCIR_MSI_ADDR_HIGH being the parameters, which reads the MSI address register
		 *  high 32bits from its capability structure in its virtual configuration space.
		 */
		vmsi_addrhi = pci_vdev_read_cfg_u32(vdev, capoff + PCIR_MSI_ADDR_HIGH);
		/** Set vmsi_msgdata to value returned by pci_vdev_read_cfg_u16 with vdev and
		 *  capoff + PCIR_MSI_DATA_64BIT being the parameters, which reads the MSI data register
		 *  from its capability structure in its virtual configuration space.
		 */
		vmsi_msgdata = pci_vdev_read_cfg_u16(vdev, capoff + PCIR_MSI_DATA_64BIT);
	} else {
		/** Set vmsi_msgdata to value returned by pci_vdev_read_cfg_u16 with vdev and
		 *  capoff + PCIR_MSI_DATA being the parameters, which reads the MSI data register
		 *  from its capability structure in its virtual configuration space.
		 */
		vmsi_msgdata = pci_vdev_read_cfg_u16(vdev, capoff + PCIR_MSI_DATA);
	}
	/** Set info.vmsi_addr.full to 'vmsi_addrlo | (vmsi_addrhi << 32)' */
	info.vmsi_addr.full = (uint64_t)vmsi_addrlo | ((uint64_t)vmsi_addrhi << 32U);
	/** Set info.vmsi_data.full to vmsi_msgdata */
	info.vmsi_data.full = vmsi_msgdata;

	/** If the remmaping vector number is in the range [0x10,0xfe]
	 */
	if ((info.vmsi_data.bits.vector >= 0x10U) && (info.vmsi_data.bits.vector <= 0xfeU)) {
		/** Call ptirq_msix_remap with the following parameters, in order to calculate the physical MSI message
		 * data and message address according to its virtual MSI data and address.
		 *  - vm
		 *  - vdev->bdf.value
		 *  - pbdf.value
		 *  - 0
		 *  - &info
		 */
		ptirq_msix_remap(vm, vdev->bdf.value, pbdf.value, 0U, &info);
		/** Call pci_pdev_write_cfg with the following parameters, in order to write the calculated physical
		 * MSI message address to its physical register.
		 *  - pbdf
		 *  - capoff + PCIR_MSI_ADDR
		 *  - 4
		 *  - (uint32_t)info.pmsi_addr.full
		 */
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR, 0x4U, (uint32_t)info.pmsi_addr.full);
		/** If the MSI address is 64bits */
		if (vdev->msi.is_64bit) {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write the calculated
			 * physical MSI message address (high 32bits) to its physical register.
			 * - pbdf
			 * - capoff + PCIR_MSI_ADDR_HIGH
			 * - 4
			 * - (uint32_t)(info.pmsi_addr.full >> 32U)
			 */
			pci_pdev_write_cfg(
				pbdf, capoff + PCIR_MSI_ADDR_HIGH, 0x4U, (uint32_t)(info.pmsi_addr.full >> 32U));
			/** Call pci_pdev_write_cfg with the following parameters, in order to write the calculated
			 * physical MSI  message data to its physical register.
			 * - pbdf
			 * - capoff + PCIR_MSI_DATA_64BIT
			 * - 2
			 * - info.pmsi_data.full
			 */
			pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA_64BIT, 0x2U, (uint16_t)info.pmsi_data.full);
		} else {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write the calculated
			 * physical MSI message data to its physical register.
			 * - pbdf
			 * - capoff + PCIR_MSI_DATA
			 * - 2
			 * - info.pmsi_data.full
			 */
			pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA, 0x2U, (uint16_t)info.pmsi_data.full);
		}

		/** Call enable_disable_msi with the following parameters, in order to enable the MSI.
		 *  - vdev
		 *  - true
		 */
		enable_disable_msi(vdev, true);
	}
}

/**
 * @brief Write the register in the MSI capability structure
 *
 * This function is called to write the register in the MSI capability structure of a physical PCI device, which is
 * associated with the given vPCI device.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 * @param[in] offset The register offset in the PCI configure space.
 * @param[in] bytes The length of the register to write.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal callback API called by write_cfg.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
void vmsi_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/** Declare the following local variables of type uint32_t.
	 *  - msgctrl representing the value of the MSI control register, not initialized. */
	uint32_t msgctrl;
	/** Declare the following local variables of type uint32_t.
	 *  - old representing the value of the \a bytes value located in the vPCI configurations space,
	 *  not initialized. */
	uint32_t old;
	/** Declare the following local variables of type uint32_t.
	 *  - ro_mask representing the MSI read-only bit, initialized as ~0. */
	uint32_t ro_mask = ~0U;

	/** Declare the following local variables of type const uint8_t [10U].
	 *  - msi_32_ro_mask representing the 32-bit MSI read-only bit. */
	static const uint8_t msi_32_ro_mask[10U] = {0xffU, 0xffU, 0xfeU, 0xffU,
						    0xffU, 0x0fU, 0xf0U, 0xffU,
						    0x00U, 0xffU};
	/** Declare the following local variables of type const uint8_t [14U].
	 *  - msi_64_ro_mask representing the 64-bit MSI read-only bit. */
	static const uint8_t msi_64_ro_mask[14U] = {0xffU, 0xffU, 0xfeU, 0xffU,
						    0xffU, 0x0fU, 0xf0U, 0xffU,
						    0xffU, 0xffU, 0xffU, 0xffU,
						    0x00U, 0xffU};

	/** If MSI is 64bit */
	if (vdev->msi.is_64bit) {
		/** Call memcpy_s with the following parameters, in order to set ro_mask to 64bit MSI read-only mask.
		 *  - &ro_mask
		 *  - bytes
		 *  - &msi_64_ro_mask[offset - vdev->msi.capoff]
		 *  - bytes
		 */
		(void)memcpy_s((void *)&ro_mask, bytes, (void *)&msi_64_ro_mask[offset - vdev->msi.capoff], bytes);
	} else {
		/** Call memcpy_s with the following parameters, in order to set ro_mask to 32bit MSI read-only mask.
		 *  - &ro_mask
		 *  - bytes
		 *  - &msi_32_ro_mask[offset - vdev->msi.capoff]
		 *  - bytes
		 */
		(void)memcpy_s((void *)&ro_mask, bytes, (void *)&msi_32_ro_mask[offset - vdev->msi.capoff], bytes);
	}

	/* If there is any bit that is not set in ro_mask. */
	if (ro_mask != ~0U) {
		/** Call enable_disable_msi with the following parameters, in order to disable the physical MSI of
		 *  the physical PCI device associated with the given vPCI device. It is required to disable MSI
		 *  before remapping the virtual MSI data/address to the phyiscal MSI data/address.
		 *  - vdev
		 *  - false
		 */
		enable_disable_msi(vdev, false);
		/** Set old to the value returned by pci_vdev_read_cfg with vdev, offset and bytes being the
		 *  parameters, which reads a bytes value from the configuration space of the given vPCI device.
		 */
		old = pci_vdev_read_cfg(vdev, offset, bytes);
		/** Call pci_vdev_write_cfg with the following parameters, in order to write 'active_val' into the
		 *  given vPCI configuration space, where active_val is calculated by
		 *  (old & ro_mask) | (val & ~ro_mask).
		 * - vdev
		 * - offset
		 * - bytes
		 * - (old & ro_mask) | (val & ~ro_mask)
		 */
		pci_vdev_write_cfg(vdev, offset, bytes, (old & ro_mask) | (val & ~ro_mask));

		/** Set msgctrl to the value returned by pci_vdev_read_cfg with vdev, vdev->msi.capoff + PCIR_MSI_CTRL
		 *  and 2 being the parameters to get the MSI control register status, which includes the bit of
		 *  enabling or disabling MSI.
		 */
		msgctrl = pci_vdev_read_cfg(vdev, vdev->msi.capoff + PCIR_MSI_CTRL, 2U);
		/** If the enabling MSI bit in msgctrl is set by guest VM */
		if ((msgctrl & PCIM_MSICTRL_MSI_ENABLE) != 0U) {
			/** Call remap_vmsi with the following parameters, in order to remap the virtual MSI
			 *  data/address to the phyiscal MSI data/address.
			 *  - vdev
			 */
			remap_vmsi(vdev);
		}
	}
}

/**
 * @brief Deinit the virtual MSI of the given vPCI device to unmap its MSI
 *
 * This function is called to deinit the virtual MSI of the given vPCI device to unmap its MSI.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by vpci_deinit_pt_dev.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
void deinit_vmsi(const struct pci_vdev *vdev)
{
	/** If there is MSI capability in the given vPCI device */
	if (has_msi_cap(vdev)) {
		/** Call ptirq_remove_msix_remapping with the following parameters, in order to remove the mapping of
		 *  the virtual MSI data/address to the physical MSI data/address.
		 *  - vdev->vpci->vm
		 *  - vdev->bdf.value
		 *  - 1
		 */
		ptirq_remove_msix_remapping(vdev->vpci->vm, vdev->bdf.value, 1U);
	}
}

/**
 * @brief Initialize the virtual MSI of the given vPCI device
 *
 * This function is called to initialize the virtual MSI of the given vPCI device. It includes the MSI capability
 * registers in the virtual configuration space and its MSI information structure.
 *
 * @param[inout] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init_pt_dev.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
void init_vmsi(struct pci_vdev *vdev)
{
	/** Declare the following local variables of type uint32_t.
	 *  - val representing a register value in the configuration space of the given vPCI, not initialized. */
	uint32_t val;
	/** Declare the following local variables of type uint32_t.
	 *  - msi_data_addr representing the MSI data register address in the configuration space of the given
	 *  vPCI, not initialized. */
	uint32_t msi_data_addr;
	/** Set val to the value returned by pci_vdev_read_cfg with vdev, PCIR_CAP_PTR and 1 being the parameters,
	 *  which reads the register of PCIR_CAP_PTR to get the offset of the first capability structure.
	 */
	val = pci_vdev_read_cfg(vdev, PCIR_CAP_PTR, 1U);
	/** If 'val' (the offset) is valid, i.e. not equals 0 or FFH */
	if ((val != 0U) && (val != 0xFFU)) {
		/** Declare the following local variables of type uint8_t.
		 *  - cap representing a capability ID, initialized as the value returned by pci_vdev_read_cfg with
		 *  vdev, val + PCICAP_ID and 1 being the parameters, which reads the capability ID from its first
		 *  capability.
		 */
		uint8_t cap = pci_vdev_read_cfg(vdev, val + PCICAP_ID, 1U);
		/** If the capability ID is PCIY_MSI */
		if (cap == PCIY_MSI) {
			/** Set vdev->msi.capoff to 'val', the offset of the MSI capability structure. */
			vdev->msi.capoff = val;
		}
	}

	/** If the given vPCI device has the MSI capability (determined by calling has_msi_cap with the device) */
	if (has_msi_cap(vdev)) {
		/** Set val to the value returned by pci_vdev_read_cfg with vdev, vdev->msi.capoff and 4 being the
		 *  parameters,, which reads a 32bits value (including the MSI control register value) from the
		 *  configuration space of the given vPCI device.
		 */
		val = pci_vdev_read_cfg(vdev, vdev->msi.capoff, 4U);
		/** Set vdev->msi.is_64bit to true, if the MSI control register indicates it uses a 64bits message
		 *  address. Otherwise set the field to false.
		 */
		vdev->msi.is_64bit = ((val & (PCIM_MSICTRL_64BIT << 16U)) != 0U);
		/** Set vdev->msi.caplen to 14 if its message address is 64bits, or to 10. */
		vdev->msi.caplen = vdev->msi.is_64bit ? 14U : 10U;

		/** Bitwise AND val by ~((uint32_t)PCIM_MSICTRL_MMC_MASK << 16U), to set
		 *  "Multiple Message Capable" field to 0, which indicates this PCI device just supports 1 vector.
		 */
		val &= ~((uint32_t)PCIM_MSICTRL_MMC_MASK << 16U);
		/** Bitwise AND val by ~((uint32_t)PCIM_MSICTRL_MME_MASK << 16U), to set the field of
		 *  "Multiple Message Enable" to 0, which indicates just 1 vector allocated.
		 */
		val &= ~((uint32_t)PCIM_MSICTRL_MME_MASK << 16U);
		/** Call pci_vdev_write_cfg with the following parameters, in order to write 'val' into the
		 *  given vPCI configuration space to update the virtual MSI capability structure.
		 * - vdev
		 * - vdev->msi.capoff
		 * - 4
		 * - val
		 */
		pci_vdev_write_cfg(vdev, vdev->msi.capoff, 4U, val);

		/** Call pci_vdev_write_cfg with the following parameters, in order to write MSG_INITIAL_VALUE into the
		 *  given vPCI configuration space to update the virtual MSI capability structure.
		 * - vdev
		 * - vdev->msi.capoff + PCIR_MSI_ADDR
		 * - 4
		 * - MSG_INITIAL_VALUE
		 */
		pci_vdev_write_cfg(vdev, vdev->msi.capoff + PCIR_MSI_ADDR, 4U, MSG_INITIAL_VALUE);

		/** If MSI is 64bit */
		if (vdev->msi.is_64bit) {
			/** Set msi_data_addr to the value, where the value is calculated by plusing
			 *  PCIR_MSI_DATA_64BIT and msi.capoff */
			msi_data_addr = vdev->msi.capoff + PCIR_MSI_DATA_64BIT;
		} else {
			/** Set msi_data_addr to the value, where the value is calculated by plusing
			 *  PCIR_MSI_DATA and msi.capoff */
			msi_data_addr = vdev->msi.capoff + PCIR_MSI_DATA;
		}

		/** Set val to the value returned by pci_vdev_read_cfg with vdev, msi_data_addr, and 4 being the
		 *  parameters, in order to read the register of the msi_data_addr to get the value of MSI data
		 *  register. */
		val = pci_vdev_read_cfg(vdev, msi_data_addr, 4U);
		/* Clear the bit MSI_DATA_TRIGGER_MODE and MSI_DATA_LEVEL_TRIGGER_MODE. Clear all of the bits in
		 * MSI_DATA_DELIVER_MODE_MASK. */
		val &= (~(MSI_DATA_TRIGGER_MODE | MSI_DATA_LEVEL_TRIGGER_MODE | MSI_DATA_DELIVER_MODE_MASK));
		/* Set the bit MSI_DATA_LEVEL_TRIGGER_MODE of val */
		val |= MSI_DATA_LEVEL_TRIGGER_MODE;
		/** Call pci_vdev_write_cfg with the following parameters, in order to write val into the
		 *  given vPCI configuration space to update the virtual MSI capability structure.
		 * - vdev
		 * - msi_data_addr
		 * - 4
		 * - val
		 */
		pci_vdev_write_cfg(vdev, msi_data_addr, 4U, val);
	}
}

/**
 * @}
 */
