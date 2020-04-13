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
#include "vpci_priv.h"
#include <ept.h>
#include <logmsg.h>

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file implements some internal APIs which are used within vPCI component
 *
 * This file implements some internal functions which are used within vPCI component. It also defines one helper
 * function to implement the features that are commonly used in this file. In addition, it defines one decomposed
 * function to improve the readability of the code.
 *
 * Following functions are internal APIs used by other source files within vPCI:
 * - pci_vdev_read_cfg / pci_vdev_write_cfg: the vPCI configuration space read/write functions
 * - pci_find_vdev: to find the vPCI device from the device list according to its BDF
 * - pci_vdev_read_bar / pci_vdev_write_bar: BAR registers read / write functions
 *
 * Helper functions:
 * - vbar_base_is_valid: check whether the BAR register's base and size is valid, called by pci_vdev_update_bar_base
 *
 * Decomposed functions:
 * - pci_vdev_update_bar_base: to update the BAR info in vPCI, called by pci_vdev_write_bar
 */


/**
 * @brief Read a configuration register from the virtual configuration space of the given vPCI device
 *
 * This function is called to read a configuration register from the virtual configuration space of the given vPCI
 * device. The length of the register should be 1 or 2 or 4 bytes.
 *
 * @param[in] vdev A vPCI device
 * @param[in] offset The register offset in the PCI configure space.
 * @param[in] bytes The length of the register to read.
 *
 * @return The register value corresponding with the given 'offset' in the vPCI virtual configuration space.
 *
 * @pre vdev != NULL
 * @pre offset < PCI_REGMAX
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
uint32_t pci_vdev_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes)
{
	/** Declare the following local variables of type uint32_t.
	 *  - val representing the register value to read, not initialized. */
	uint32_t val;

	/** Depending on the given 'bytes' to read */
	switch (bytes) {
	/** '1' means to read one byte from the vPCI's configuration space */
	case 1U:
		/** Set val to the value returned by pci_vdev_read_cfg_u8 with vdev and offset being the parameters,
		 *  which is to read one byte from the configuration space.
		 */
		val = pci_vdev_read_cfg_u8(vdev, offset);
		/** End of case */
		break;
		/** '2' means to read two bytes (a word) from the vPCI's configuration space */
	case 2U:
		/** Set val to the value returned by pci_vdev_read_cfg_u16 with vdev and offset being the parameters,
		 *  which is to read two bytes from the configuration space.
		 */
		val = pci_vdev_read_cfg_u16(vdev, offset);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Set val to the value returned by pci_vdev_read_cfg_u32 with vdev and offset being the parameters,
		 *  which is to read four bytes from the configuration space.
		 */
		val = pci_vdev_read_cfg_u32(vdev, offset);
		/** End of case */
		break;
	}

	/** Return val, the configuration register's value */
	return val;
}

/**
 * @brief Write a register value to the virtual configuration space of the given vPCI device
 *
 * This function is called to write a register value to the virtual configuration space of the given vPCI device.
 * It will write a register with the length of 1 or 2 or 4 bytes.
 *
 * @param[inout] vdev A vPCI device
 * @param[in] offset The register offset in the PCI configure space.
 * @param[in] bytes The length of the register to write.
 * @param[in] val The value of the register to write.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 * @pre offset < PCI_REGMAX
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
void pci_vdev_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/** Depending on the number of the given 'bytes' */
	switch (bytes) {
		/** '1' means to write one byte to the vPCI's configuration space */
	case 1U:
		/** Call pci_vdev_write_cfg_u8 with the following parameters, in order to write one byte to the given
		 *  vPCI's virtual configuration space.
		 *  - vdev
		 *  - offset
		 *  - val
		 */
		pci_vdev_write_cfg_u8(vdev, offset, (uint8_t)val);
		/** End of case */
		break;
		/** '2' means to write two bytes (a word) to the vPCI's configuration space */
	case 2U:
		/** Call pci_vdev_write_cfg_u16 with the following parameters, in order to write two bytes (a word) to
		 *  the given vPCI's virtual configuration space.
		 *  - vdev
		 *  - offset
		 *  - val
		 */
		pci_vdev_write_cfg_u16(vdev, offset, (uint16_t)val);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Call pci_vdev_write_cfg_u32 with the following parameters, in order to write four bytes to
		 *  the given vPCI's virtual configuration space.
		 *  - vdev
		 *  - offset
		 *  - val
		 */
		pci_vdev_write_cfg_u32(vdev, offset, val);
		/** End of case */
		break;
	}
}

