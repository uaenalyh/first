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

#ifndef VPCI_PRIV_H_
#define VPCI_PRIV_H_

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file implements a few of helper APIs used within vPCI component
 *
 * This file implements a few of helper APIs used within vPCI component. Also it delcares the internal APIs used
 * within vPCI component.
 */

#include <types.h>
#include <pci.h>
#include <vpci.h>

/**
 * @brief Check whether a number is within a specific range
 *
 * This function is called to check whether a given number is within a given specific range.
 *
 * @param[in] value The number to be checked.
 * @param[in] lower The low end of the range
 * @param[in] len The length of the range
 *
 * @return True if the value is the range, or return false.
 *
 * @pre None
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool in_range(uint32_t value, uint32_t lower, uint32_t len)
{
	/** Return true if the value in the range: lower <= value < (low + len), or return false. */
	return ((value >= lower) && (value < (lower + len)));
}

/**
 * @brief Read a byte from the configuration space of the given vPCI device
 *
 * This function is called to read a byte from the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to read.
 * @param[in] offset The register offset in the configuration space.
 *
 * @return The register's value as a byte length
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline uint8_t pci_vdev_read_cfg_u8(const struct pci_vdev *vdev, uint32_t offset)
{
	/** Return vdev->cfgdata.data_8[offset], a byte value in the virtual configuration
	 *  space of the given vPCI device.
	 */
	return vdev->cfgdata.data_8[offset];
}

/**
 * @brief Read a word (2 bytes) from the configuration space of the given vPCI device
 *
 * This function is called to read a word (2 bytes) from the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to read.
 * @param[in] offset The register offset in the configuration space.
 *
 * @return The register's value as a word length
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline uint16_t pci_vdev_read_cfg_u16(const struct pci_vdev *vdev, uint32_t offset)
{
	/** Return vdev->cfgdata.data_16[offset >> 1], a word (2 bytes) value in the virtual configuration
	 *  space of the given vPCI device.
	 */
	return vdev->cfgdata.data_16[offset >> 1U];
}

/**
 * @brief Read a DWORD (4 bytes) from the configuration space of the given vPCI device
 *
 * This function is called to read a DWORD (4 bytes) from the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to read.
 * @param[in] offset The register offset in the configuration space.
 *
 * @return The register's value as a DWORD length
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline uint32_t pci_vdev_read_cfg_u32(const struct pci_vdev *vdev, uint32_t offset)
{
	/** Return vdev->cfgdata.data_32[offset >> 2], a DWORD (4 bytes) value in the virtual configuration
	 *  space of the given vPCI device.
	 */
	return vdev->cfgdata.data_32[offset >> 2U];
}

/**
 * @brief Write a byte to the configuration space of the given vPCI device
 *
 * This function is called to write a byte to the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to write.
 * @param[in] offset The register offset in the configuration space.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static inline void pci_vdev_write_cfg_u8(struct pci_vdev *vdev, uint32_t offset, uint8_t val)
{
	/** Set vdev->cfgdata.data_8[offset] to val */
	vdev->cfgdata.data_8[offset] = val;
}

/**
 * @brief Write a word (2 bytes) to the configuration space of the given vPCI device
 *
 * This function is called to a word (2 bytes) to the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to write.
 * @param[in] offset The register offset in the configuration space.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static inline void pci_vdev_write_cfg_u16(struct pci_vdev *vdev, uint32_t offset, uint16_t val)
{
	/** Set vdev->cfgdata.data_16[offset >> 1U] to val */
	vdev->cfgdata.data_16[offset >> 1U] = val;
}

/**
 * @brief Write a DWORD (4 bytes) to the configuration space of the given vPCI device
 *
 * This function is called to a DWORD (4 bytes) to the configuration space of the given vPCI device.
 *
 * @param[in] vdev A vPCI device whose configuration space is to write.
 * @param[in] offset The register offset in the configuration space.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static inline void pci_vdev_write_cfg_u32(struct pci_vdev *vdev, uint32_t offset, uint32_t val)
{
	/** Set vdev->cfgdata.data_32[offset >> 2U] to val */
	vdev->cfgdata.data_32[offset >> 2U] = val;
}

/**
 * @brief Check whether a register is one of the BAR registers of the given vPCI device
 *
 * This function is called to check whether a register is one of the BAR registers of the given vPCI device
 *
 * @param[in] vdev A vPCI device whose BAR registers range is used to check.
 * @param[in] offset The register offset in the configuration space.
 *
 * @return True if the register is the BAR registers' range, or return false.
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool vbar_access(const struct pci_vdev *vdev, uint32_t offset)
{
	/** Declare the following local variables of type bool.
	 *  - ret representing the result of checking, initialized as false. */
	bool ret = false;

	/** If the offset is within the range of the BAR registers of the given vPCI device, with the return values
	 *  of calls to pci_bar_offset(0) and pci_bar_offset(vdev->nr_bars) being the begin (inclusive) and end
	 *  (exclusive).
	 */
	if ((offset >= pci_bar_offset(0U)) && (offset < pci_bar_offset(vdev->nr_bars))) {
		/** Set ret to true */
		ret = true;
	}

	/** Return ret, the checked result */
	return ret;
}

/**
 * @brief Check whether the given vPCI device has a MSI capability
 *
 * This function is called to check whether the given vPCI device has a MSI capability. If it has a MSI capability, the
 * offset of the MSI capability structure should not be 0.
 *
 * @param[in] vdev A vPCI device whose MSI info is to check
 *
 * @return True if the given vPCI device has a MSI capability, or return false.
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool has_msi_cap(const struct pci_vdev *vdev)
{
	/** Return true if vdev->msi.capoff is not 0, or return false. */
	return (vdev->msi.capoff != 0U);
}

/**
 * @brief Check whether a register is within the range of the given vPCI device's MSI capability structure.
 *
 * This function is called to check whether a register is within the range of the given vPCI device's MSI capability
 * structure.
 *
 * @param[in] vdev A vPCI device whose MSI capability structure is to check
 * @param[in] offset The register offset in the configuration space.
 *
 * @return True if the register is in the range of the given vPCI device's MSI capability structure, or return false.
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal helper function and shall be called after the vPCI device initialized.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool msicap_access(const struct pci_vdev *vdev, uint32_t offset)
{
	/** Return true if the given vPCI device has a MSI capability (determined by calling has_msi_cap with vdev)
	 *  and the given register's offset in the range of its MSI capability structure (determined by calling
	 *  in_range with offset, vdev->msi.capoff and vdev->msi.caplen).
	 */
	return (has_msi_cap(vdev) && in_range(offset, vdev->msi.capoff, vdev->msi.caplen));
}

void init_vdev_pt(struct pci_vdev *vdev);
void vdev_pt_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val);

void init_vmsi(struct pci_vdev *vdev);
void vmsi_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vmsi(const struct pci_vdev *vdev);

uint32_t pci_vdev_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes);
void pci_vdev_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);

struct pci_vdev *pci_find_vdev(struct acrn_vpci *vpci, union pci_bdf vbdf);

uint32_t pci_vdev_read_bar(const struct pci_vdev *vdev, uint32_t idx);
void pci_vdev_write_bar(struct pci_vdev *vdev, uint32_t idx, uint32_t val);

/**
 * @}
 */

#endif /* VPCI_PRIV_H_ */
