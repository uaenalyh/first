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
#include <vtd.h>
#include <bits.h>
#include <board.h>
#include <platform_acpi_info.h>

/**
 * @defgroup hwmgmt_pci hwmgmt.pci
 * @ingroup hwmgmt
 * @brief Implementation of all external APIs to operate PCI configuration registers of a physical PCI device.
 *
 * This module implements the APIs to operate PCI configuration registers of a physical PCI device. The main functions
 * include: read a register from PCI configuration space and write a register to PCI configuration space.
 *
 * Usage:
 * - 'vp-dm.vperipheral' module depends on this module to operate the physical PCI device associated with the
 * virtual PCI device.
 *
 * Dependency:
 * - This module depends on 'lib.lock' module to do spin lock and unlock for PCI devices operation.
 * - This module depends on 'hwmgmt.io' module to do port IO read/write operations.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the hwmgmt.pci module.
 *
 * This file implements all external functions, data structures, and macros that shall be provided by the
 * hwmgmt.pci module.
 *
 * It also defines a helper functions:
 * - pci_pdev_calc_address: calculate the address used to do port I/O operation for PCI configuration registers.
 *
 */

/**
 * @brief A spin lock variable used in PCI read/write operation, to avoid different guest VMs to operate the PCI
 * configuration space in parallel.
 */
static spinlock_t pci_device_lock = {
	.head = 0U,
	.tail = 0U
};

/**
 * @brief Calculate an address as PCI spec requirement and used to operate a PCI device configuration register
 *
 * This function is called to calculate an address as PCI spec requirement and the address can be used to
 * operate a PCI device configuration register. The 32bits address format is:
 * - bit 0-7: the register offset in the PCI configuration space
 * - bit 8-23: the BDF number of the PCI device
 * - bit 31: the enable bit, set it to 1 to make this address valid
 * Other bits are set to 0.
 *
 * @param[in] bdf The BDF number of the PCI device.
 * @param[in] offset The register offset in the PCI configuration space.
 *
 * @return the address used to read/write PCI configuration register
 *
 * @pre offset < PCI_REGMAX
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark It is an internal function called by pci_pdev_read_cfg and pci_pdev_write_cfg
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static uint32_t pci_pdev_calc_address(union pci_bdf bdf, uint32_t offset)
{
	/** Declare the following local variables of type uint32_t.
	 *  - addr representing the address to be returned, initialized as bdf.value. */
	uint32_t addr = (uint32_t)bdf.value;

	/** Shift 'addr' left for 8 bits, in order to move BDF value to bits: 8-23 */
	addr <<= 8U;
	/** Bitwise OR 'addr' by 'offset' OR PCI_CFG_ENABLE, in order to set bit0-7 to 'offset', and bit32 to 1 */
	addr |= (offset | PCI_CFG_ENABLE);
	/** Return the address used to read/write PCI configuration register */
	return addr;
}

/**
 * @brief Read a register from the physical PCI device configuration space
 *
 * This function is called to read a register from the physical PCI device configuration space. It calculates an
 * addressable value according to the input parameters of the PCI device: BDF, offset and bytes. And write the
 * address to the PCI configuration-address port, then read the register value from the PCI configuration-data port.
 *
 * @param[in] bdf The BDF number of the PCI device.
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length to read.
 *
 * @return The register value of the phyiscal PCI device in its configuration space.
 *
 * @pre offset < PCI_REGMAX
 * @pre (offset & (bytes - 1)) == 0
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	/** Declare the following local variables of type uint32_t.
	 *  - addr representing the value to address the register of the physical PCI device, not initialized. */
	uint32_t addr;
	/** Declare the following local variables of type uint32_t.
	 *  - val representing the value of the register to read, not initialized. */
	uint32_t val;
	/** Declare the following local variables of type uint16_t.
	 *  - data_port_addr representing the address of the PCI data port to read, not initialized. */
	uint16_t data_port_addr;

	/** Call pci_pdev_calc_address with the following parameters, in order to calculate the value
	 *  for PCI configuration-address port to access the given register, and set addr to its return value.
	 *  - bdf
	 *  - offset
	 */
	addr = pci_pdev_calc_address(bdf, offset);

	/** Set data_port_addr to PCI_CONFIG_DATA, which is the base address of the PCI data port. */
	data_port_addr = (uint16_t)PCI_CONFIG_DATA;
	/** Increment data_port_addr by the last two bits in \a offset, which is the offset within the data port where
	 *  the to-be-accessed register resides. */
	data_port_addr += (uint16_t)(offset & 3U);

	/** Call spinlock_obtain with the following parameters, in order to block other access to the PCI configuration
	 *  space, and to avoid different guest VMs to operate the PCI configuration space in parallel.
	 *  - &pci_device_lock
	 */
	spinlock_obtain(&pci_device_lock);

	/** Call pio_write32 with the following parameters, in order to write 'addr' to PCI configuration-address port.
	 *  - addr
	 *  - PCI_CONFIG_ADDR
	 */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/** Depending on the value of 'bytes': 1, 2 or others (4 bytes aligned as default) */
	switch (bytes) {
	/** To read \a bytes is 1 */
	case 1U:
		/** Set val to the value returned by pio_read8 with following parameter, to read one byte from the
		 *  physical PCI configure data port.
		 *  - data_port_addr
		 */
		val = (uint32_t)pio_read8(data_port_addr);
		/** End of case */
		break;
	/** To read \a bytes is 2 */
	case 2U:
		/** Set val to the value returned by pio_read16 with following parameter, to read two bytes from the
		 *  physical PCI configure data port.
		 *  - data_port_addr
		 */
		val = (uint32_t)pio_read16(data_port_addr);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Set val to the value returned by pio_read32 with following parameter, to read four bytes from the
		 *  physical PCI configure data port.
		 *  - data_port_addr
		 */
		val = pio_read32(data_port_addr);
		/** End of case */
		break;
	}
	/** Call spinlock_release with the following parameters, in order to unlock the access to PCI configuration
	 *  space.
	 *  - &pci_device_lock
	 */
	spinlock_release(&pci_device_lock);

	/** Return 'val' which is the register value. */
	return val;
}

