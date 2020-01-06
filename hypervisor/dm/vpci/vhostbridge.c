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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static void init_vhostbridge(struct pci_vdev *vdev)
{
	/* PCI config space */
	pci_vdev_write_cfg_u16(vdev, PCIR_VENDOR, (uint16_t)0x8086U);
	pci_vdev_write_cfg_u16(vdev, PCIR_DEVICE, (uint16_t)0x5af0U);

	pci_vdev_write_cfg_u8(vdev, PCIR_REVID, (uint8_t)0xbU);

	pci_vdev_write_cfg_u8(vdev, PCIR_HDRTYPE, (uint8_t)PCIM_HDRTYPE_NORMAL);
	pci_vdev_write_cfg_u8(vdev, PCIR_CLASS, (uint8_t)PCIC_BRIDGE);
	pci_vdev_write_cfg_u8(vdev, PCIR_SUBCLASS, (uint8_t)PCIS_BRIDGE_HOST);
}

static void deinit_vhostbridge(__unused struct pci_vdev *vdev)
{
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static int32_t vhostbridge_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_cfg(vdev, offset, bytes);
	return 0;
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static int32_t vhostbridge_write_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	return 0;
}

const struct pci_vdev_ops vhostbridge_ops = {
	.init_vdev = init_vhostbridge,
	.deinit_vdev = deinit_vhostbridge,
	.write_vdev_cfg = vhostbridge_write_cfg,
	.read_vdev_cfg = vhostbridge_read_cfg,
};

/**
 * @}
 */