/**
 * @brief Find a vPCI device from the vPCI devices list according to the given virtual BDF
 *
 * This function is called to find a vPCI device from the vPCI devices list according to the given virtual BDF.
 *
 * @param[in] vpci A pointer to the vPCI device list.
 * @param[in] vbdf The virtual BDF of the vPCI device to find.
 *
 * @return The vPCI device whose virtual BDF is matched with the given one if found, or return NULL.
 *
 * @pre vpci != NULL
 *
 * @pre vpci->pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
struct pci_vdev *pci_find_vdev(struct acrn_vpci *vpci, union pci_bdf vbdf)
{
	/** Declare the following local variables of type 'struct pci_vdev *'.
	 *  - vdev representing the vPCI device to be found, initialized as NULL.
	 *  - tmp representing a vPCI device in the devices list, initialized as NULL.
	 */
	struct pci_vdev *vdev = NULL, *tmp = NULL;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the index to probe the vPCI device, not initialized. */
	uint32_t i;

	/** For each i ranging from 0 to vpci->pci_vdev_cnt - 1 [with a step of 1] */
	for (i = 0U; i < vpci->pci_vdev_cnt; i++) {
		/** Set tmp to &(vpci->pci_vdevs[i]), the pointer to one vPCI device from the devices list */
		tmp = &(vpci->pci_vdevs[i]);

		/** If the probed vPCI device's virtual BDF equals the given one */
		if (tmp->bdf.value == vbdf.value) {
			/** Set vdev to tmp, which is the target vPCI device */
			vdev = tmp;
			/** End of case */
			break;
		}
	}

	/** Return vdev, which is the found vPCI device or NULL */
	return vdev;
}

/**
 * @brief Read a BAR register of the given vPCI device according to the given BAR index.
 *
 * This function is called to read a BAR register of the given vPCI device according to the given BAR index.
 * Generally, it will return the value of the BAR register, but if the value is 0FFFFFFFFH, it will return the size
 * of the BAR.
 *
 * @param[in] vdev A vPCI device whose BAR is to read.
 * @param[in] idx The BAR index.
 *
 * @return The BAR register's value or return the BAR size if value is 0FFFFFFFFH
 *
 * @pre vdev != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by vpci_read_pt_dev_cfg
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
uint32_t pci_vdev_read_bar(const struct pci_vdev *vdev, uint32_t idx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - bar representing the value of the BAR register, not initialized.
	 *  - offset representing the position of the BAR register in PCI configuration space, not initialized.
	 */
	uint32_t bar, offset;

	/** Set offset to the value returned by pci_bar_offset with idx being the parameter, which is to get the
	 *  position of the BAR register in PCI configuration space according to the given BAR index.
	 */
	offset = pci_bar_offset(idx);
	/** Set bar to the value returned pci_vdev_read_cfg_u32 with vdev and offset being the parameters, which is to
	 *  get the 4 bytes register value from the virtual configuration space according to the given vPCI device and
	 *  its register offset.
	 */
	bar = pci_vdev_read_cfg_u32(vdev, offset);
	/** If 'bar' is 0FFFFFFFFH, which is set by BAR writing in order to get its BAR size */
	if (bar == ~0U) {
		/** Set bar to vdev->bar[idx].mask, which is the BAR size. */
		bar = vdev->bar[idx].mask;
	}
	/** Return the BAR register's value or the BAR size */
	return bar;
}

#define PCI_VBAR_BASE_LIMIT 0xC0000000UL /**< Pre-defined the minimum base of the BAR MMIO space range. */
#define PCI_VBAR_TOP_LIMIT  0xE0000000UL /**< Pre-defined the maximum top of the BAR MMIO space range. */

