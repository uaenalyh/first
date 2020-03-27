/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
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

#ifndef PCI_H_
#define PCI_H_

/**
 * @addtogroup hwmgmt_pci
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the hwmgmt.pci module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * hwmgmt.pci module.
 *
 * For the macros definition of PCI configuration space registers, there are some rules as following:
 * - PCIM_xxx: mask to locate subfield in register
 * - PCIR_xxx: configuration register offset
 * - PCIC_xxx: device class
 * - PCIS_xxx: device subclass
 */

#define PCI_BAR_COUNT 0x6U /**< Pre-defined PCI devices BAR count. */
#define PCI_REGMAX    0xFFU /**< Pre-defined the MAX offset of the PCI configuration space registers. */

#define PCI_CONFIG_ADDR 0xCF8U /**< Pre-defined the I/O port of PCI configuration address. */
#define PCI_CONFIG_DATA 0xCFCU /**< Pre-defined the I/O port of PCI configuration data. */

#define PCI_CFG_ENABLE 0x80000000U /**< Pre-defined the mask used when accessing PCI configuration space registers. */

/* PCI config header registers for all devices */
#define PCIR_VENDOR          0x00U /**< Pre-defined the offset of vendor ID register in PCI configuration space. */
#define PCIR_DEVICE          0x02U /**< Pre-defined the offset of device ID register in PCI configuration space. */
#define PCIR_COMMAND         0x04U /**< Pre-defined the offset of command register in PCI configuration space. */
#define PCIM_CMD_INTxDIS     0x400U /**< Pre-defined the mask used to set disable bit to PCI legacy interrupt. */
#define PCIR_REVID           0x08U /**< Pre-defined the offset of revision ID register in PCI configuration space. */
#define PCIR_SUBCLASS        0x0AU /**< Pre-defined the offset of subclass ID register in PCI configuration space. */
#define PCIR_CLASS           0x0BU /**< Pre-defined the offset of class ID register in PCI configuration space. */
#define PCIR_HDRTYPE         0x0EU /**< Pre-defined the offset of header type register in PCI configuration space. */
#define PCIM_HDRTYPE_NORMAL  0x00U /**< Pre-defined the mask used as to define the type of PCI configuration space */
#define PCIR_BARS            0x10U /**< Pre-defined the offset of BAR registers in PCI configuration space.*/
#define PCIM_BAR_SPACE       0x01U /**< Pre-defined the mask used to check PCI BAR type: MMIO or I/O. */
#define PCIM_BAR_IO_SPACE    0x01U /**< Pre-defined the mask used to indicate the BAR type: I/O mapped. */
#define PCIM_BAR_MEM_TYPE    0x06U /**< Pre-defined the mask used to check the MMIO BAR is 32 or 64bits. */
#define PCIM_BAR_MEM_32      0x00U /**< Pre-defined the mask used to indicate the MMIO BAR is 32bits. */
#define PCIM_BAR_MEM_64      0x04U /**< Pre-defined the mask used to indicate the MMIO BAR is 64bits. */
#define PCIR_CAP_PTR         0x34U /**< Pre-defined the offset of capability register in PCI configuration space. */

#define PCI_BASE_ADDRESS_MEM_MASK (~0x0fUL) /**< Pre-defined the mask used to calculate base address of a MMIO BAR */

/* Capability Register Offsets */
#define PCICAP_ID      0x0U /**< Pre-defined the offset of capability ID register within a capability structure. */
#define PCICAP_NEXTPTR 0x1U /**< Pre-defined the offset of next capability register within a capability structure. */

/* Capability Identification Numbers */
#define PCIY_MSI 0x05U /**< Pre-defined the ID number of MSI capability. */

/* PCI Message Signalled Interrupts (MSI) */
#define PCIR_MSI_CTRL           0x02U /**< Pre-defined the offset of MSI control register within MSI capability */
#define PCIM_MSICTRL_64BIT      0x80U /**< Pre-defined the mask used to check the MSI address/data is 64bits */
#define PCIM_MSICTRL_MSI_ENABLE 0x01U /**< Pre-defined the mask used to enable/disable MSI */
#define PCIR_MSI_ADDR           0x4U /**< Pre-defined the offset of MSI address register within MSI capability */
#define PCIR_MSI_ADDR_HIGH      0x8U /**< Pre-defined the offset of high 32bits MSI address register. */
#define PCIR_MSI_DATA           0x8U /**< Pre-defined the offset of MSI data register within MSI capability */
#define PCIR_MSI_DATA_64BIT     0xCU /**< Pre-defined the offset of high 32bits MSI data register. */

/**
 * @brief Pre-defined the mask of "multiple message capable" fields in MSI control register.
 */
#define PCIM_MSICTRL_MMC_MASK   0x000EU
/**
 * @brief Pre-defined the mask of "multiple message enable" fields in MSI control register.
 */
#define PCIM_MSICTRL_MME_MASK   0x0070U

/* PCI device class */
#define PCIC_BRIDGE      0x06U /**< Pre-defined the class value for PCI host bridge. */
#define PCIS_BRIDGE_HOST 0x00U /**< Pre-defined the subclass value for PCI host bridge. */

/**
 * @brief Data structure to present PCI BDF information
 *
 * Data structure to present PCI BDF information, a 16 bits number including BUS (8bits)/DEVICE (5bits)/
 * FUNCTION (3bits), or combine DEVICE and FUNCTION as a 8bits number.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
union pci_bdf {
	uint16_t value; /**< A 16bits number combined BUS, DEVICE and FUNCTION. */
	struct {
		uint8_t f : 3; /**< BITs 0-2 as FUNCTION number of a PCI device */
		uint8_t d : 5; /**< BITs 3-7 as DEVICE number of a PCI device */
		uint8_t b; /**< BITs 8-15 as BUS number of a PCI device */
	} bits;
	struct {
		uint8_t devfun; /**< BITs 0-7 combined DEVICE and FUNCTION numbers */
		uint8_t bus; /**< BITs 8-15 as BUS number of a PCI device */
	} fields;
};

/**
 * @brief An enumeration type to indicate a BAR register's type.
 *
 * This enumeration type represents the BAR register's types, including IO BAR and MMIO BAR (32bits or 64bits).
 *
 * @remark N/A
 */
enum pci_bar_type {
	PCIBAR_NONE = 0, /**< the value to indicate no BAR included in the PCI device */
	PCIBAR_IO_SPACE, /**< IO BAR */
	PCIBAR_MEM32, /**< 32bits MMIO BAR */
	PCIBAR_MEM64, /**< 64bits MMIO BAR */
	PCIBAR_MEM64HI, /**< the high 32bits part of a 64bits MMIO BAR */
};

/**
 * @brief Get the offset of a BAR register in PCI configuration space.
 *
 * This function is called to get the offset of a BAR register in PCI configuration space.
 *
 * @param[in] idx The index of the BAR register in PCI configuration space.
 *
 * @return the offset of the BAR register corresponding with the given 'idx'
 *
 * @pre idx < PCI_BAR_COUNT
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static inline uint32_t pci_bar_offset(uint32_t idx)
{
	/** Return the sum of PCIR_BARS and idx left-shifted by 2, which is the offset of the (idx + 1)'th BAR
	 *  in the PCI configuration space */
	return PCIR_BARS + (idx << 2U);
}

uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes);
void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);

/**
 * @}
 */

#endif /* PCI_H_ */
