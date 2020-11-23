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

#ifndef VPCI_H_
#define VPCI_H_

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the virtual PCI (vPCI) component.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the vPCI
 * component in vp-dm.vperipheral module.
 *
 */

#include <spinlock.h>
#include <pci.h>

/**
 * @brief Data structure to present PCI BAR information
 *
 * Data structure to present PCI BAR information, including its type (MMIO BAR and 32bits or 64bits), BAR size,
 * GPA/HPA of BAR base, BAR memory type (low 4bits of a BAR register) and BAR size mask.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct pci_bar {
	enum pci_bar_type type; /**< The BAR type: IO BAR, or 32bit/64bits MMIO BAR. */
	uint64_t size; /**< The BAR size */
	uint64_t base; /**< The guest physical address of BAR base */
	uint64_t base_hpa; /**< The host physical address of BAR base */
	uint32_t fixed; /**< The BAR memory type encoding, the low 4bits of a BAR register */
	uint32_t mask; /**< The BAR mask: BAR register value & ~0FH */
};

/**
 * @brief Data structure to present PCI MSI capability basic information
 *
 * Data structure to present PCI MSI capability basic information, including its type (32bits or 64bits capability),
 * offset and length of the MSI capability.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct pci_msi {
	bool is_64bit;   /**< A boolean to indicate this MSI is 64bits or not */
	uint32_t capoff; /**< The offset of the MSI capability in PCI configuration space */
	uint32_t caplen; /**< The length of the MSI capability */
};

/**
 * @brief A union data structure to store all the data of a PCI configuration space
 *
 * A union data structure to store all the data of a PCI configuration space. It includes 256 bytes. Also it
 * can be accessed as 2bytes or 4bytes aligned.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
union pci_cfgdata {
	uint8_t data_8[PCI_REGMAX + 1U];  /**< The PCI configuration space data as 1 bytes access */
	uint16_t data_16[(PCI_REGMAX + 1U) >> 1U]; /**< The PCI configuration space data as 2 bytes access */
	uint32_t data_32[(PCI_REGMAX + 1U) >> 2U]; /**< The PCI configuration space data as 4 bytes access */
};

struct pci_vdev; /**< A declaration of 'struct pci_vdev' just for usage here */

/**
 * @brief Data structure to present a set of operation functions to a vPCI device
 *
 * Data structure to present a set of operation functions to a vPCI device. It includes 4 functions:
 * init / de-init / read / write.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct pci_vdev_ops {
	void (*init_vdev)(struct pci_vdev *vdev); /**< The function to initialize the vPCI device */
	void (*deinit_vdev)(struct pci_vdev *vdev); /**< The function to de-init the vPCI device */

	/**
	 * @brief The function to write a register in the vPCI configuration space
	 */
	void (*write_vdev_cfg)(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
	/**
	 * @brief  The function to read a register in the vPCI configuration space
	 */
	void (*read_vdev_cfg)(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
};

/**
 * @brief Data structure to present a vPCI device information
 *
 * Data structure to present a vPCI device information, which includes the associated information: the pointer to
 *  the vPCI field of a VM and the pointer to its device configuration data; and the basic information of its self:
 * like its BAR, MSI, virtual configuration space registers and virtual BDF, and physical BDF if it is associated with
 * a phyiscal PCI device.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct pci_vdev {
	const struct acrn_vpci *vpci; /**< The pointer the vPCI field of a VM */
	union pci_bdf bdf;  /**< The BDF (bus/device/function) of the virtual PCI device. */
	union pci_bdf pbdf; /**< The BDF of the physical PCI device associated with this vPCI device. */

	union pci_cfgdata cfgdata; /**< The configuration space data of the vPCI device. */

	uint32_t nr_bars;  /**< The BAR number of this vPCI device. */
	struct pci_bar bar[PCI_BAR_COUNT]; /**< The BARs info of this vPCI device. */

	struct pci_msi msi; /**< The MSI info of this vPCI device. */

	struct acrn_vm_pci_dev_config *pci_dev_config; /**< Pointer to corresponding PCI device's vm_config */

	const struct pci_vdev_ops *vdev_ops; /**< Pointer to the corressponding operations for the vPCI device */
};

/**
 * @brief A union data structure to present a MMIO address used when a guest OS accesses a PCI configuration register
 *
 * A union data structure to present an address used when a guest OS accesses a PCI configuration register. It includes
 * the BDF of a PCI device, the register offset in the configuration space, and a enabling bit.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
union pci_cfg_addr_reg {
	uint32_t value;  /**< The value of the MMIO address to access a PCI configuration register */
	struct {
		uint32_t reg_num : 8; /**< BITs 0-7, Register Number (BITs 0-1, always reserve to 0) */
		uint32_t bdf : 16; /**< BITs 8-23, BDF Number */
		uint32_t resv : 7; /**< BITs 24-30, Reserved */
		uint32_t enable : 1; /**< BITs 31, Enable bit */
	} bits;
};

/**
 * @brief Data structure to present the vPCI information of a VM
 *
 * Data structure to present the vPCI information of a VM.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct acrn_vpci {
	spinlock_t lock;  /**< The lock used in accessing the PCI configuration space */
	struct acrn_vm *vm; /**< Pointer to the VM which this vPCI uint belongs to */
	union pci_cfg_addr_reg addr; /**< The MMIO address to access a PCI configuration register */
	uint32_t pci_vdev_cnt; /**< The total number of the devices included by this vPCI unit */
	struct pci_vdev pci_vdevs[CONFIG_MAX_PCI_DEV_NUM]; /**< The vPCI devices list */
};

extern const struct pci_vdev_ops vhostbridge_ops;
void vpci_init(struct acrn_vm *vm);
void vpci_cleanup(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* VPCI_H_ */