/**
 * @brief Write a value to a configuration register of the physical PCI device
 *
 * This function is called to write a value to a configuration register of the physical PCI device. It calculates an
 * addressable value according to the input parameters of the PCI device: BDF, offset and bytes. And write the
 * address to the PCI configuration-address port, then write the register value to the PCI configuration-data port.
 *
 * @param[in] bdf The BDF number of the PCI device.
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length to write.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre offset < PCI_REGMAX
 * @pre (offset & (bytes - 1)) == 0
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark It is a public API called by other modules.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/** Declare the following local variables of type uint32_t.
	 *  - addr representing the value to address the register of the physical PCI device, not initialized. */
	uint32_t addr;
	/** Declare the following local variables of type uint16_t.
	 *  - data_port_addr representing the address of the PCI data port to read, not initialized. */
	uint16_t data_port_addr;

	/** Call spinlock_obtain with the following parameters, in order to block other access to the PCI configuration
	 *  space, and to avoid different guest VMs to operate the PCI configuration space in parallel.
	 *  - &pci_device_lock
	 */
	spinlock_obtain(&pci_device_lock);

	/** Call pci_pdev_calc_address with the following parameters, in order to calculate the value
	 *  for PCI configuration-address port to access the given register, and set addr to its return value.
	 *  - bdf
	 *  - offset
	 */
	addr = pci_pdev_calc_address(bdf, offset);

	/** Set data_port_addr to PCI_CONFIG_DATA, which is the base address of the PCI data port. */
	data_port_addr = (uint16_t)PCI_CONFIG_DATA;
	/** Increment data_port_addr by the last two bits in \a offset, which is the offset within the data port where
	 *  the to-be-accessed register resides. */
	data_port_addr += (uint16_t)(offset & 3U);

	/** Call pio_write32 with the following parameters, in order to write 'addr' to PCI configuration-address port.
	 *  - addr
	 *  - PCI_CONFIG_ADDR
	 */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/** Depending on the value of 'bytes' */
	switch (bytes) {
	/** To write \a bytes is 1 */
	case 1U:
		/** Call pio_write8 with the following parameters, in order to write 'val' to the configuration register
		 *  of the physical PCI device.
		 *  - (uint8_t)val
		 *  - data_port_addr
		 */
		pio_write8((uint8_t)val, data_port_addr);
		/** End of case */
		break;
	/** To write \a bytes is 2 */
	case 2U:
		/** Call pio_write16 with the following parameters, in order to write 'val' to the register in the
		 *  configuration space.
		 *  of the physical PCI device.
		 *  - (uint16_t)val
		 *  - data_port_addr
		 */
		pio_write16((uint16_t)val, data_port_addr);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Call pio_write32 with the following parameters, in order to write 'val' to the configuration
		 *  register of the physical PCI device.
		 *  - val
		 *  - data_port_addr
		 */
		pio_write32(val, data_port_addr);
		/** End of case */
		break;
	}
	/** Call spinlock_release with the following parameters, in order to unlock the access to PCI configuration
	 *  space.
	 *  - &pci_device_lock
	 */
	spinlock_release(&pci_device_lock);
}

/**
 * @}
 */