/**
 * @brief Check whether a BAR base is valid
 *
 * This function is called to check whether a BAR base is valid according to the given BAR base address and its size.
 * The BAR MMIO space range should be limited between PCI_VBAR_BASE_LIMIT and PCI_VBAR_TOP_LIMIT.
 *
 * @param[in] bar_base The base address of the BAR to check
 * @param[in] size The size of a BAR to check
 *
 * @return True if the BAR range is valid, of return false.
 *
 * @pre None
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by pci_vdev_update_bar_base
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static bool vbar_base_is_valid(uint64_t bar_base, uint64_t size)
{
	/** Declare the following local variables of type bool.
	 *  - result representing the check result to be returned, initialized as true. */
	bool result = true;

	/** If BAR MMIO range is not between PCI_VBAR_BASE_LIMIT and PCI_VBAR_TOP_LIMIT */
	if ((bar_base < PCI_VBAR_BASE_LIMIT) || ((bar_base + size) > PCI_VBAR_TOP_LIMIT)) {
		/** Set result to false */
		result = false;
	}

	/** Return the check result */
	return result;
}

/**
 * @brief Update a BAR base address info of the given vPCI device
 *
 * This function is called to update a BAR base address info of the given vPCI device. It will check the BAR type and
 * get the BAR register value from the configuration space of the given vPCI device first, and then calcaulate the base
 * address value and update to its field of 'struct pci_bar'.
 *
 * @param[inout] vdev A vPCI device whose BAR base info is to update.
 * @param[in] idx The BAR index.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by pci_vdev_write_bar.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void pci_vdev_update_bar_base(struct pci_vdev *vdev, uint32_t idx)
{
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing the pointer to the BAR got from BAR array as to the given index, not initialized. */
	struct pci_bar *vbar;
	/** Declare the following local variables of type uint64_t.
	 *  - base representing base address of the BAR whose index is the given one, initialized as 0. */
	uint64_t base = 0UL;
	/** Declare the following local variables of type uint32_t.
	 *  - lo representing the low 32bits value of the BAR base if it is a 64bits BAR, not initialized.
	 *  - hi representing the high 32bits value of the BAR base if it is a 64bits BAR, not initialized.
	 *  - offset representing the position of the BAR in its configuration space, not initialized.
	 */
	uint32_t lo, hi, offset;

	/** Set vbar to &vdev->bar[idx] */
	vbar = &vdev->bar[idx];
	/** Set offset to the value returned by pci_bar_offset with idx being the parameter, which gets the BAR
	 *  offset in the configuration space according to the given index.
	 */
	offset = pci_bar_offset(idx);
	/** Set lo to the value returned by pci_vdev_read_cfg_u32 with vdev and offset being the parameters,
	 *  which reads the BAR register value.
	 */
	lo = pci_vdev_read_cfg_u32(vdev, offset);
	/** If vbar->type is not PCIBAR_NONE and 'lo' is not 0FFFFFFFFH */
	if ((vbar->type != PCIBAR_NONE) && (lo != ~0U)) {
		/** Set base to 'lo & vbar->mask', which sets the low 4bits to 0 and keeps the base MMIO address */
		base = lo & vbar->mask;

		/** If the it is a 64bits BAR, which includes two 32bits BAR */
		if (vbar->type == PCIBAR_MEM64) {
			/** Set vbar to next BAR (high 32bits) */
			vbar = &vdev->bar[idx + 1U];
			/** Set hi to the value returned by pci_vdev_read_cfg_u32 with vdev and offset + 4 being the
			 *  parameters, which is to get the high 32bits register value.
			 */
			hi = pci_vdev_read_cfg_u32(vdev, offset + 4U);
			/** If hi is not 0FFFFFFFFH, which means it is a valid high 32bits MMIO address */
			if (hi != ~0U) {
				/** Bitwise AND hi by vbar->mask to the base of high 32bits value */
				hi &= vbar->mask;
				/** Bitwise OR base by 'hi << 32', which moves the high 32bits value into 'base' */
				base |= ((uint64_t)hi << 32U);
			} else {
				/** Set base to 0, for the high 32bits is invalid MMIO address (GPA of the BAR base) */
				base = 0UL;
			}
		}
		/** If vbar->type is PCIBAR_IO_SPACE, which means it is a IO BAR and not supported here */
		if (vbar->type == PCIBAR_IO_SPACE) {
			/** Bitwise AND base by 0FFFFH */
			base &= 0xffffUL;
		}
	}

	/** If 'base' is not 0, but the address range from 'base' with a size of 'vdev->bar[idx].size' is not a valid
	 *  MMIO address range for PCI
	 */
	if ((base != 0UL) && !vbar_base_is_valid(base, vdev->bar[idx].size)) {
		/** Logging the following information with a log level of LOG_FATAL.
		 *  - "%s, %x:%x.%x set invalid bar[%d] base: 0x%lx, size: 0x%lx\n"
		 *  - __func__
		 *  - vdev->bdf.bits.b
		 *  - vdev->bdf.bits.d
		 *  - vdev->bdf.bits.f
		 *  - idx
		 *  - base
		 *  - vdev->bar[idx].size
		 */
		pr_fatal("%s, %x:%x.%x set invalid bar[%d] base: 0x%lx, size: 0x%lx\n", __func__, vdev->bdf.bits.b,
			vdev->bdf.bits.d, vdev->bdf.bits.f, idx, base, vdev->bar[idx].size);
		/** Set base to 0, which means it is a invalid GPA of BAR base, and will be ignored temporarily */
		base = 0UL;
	}

	/** Set vdev->bar[idx].base to 'base', which could be remapped to the HPA of BAR base */
	vdev->bar[idx].base = base;
}

