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

/*_
 * Emulate a PCI Host bridge:
 * Intel Corporation Celeron N3350/Pentium N4200/Atom E3900
 * Series Host Bridge (rev 0b)
 */

#include <vm.h>
#include <pci.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all virtual hostbridge operation APIs that shall be provided by vhostbridge.
 *
 * This file implements all virtual hostbridge operation APIs that shall be provided by vhostbridge as part of
 * the vPCI component. The vhostbridge is a pure virtual hostbridge, which is not related with the physical hostbridge.
 * All the APIs in this file is provided as callback members of "struct pci_vdev_ops".
 *
 * Following functions are included:
 * - init_vhostbridge: initialize the virtual hostbridge configuration space.
 * - deinit_vhostbridge: de-init function, just an empty function, no specific operation.
 * - vhostbridge_read_cfg: read the virtual hostbridge register from its configuration space.
 * - vhostbridge_write_cfg: write configuration register, just ignore the operation
 */

/**
 * @brief Initialize the virtual hostbridge configuration space.
 *
 * This function is called to initialize the virtual hostbridge configuration space. It sets the main registers to
 * indicate it is specific PCI hostbridge device without any capability.
 *
 * @param[in] vdev A vPCI device which will be initialized as a virtual hostbridge.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init_vdev.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void init_vhostbridge(struct pci_vdev *vdev)
{
	/** Call pci_vdev_write_cfg_u16 with the following parameters, in order to write the vendor ID register in the
	 *  virtual configuration space of the given vPCI device.
	 *  - vdev
	 *  - PCIR_VENDOR
	 *  - 0x8086U
	 */
	pci_vdev_write_cfg_u16(vdev, PCIR_VENDOR, (uint16_t)0x8086U);
	/** Call pci_vdev_write_cfg_u16 with the following parameters, in order to write the device ID register in the
	 *  virtual configuration space of the given vPCI device.
	 *  - vdev
	 *  - PCIR_DEVICE
	 *  - 0x5af0U
	 */
	pci_vdev_write_cfg_u16(vdev, PCIR_DEVICE, (uint16_t)0x5af0U);

	/** Call pci_vdev_write_cfg_u8 with the following parameters, in order to write the revision ID register in
	 *  the virtual configuration space of the given vPCI device.
	 *  - vdev
	 *  - PCIR_REVID
	 *  - 0xbU
	 */
	pci_vdev_write_cfg_u8(vdev, PCIR_REVID, (uint8_t)0xbU);

	/** Call pci_vdev_write_cfg_u8 with the following parameters, in order to write the header type register in
	 *  the virtual configuration space of the given vPCI device.
	 *  - vdev
	 *  - PCIR_HDRTYPE
	 *  - PCIM_HDRTYPE_NORMAL
	 */
	pci_vdev_write_cfg_u8(vdev, PCIR_HDRTYPE, (uint8_t)PCIM_HDRTYPE_NORMAL);
	/** Call pci_vdev_write_cfg_u8 with the following parameters, in order to write the class ID register in
	 *  the virtual configuration space of the given vPCI device. The ID indicates it is bridge class.
	 *  - vdev
	 *  - PCIR_CLASS
	 *  - PCIC_BRIDGE
	 */
	pci_vdev_write_cfg_u8(vdev, PCIR_CLASS, (uint8_t)PCIC_BRIDGE);
	/** Call pci_vdev_write_cfg_u8 with the following parameters, in order to write the subclass ID register in
	 *  the virtual configuration space of the given vPCI device. The ID indicates it is host bridge device.
	 *  - vdev
	 *  - PCIR_SUBCLASS
	 *  - PCIS_BRIDGE_HOST
	 */
	pci_vdev_write_cfg_u8(vdev, PCIR_SUBCLASS, (uint8_t)PCIS_BRIDGE_HOST);
}

/**
 * @brief Deinit the virtual hostbridge device.
 *
 * This function is called to deinit the virtual hostbridge device. It does nothing.
 *
 * @param[in] vdev A vPCI device which is a virtual hostbridge
 *
 * @return N/A
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by vpci_cleanup.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void deinit_vhostbridge(__unused struct pci_vdev *vdev)
{
}

/**
 * @brief Read the register in the configuration space of the given virtual hostbridge device.
 *
 * This function is called to read the register in the configuration space of the given virtual hostbridge device.
 *
 * @param[in] vdev A vPCI device which is a virtual hostbridge
 * @param[in] offset The register offset in the configuration space.
 * @param[in] bytes The length of the register to read.
 * @param[out] val The pointer to save the value to read.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by read_cfg.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void vhostbridge_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/** Set '*val' to the value returned by pci_vdev_read_cfg, which reads the register in the configuration
	 *  space of the given virtual hostbridge device.
	 */
	*val = pci_vdev_read_cfg(vdev, offset, bytes);
}

/**
 * @brief Write the register in the configuration space of the given virtual hostbridge device.
 *
 * This function is called to write the register in the configuration space of the given virtual hostbridge device.
 * For the virtual hostbridge, there are no capability supported, so just ignore the writing operation.
 *
 * @param[in] vdev A vPCI device which is a virtual hostbridge
 * @param[in] offset The register offset in the configuration space.
 * @param[in] bytes The length of the register to write.
 * @param[in] val The value to write.
 *
 * @return N/A
 *
 * @pre vdev != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by write_cfg.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev is different among parallel invocation.
 */
static void vhostbridge_write_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
}

/**
* @brief A set of callback functions used to operate the virtual hostbridge device.
**/
const struct pci_vdev_ops vhostbridge_ops = {
	.init_vdev = init_vhostbridge,   /**< The function to initialize the virtual hostbridge. */
	.deinit_vdev = deinit_vhostbridge,   /**< The function to deinit the virtual hostbridge. */
	.write_vdev_cfg = vhostbridge_write_cfg,  /**< The function to write the configuration register. */
	.read_vdev_cfg = vhostbridge_read_cfg,  /**< The function to read the configuration register. */
};

/**
 * @}
 */
