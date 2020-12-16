/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_H
#define IO_H

/**
 * @defgroup hwmgmt_io hwmgmt.io
 * @ingroup hwmgmt
 * @brief Implementation of read and write operations for port I/O and MMIO.
 *
 * This module implements the read and write operations on physical I/O ports and I/O memory.
 *
 * Usage:
 * - 'hwmgmt.irq' module depends on this module to disable PIC.
 * - 'vp-dm.vperipheral' module depends on this module to read physical RTC value.
 * - 'hwmgmt.pci' module depends on this module to read/write PCI configuration space registers.
 * - 'hwmgmt.apic' module depends on this module to read/write IOAPIC registers.
 * - 'hwmgmt.vtd' module depends on this module to read/write remapping registers.
 *
 * @{
 */

/**
 * @file
 * @brief This file contains all the APIs implementations for hwmgmt.io module.
 *
 */

#include <types.h>

/**
 * @brief To write an 8-bits value to the specified I/O port.
 *
 * @param[in] value The 8-bits value to write.
 * @param[in] port The I/O port to write to.
 *
 * @return None
 *
 * @pre port == 0A1H || port == 21H || port == 70H || 0CFCH <= port <= 0CFFH

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void pio_write8(uint8_t value, uint16_t port)
{
	/** Execute 'outb' instruction in order to write an 8-bits value to a I/O port.
	 *  - Input operands: \a value which is a source operand, \a port which is a destination operand
	 *  - Output operands: none
	 *  - Clobbers none
	 */
	asm volatile("outb %0,%1" ::"a"(value), "dN"(port));
}

/**
 * @brief To read an 8-bits value from the specified I/O port.
 *
 * @param[in] port The I/O port to read from.
 *
 * @return An 8-bits value read from the specified I/O port.
 *
 * @pre port == 071H || (0CFCH <= port && port <= 0CFFH)

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint8_t pio_read8(uint16_t port)
{
	/** Declare the following local variables of type uint8_t.
	 *  - 'value' representing the value to be returned, not initialized.
	 */
	uint8_t value;
	/** Execute 'inb' instruction in order to get an 8-bits value from a I/O port.
	 *  - Input operands: \a port which is a source operand
	 *  - Output operands: 'value' which is a destination operand
	 *  - Clobbers none
	 */
	asm volatile("inb %1,%0" : "=a"(value) : "dN"(port));
	/** Return 'value' */
	return value;
}

/**
 * @brief To write a 16-bits value to the specified I/O port.
 *
 * @param[in] value The 16-bits value to write.
 * @param[in] port The I/O port to write to.
 *
 * @return None
 *
 * @pre port == 0CFCH || port == 0CFEH

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void pio_write16(uint16_t value, uint16_t port)
{
	/** Execute 'outw' instruction in order to write a 16-bits value to a I/O port.
	 *  - Input operands: \a value which is a source operand, \a port which is a destination operand
	 *  - Output operands: none
	 *  - Clobbers none
	 */
	asm volatile("outw %0,%1" ::"a"(value), "dN"(port));
}

/**
 * @brief To read a 16-bits values from the specified I/O port.
 *
 * @param[in] port The I/O port to read from.
 *
 * @return A 16-bits value read from the specified I/O port.
 *
 * @pre port == 0CFCH || port == 0CFEH

 * @post N/A
 *
 * @remark Application Constraints: the following port range shall be available on the physical platform:
 *         - 21H
 *         - 70H - 71H
 *         - 0A1H
 *         - 0CF8H
 *         - 0CFCH - 0CFFH
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint16_t pio_read16(uint16_t port)
{
	/** Declare the following local variables of type uint16_t.
	 *  - 'value' representing the value to be returned, not initialized.
	 */
	uint16_t value;
	/** Execute 'inw' instruction in order to get a 16-bits value from a I/O port.
	 *  - Input operands: \a port which is a source operand
	 *  - Output operands: 'value' which is a destination operand
	 *  - Clobbers none
	 */
	asm volatile("inw %1,%0" : "=a"(value) : "dN"(port));
	/** Return 'value' */
	return value;
}

/**
 * @brief To write a 32-bits value to the specified I/O port.
 *
 * @param[in] value The 32-bits value to write.
 * @param[in] port The I/O port to write to.
 *
 * @return None
 *
 * @pre port == 0CF8H || port == 0CFCH

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void pio_write32(uint32_t value, uint16_t port)
{
	/** Execute 'outl' instruction in order to write a 32-bits value to a I/O port.
	 *  - Input operands: \a value which is a source operand, \a port which is a destination operand
	 *  - Output operands: none
	 *  - Clobbers none
	 */
	asm volatile("outl %0,%1" ::"a"(value), "dN"(port));
}

/**
 * @brief To read a 32-bits values from the specified I/O port.
 *
 * @param[in] port The I/O port to read from.
 *
 * @return A 32-bits value read from the specified I/O port.
 *
 * @pre port == 0CFCH

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint32_t pio_read32(uint16_t port)
{
	/** Declare the following local variables of type uint32_t.
	 *  - 'value' representing the value to be returned, not initialized.
	 */
	uint32_t value;

	/** Execute 'inl' instruction in order to get a 32-bits value from a I/O port.
	 *  - Input operands: \a port which is a source operand
	 *  - Output operands: 'value' which is a destination operand
	 *  - Clobbers none
	 */
	asm volatile("inl %1,%0" : "=a"(value) : "dN"(port));
	/** Return 'value' */
	return value;
}

/**
 * @brief To write a 32-bits value to a memory mapped IO device.
 *
 * @param[in] value The 32-bits value to write.
 * @param[in] addr The memory address to write to.

 * @return None
 *
 * @pre (0FEC00000H <= addr && addr <= 0FEC003FFH) || (0FED90000H <= addr && addr < 0FED92000H)

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void mmio_write32(uint32_t value, void *addr)
{
	/** Declare the following local variables of type 'volatile uint32_t *'.
	 *  - 'addr32' representing the memory mapped IO address to write, initialized as the value of \a addr.
	 */
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	/** Write \a value to the specified memory addressed by the virtual address \a addr */
	*addr32 = value;
}

/**
 * @brief To read a 32-bits value from a memory mapped IO device.
 *
 * @param[in] addr The memory address to read from.
 *
 * @return A 32-bits value read from the given address.

 * @pre (0FEC00000H <= addr && addr <= 0FEC003FFH) || (0FED90000H <= addr && addr < 0FED92000H)

 * @post N/A
 *
 * @remark Application Constraints: the following MMIO range shall be available on the physical platform:
 *         - 0FEC00000H - 0FEC003FFH
 *         - 0FED90000H - 0FED92000H
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint32_t mmio_read32(const void *addr)
{
	/** Return a 32-bits value read from the virtual address \a addr */
	return *((volatile const uint32_t *)addr);
}

/**
 * @}
 */

#endif /* _IO_H defined */
