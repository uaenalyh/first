/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
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
 *
 * Only support Type 0 and Type 1 PCI(e) device. Remove PC-Card type support.
 *
 */
#include <types.h>
#include <spinlock.h>
#include <io.h>
#include <pci.h>
#include <uart16550.h>
#include <logmsg.h>
#include <pci_dev.h>
#include <vtd.h>
#include <bits.h>
#include <board.h>

static spinlock_t pci_device_lock;

/* @brief: Find the DRHD index corresponding to a PCI device
 * Runs through the pci_pdev_array and returns the value in drhd_idx
 * member from pdev structure that matches matches B:D.F
 *
 * @pbdf[in]	B:D.F of a PCI device
 *
 * @return if there is a matching pbdf in pci_pdev_array, pdev->drhd_idx, else INVALID_DRHD_INDEX
 */

static uint32_t pci_pdev_calc_address(union pci_bdf bdf, uint32_t offset)
{
	uint32_t addr = (uint32_t)bdf.value;

	addr <<= 8U;
	addr |= (offset | PCI_CFG_ENABLE);
	return addr;
}

uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	uint32_t addr;
	uint32_t val;

	addr = pci_pdev_calc_address(bdf, offset);

	spinlock_obtain(&pci_device_lock);

	/* Write address to ADDRESS register */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/* Read result from DATA register */
	switch (bytes) {
	case 1U:
		val = (uint32_t)pio_read8((uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		val = (uint32_t)pio_read16((uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		val = pio_read32((uint16_t)PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);

	return val;
}

void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t addr;

	spinlock_obtain(&pci_device_lock);

	addr = pci_pdev_calc_address(bdf, offset);

	/* Write address to ADDRESS register */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/* Write value to DATA register */
	switch (bytes) {
	case 1U:
		pio_write8((uint8_t)val, (uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		pio_write16((uint16_t)val, (uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		pio_write32(val, (uint16_t)PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);
}

/* enable: 1: enable INTx; 0: Disable INTx */
void enable_disable_pci_intx(union pci_bdf bdf, bool enable)
{
	uint32_t cmd, new_cmd;

	/* Set or clear the INTXDIS bit in COMMAND register */
	cmd = pci_pdev_read_cfg(bdf, PCIR_COMMAND, 2U);
	if (enable) {
		new_cmd = cmd & ~PCIM_CMD_INTxDIS;
	} else {
		new_cmd = cmd | PCIM_CMD_INTxDIS;
	}

	if ((cmd ^ new_cmd) != 0U) {
		pci_pdev_write_cfg(bdf, PCIR_COMMAND, 0x2U, new_cmd);
	}
}