/**
 * @brief Write a BAR register to the configuration space of the given vPCI
 *
 * This function is called to write a BAR register to the configuration space of the given vPCI. It will write
 * the given value to the BAR register, and update the BAR base info.
 *
 * @param[inout] vdev A vPCI device whose BAR register is to write
 * @param[in] idx The BAR index.
 * @param[in] val The value to write to the BAR register.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 * @pre 0 <= idx && idx <= 5
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal API called by other vPCI source file.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
void pci_vdev_write_bar(struct pci_vdev *vdev, uint32_t idx, uint32_t val)
{
	/** Declare the following local variables of type 'struct pci_bar *'.
	 *  - vbar representing the pointer to the BAR got from BAR array as to the given index, not initialized. */
	struct pci_bar *vbar;
	/** Declare the following local variables of type uint32_t.
	 *  - bar representing the register value of the BAR, not initialized.
	 *  - offset representing the BAR position in the configuration space, not initialized.
	 */
	uint32_t bar, offset;
	/** Declare the following local variables of type uint32_t.
	 *  - update_idx representing the index of the BAR whose base address is to update, initialized as 'idx'. */
	uint32_t update_idx = idx;

	/** Set vbar to &vdev->bar[idx] */
	vbar = &vdev->bar[idx];
	/** Set bar to 'val & vbar->mask', which gets the base address as to the given base value */
	bar = val & vbar->mask;
	/** Bitwise OR bar by vbar->fixed, which includes a full info of the BAR register. */
	bar |= vbar->fixed;
	/** Set offset to the value returned by pci_bar_offset with idx being the parameter, which gets the BAR
	 *  offset in the configuration space according to the given BAR index.
	 */
	offset = pci_bar_offset(idx);
	/** Call pci_vdev_write_cfg_u32 with the following parameters, in order to write the BAR register into the
	 *  configuration space of the given vPCI device.
	 *  - vdev
	 *  - offset
	 *  - bar
	 */
	pci_vdev_write_cfg_u32(vdev, offset, bar);

	/** If vbar->type is a 64bits BAR */
	if (vbar->type == PCIBAR_MEM64HI) {
		/** Decrement update_idx by 1, for a 64bits BAR, it need update its BAR base address according to its
		 *  low 32bits register and high 32bits register in pair.
		 */
		update_idx -= 1U;
	}

	/** Call pci_vdev_update_bar_base with the following parameters, in order to update the base address info
	 *  of the given vPCI device's BAR.
	 *  - vdev
	 *  - update_idx
	 */
	pci_vdev_update_bar_base(vdev, update_idx);
}

/**
 * @}
 */
