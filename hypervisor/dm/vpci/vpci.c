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

#include <vm.h>
#include <vtd.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by vPCI component.
 *
 * This file implements all external functions that shall be provided by the vPCI part of vp-dm.vperipheral module.
 * The vPCI devices includes: vhostbirdge and some passthrough devices. The vhostbridge is a pure virtual hostbridge,
 * which is not related with the physical hostbridge. But to the passthrough PCI devices, some operations is based on
 * their virtual configuration space, some are mapped to their physical configuration space, like MSI and BAR registers.
 *
 * It defines one initial function, one de-init function, and four callback functions (read/write address and data
 * register) which are registered and called when PCI port IO is accessed. It also defines some helper functions to
 * implement the features that are commonly used in this file. In addition, it defines some decomposed functions to
 * improve the readability of the code.
 *
 * Helper functions: vpci_is_valid_access_offset, vpci_is_valid_access_byte, vpci_is_valid_access, pci_bar_index
 *
 * Decomposed functions: read_cfg, write_cfg, vpci_init_vdevs, assign_vdev_pt_iommu_domain,
 * remove_vdev_pt_iommu_domain, init_default_cfg, vpci_init_pt_dev, vpci_deinit_pt_dev, vpci_write_pt_dev_cfg,
 * vpci_read_pt_dev_cfg, vpci_init_vdev, vpci_init_vdevs
 *
 * Note: for FuSa scope, the passthrough PCI devices just includes: USB controller and Ethernet card.
 */

#define PCI_USB_CONTROLLER 0x0CU /**< Pre-defined class number of USB PCI device. */
#define PCI_ETH_CONTROLLER 0x02U /**< Pre-defined class number of Ethernet PCI device. */

#define PCI_DISABLED_CONFIG_ADDR 0x00FFFF00U /**< A value indicating the PCI config address port is disabled. */

/**
 * @brief Data structure to represent a PCI capability's ID and its length.
 *
 * Data structure to represent a PCI capability's ID and its length, because hypervisor just exposes MSI capability to
 * the guest VM, this structure is used to clean up some hidden capabilities in the virtual configuration space.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct cap_info {
	uint8_t id;     /**< The ID of one capability. */
	uint8_t length; /**< The length of one capability. */
};

/**
 * @brief Data structure to represent some unused fields in the virtual PCI configuration space
 *
 * Data structure to represent some unused fields to be hidden from guest VM. Because some fields in the passthrough
 * physical PCI configuration space are not used in native, they'll be clean up in its virtual configuration space.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct unused_fields {
	uint8_t offset;  /**< The offset of the unused field in PCI configuration space. */
	uint8_t length;  /**< The length of this unused field. */
};

static void vpci_init_vdevs(struct acrn_vm *vm);
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);

/**
 * @brief Read a port value from the virtual PCI configuration address IO port-0CF8H
 *
 * This function is called to read a port value from the PCI configuration address IO port-0CF8H. It is called when
 * a guest VM reads its PCI configuration address IO port.
 *
 * @param[inout] vcpu The vCPU which attempts to execute the port IO read
 * @param[in] addr The port IO address that the vCPU attempts to access.
 * @param[in] bytes Number of bytes the vCPU attempts to read.
 *
 * @return None.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 0CF8H
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post Assign vcpu->vm->vpci->addr.value to vcpu->req.reqs.pio.value if 'bytes' == 4 and addr == 0CF8H; or assign
 * 0FFFFFFFFH to vcpu->req.reqs.pio.value.
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a callback function and called only after vpci_init has been called.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void pci_cfgaddr_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	/** Declare the following local variables of type uint32_t.
	 *  - val representing the port value to read, initialized as 0FFFFFFFFH. */
	uint32_t val = ~0U;
	/** Declare the following local variables of type 'struct acrn_vpci *'.
	 *  - vpci representing the vPCI owned by the VM, initialized as '&vcpu->vm->vpci'. */
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	/** Declare the following local variables of type 'union pci_cfg_addr_reg *'.
	 *  - cfg_addr representing the vPCI configuration address info, initialized as '&vpci->addr'. */
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	/** Declare the following local variables of type 'struct pio_request *'.
	 *  - pio_req representing the port IO request of the vCPU, initialized as '&vcpu->req.reqs.pio'. */
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	/** If 'addr' is 0CF8H and 'bytes' equals 4 */
	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/** Set val to cfg_addr->value, which means that it is a normal reading operation */
		val = cfg_addr->value;
	}

	/** Set pio_req->value to val, which will return to the guest VM. */
	pio_req->value = val;
}

/**
 * @brief Write a value to the virtual PCI configuration address IO port-0CF8H
 *
 * This function is called to write a value to the PCI configuration address IO port-0CF8H. It is called when a guest
 * VM writes its PCI configuration address IO port.
 *
 * @param[inout] vcpu The vCPU which attempts to execute the port IO write
 * @param[in] addr The port IO address that the vCPU attempts to access.
 * @param[in] bytes Number of bytes the vCPU attempts to write.
 * @param[in] val The value that the vCPU attempts to write.
 *
 * @return None.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 0CF8H
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post Assign (val & 80FFFFFCH) to vcpu->vm->vpci->addr.value if 'bytes' == 4 and addr == 0CF8H; or do nothing.
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a callback function and called only after vpci_init has been called.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void pci_cfgaddr_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	/** Declare the following local variables of type 'struct acrn_vpci *'.
	 *  - vpci representing the vPCI owned by the VM, initialized as '&vcpu->vm->vpci'. */
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	/** Declare the following local variables of type 'union pci_cfg_addr_reg *'.
	 *  - cfg_addr representing the vPCI configuration address info, initialized as '&vpci->addr'. */
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;

	/** If 'addr' is 0CF8H and 'bytes' equal 4 */
	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/** Set cfg_addr->value to 'val & (~0x7f000003U)', unmask reserved fields: BITs 24-30 and BITs 0-1 */
		cfg_addr->value = val & (~0x7f000003U);
	}
}

/**
 * @brief Check whether the offset of a vPCI access is valid
 *
 * This function is called to check whether the offset of a vPCI access is valid. It checks the given 'offset'
 * in PCI configuration space and the given 'bytes' to read/write. The general rules are as following:
 * - offset & 3H == 0, bytes can be 1 / 2 / 4
 * - offset & 3H == 2, bytes can be 1 / 2
 * - offset & 1H == 1, bytes only can be 1
 *
 * @param[in] offset The register address in PCI configuration space
 * @param[in] bytes Number of bytes to read/write.
 *
 * @return A boolean to indicate whether the offset is valid: true if valid, false if invalid.
 *
 * @pre (bytes == 1) || (bytes == 2) || (bytes == 4)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal function called within vPCI.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool vpci_is_valid_access_offset(uint32_t offset, uint32_t bytes)
{
	/** Return true if '(offset & (bytes - 1)) == 0', or return false */
	return ((offset & (bytes - 1U)) == 0U);
}

/**
 * @brief Check whether the number of bytes to read/write in a PCI access is valid
 *
 * This function is called to check whether the number of bytes to read/write is valid for a PCI configuration port
 * access. It checks the given 'bytes' which should be 1 / 2 / 4.
 *
 * @param[in] bytes Number of bytes to read/write.
 *
 * @return A boolean to indicate whether the access is valid: true if valid, false if invalid.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal function called within vPCI.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool vpci_is_valid_access_byte(uint32_t bytes)
{
	/** Return true if the given bytes is one of 1/2/4. */
	return ((bytes == 1U) || (bytes == 2U) || (bytes == 4U));
}

/**
 * @brief Check whether a vPCI access is valid
 *
 * This function is called to check whether a vPCI access is valid. It calls internal helper functions to check the
 * given 'offset' and 'bytes' and return the result.
 *
 * @param[in] offset The register address in PCI configuration space
 * @param[in] bytes Number of bytes to read/write.
 *
 * @return A boolean to indicate whether the access is valid: true if valid, false if  invalid.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal function called within vPCI.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline bool vpci_is_valid_access(uint32_t offset, uint32_t bytes)
{
	/** Return true if the returned values of both functions vpci_is_valid_access_byte (with bytes being the
	 *  parameter) and vpci_is_valid_access_offset (with offset and bytes being the parameters) are true.
	 */
	return (vpci_is_valid_access_byte(bytes) && vpci_is_valid_access_offset(offset, bytes));
}

/**
 * @brief Read a register in the virtual PCI configuration space by its data IO port: 0CFCH-0CFFH
 *
 * This function is called to read a register in the PCI configuration space by its data IO port: 0CFCH-0CFFH.
 * It is called when a guest VM reads its PCI configuration data IO port. This function calls the internal function
 * to get the register's value from the corresponding PCI device configuration space.
 *
 * @param[inout] vcpu The vCPU which attempts to execute the port IO read
 * @param[in] addr The port IO address that the vCPU attempts to access.
 * @param[in] bytes Number of bytes the vCPU attempts to read.
 *
 * @return None.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 0CF8H
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post The register value got from the PCI device configuration space will be set to the give vCPU's port IO request
 * field and returned to the guest VM.
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a callback function and called only after vpci_init has been called.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void pci_cfgdata_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM the given vCPU belongs to, initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;
	/** Declare the following local variables of type 'struct acrn_vpci *'.
	 *  - vpci representing the vPCI which belongs to the VM, initialized as &vm->vpci. */
	struct acrn_vpci *vpci = &vm->vpci;
	/** Declare the following local variables of type 'union pci_cfg_addr_reg'.
	 *  - cfg_addr representing the vPCI configuration address info, not initialized. */
	union pci_cfg_addr_reg cfg_addr;
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - bdf representing the BDF info of the PCI device this vCPU attempts to access, not initialized. */
	union pci_bdf bdf;
	/** Declare the following local variables of type uint32_t.
	 *  - offset representing the value offset to the base of PCI configuration data IO port: 0CFCH, initialized as
	 *  'addr - PCI_CONFIG_DATA'.
	 */
	uint32_t offset = addr - PCI_CONFIG_DATA;
	/** Declare the following local variables of type uint32_t.
	 *  - val representing the default value to read from PCI configuration space, initialized as 0FFFFFFFFH. */
	uint32_t val = ~0U;
	/** Declare the following local variables of type 'struct pio_request *'.
	 *  - pio_req representing the IO request field of the trapped vCPU, initialized as &vcpu->req.reqs.pio. */
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	/** Set cfg_addr.value to the value returned by function atomic_swap32, which returns vpci->addr.value and
	 *  set vpci->addr.value to PCI_DISABLED_CONFIG_ADDR.
	 */
	cfg_addr.value = atomic_swap32(&vpci->addr.value, PCI_DISABLED_CONFIG_ADDR);
	/** If cfg_addr.bits.enable is not 0, which means the configuration address is enabled. */
	if (cfg_addr.bits.enable != 0U) {
		/** Declare the following local variables of type uint32_t.
		 *  - target_reg representing the target register to read in the PCI configuration space, initialized
		 *  as 'cfg_addr.bits.reg_num + offset'.
		 */
		uint32_t target_reg = cfg_addr.bits.reg_num + offset;
		/** If the value returned by vpci_is_valid_access (with target_reg and bytes being the parameters)
		 *  is true, which means this access is valid */
		if (vpci_is_valid_access(target_reg, bytes)) {
			/** Set bdf.value to cfg_addr.bits.bdf */
			bdf.value = cfg_addr.bits.bdf;
			/** Call read_cfg with the following parameters, in order to read the register's value from the
			 *  corresponding PCI device configuration space.
			 *  - vpci
			 *  - bdf
			 *  - target_reg
			 *  - bytes
			 *  - &val
			 */
			read_cfg(vpci, bdf, target_reg, bytes, &val);
		}
	}

	/** Set pio_req->value to val, which will be returned to the guest VM. */
	pio_req->value = val;
}

/**
 * @brief Write a register in the virtual PCI configuration space by its data IO port: 0CFCH-0CFFH
 *
 * This function is called to write a register value to the PCI configuration space through its data IO port:
 * 0CFCH-0CFFH. It is called when a guest VM writes its PCI configuration space through data IO port. This
 * function calls internal function to write the register to the corresponding PCI device configuration space.
 *
 * @param[inout] vcpu The vCPU which attempts to execute the port IO write
 * @param[in] addr The port IO address that the vCPU attempts to access.
 * @param[in] bytes Number of bytes the vCPU attempts to write.
 * @param[in] val The value the vCPU attempts to write.
 *
 * @return None.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 0CF8H
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a callback function and called only after vpci_init has been called.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void pci_cfgdata_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM the given vCPU belongs to, initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;
	/** Declare the following local variables of type 'struct acrn_vpci *'.
	 *  - vpci representing the vPCI which belongs to the VM, initialized as &vm->vpci. */
	struct acrn_vpci *vpci = &vm->vpci;
	/** Declare the following local variables of type 'union pci_cfg_addr_reg'.
	 *  - cfg_addr representing the vPCI configuration address info, not initialized. */
	union pci_cfg_addr_reg cfg_addr;
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - bdf representing the BDF info of the PCI device this vCPU attempts to access, not initialized. */
	union pci_bdf bdf;
	/** Declare the following local variables of type uint32_t.
	 *  - offset representing the value offset to the base of PCI configuration data IO port: 0CFCH, initialized as
	 *  'addr - PCI_CONFIG_DATA'.
	 */
	uint32_t offset = addr - PCI_CONFIG_DATA;

	/** Set cfg_addr.value to the value returned by function atomic_swap32, which returns vpci->addr.value and
	 *  set vpci->addr.value to PCI_DISABLED_CONFIG_ADDR.
	 */
	cfg_addr.value = atomic_swap32(&vpci->addr.value, PCI_DISABLED_CONFIG_ADDR);
	/** If cfg_addr.bits.enable is not 0, which means the configuration address is enabled. */
	if (cfg_addr.bits.enable != 0U) {
		/** Declare the following local variables of type uint32_t.
		 *  - target_reg representing the target register to write in the PCI configuration space, initialized
		 *  as 'cfg_addr.bits.reg_num + offset'.
		 */
		uint32_t target_reg = cfg_addr.bits.reg_num + offset;
		/** If the value returned by vpci_is_valid_access (with target_reg and bytes being the parameters) is
		 *  true, which means this access is valid.
		 */
		if (vpci_is_valid_access(target_reg, bytes)) {
			/** Set bdf.value to cfg_addr.bits.bdf */
			bdf.value = cfg_addr.bits.bdf;
			/** Call write_cfg with the following parameters, in order to write the given 'val' to the
			 *  target register of the corresponding PCI device configuration space.
			 *  - vpci
			 *  - bdf
			 *  - target_reg
			 *  - bytes
			 *  - val
			 */
			write_cfg(vpci, bdf, target_reg, bytes, val);
		}
	}
}

/**
 * @brief Initialize the vPCI component which belongs to the given VM
 *
 * This function is called to initialize the vPCI component which belongs to the given VM. It will initialize each
 * PCI device owned by the given VM. Also it will register PCI port IO access callback functions, including read
 * and write functions for address and data IO ports. So when the VM accesses its PCI port IO, it will trap into
 * hypervisor and call these functions.
 *
 * @param[inout] vm A pointer to a VM instance whose vPCI component is to be initialized.
 *
 * @return None.
 *
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is a public API called by vp-base.vm module.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation.
 */
void vpci_init(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct vm_io_range'.
	 *  - pci_cfgaddr_range representing the PCI configuration address port IO range, initialized as
	 *  .base=0CF8H, .len=1.
	 */
	struct vm_io_range pci_cfgaddr_range = { .base = PCI_CONFIG_ADDR, .len = 1U };

	/** Declare the following local variables of type 'struct vm_io_range'.
	 *  - pci_cfgdata_range representing the PCI configuration data port IO range, initialized as
	 *  .base=0CFCH, .len=4.
	 */
	struct vm_io_range pci_cfgdata_range = { .base = PCI_CONFIG_DATA, .len = 4U };

	/** Set vm->vpci.addr.value to PCI_DISABLED_CONFIG_ADDR as a default value */
	vm->vpci.addr.value = PCI_DISABLED_CONFIG_ADDR;
	/** Set vm->vpci.vm to vm */
	vm->vpci.vm = vm;
	/** Set vm->iommu to the value returned by create_iommu_domain with vm->vm_id, the return value of a call to
	 *  hva2hpa (with vm->arch_vm.nworld_eptp) and 48 being the parameters, which creates an IOMMU domain (with an
	 *  address width of 48) for the given VM
	 */
	vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	/** Call vpci_init_vdevs with the following parameters, in order to initialize the vPCI devices list for the
	 *  given VM.
	 *  - vm
	 */
	vpci_init_vdevs(vm);

	/** Call register_pio_emulation_handler with the following parameters, in order to intercept port IO 0CF8H,
	 *  which is the PCI configuration address port IO. So when the guest VM accesses this port, it will trap into
	 *  hypervisor and call these registered functions.
	 *  - vm
	 *  - PCI_CFGADDR_PIO_IDX
	 *  - &pci_cfgaddr_range
	 *  - pci_cfgaddr_io_read
	 *  - pci_cfgaddr_io_write
	 */
	register_pio_emulation_handler(
		vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range, pci_cfgaddr_io_read, pci_cfgaddr_io_write);

	/** Call register_pio_emulation_handler with the following parameters, in order to intercept ports 0CFCH-0CFFH,
	 *  which are the PCI configuration data port IO. So when the guest VM accesses these ports, it will trap into
	 *  hypervisor and call these registered functions.
	 *  - vm
	 *  - PCI_CFGDATA_PIO_IDX
	 *  - &pci_cfgdata_range
	 *  - pci_cfgdata_io_read
	 *  - pci_cfgdata_io_write
	 */
	register_pio_emulation_handler(
		vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range, pci_cfgdata_io_read, pci_cfgdata_io_write);

	/** Call spinlock_init with the following parameters, in order to initialize the vPCI lock.
	 *  - &vm->vpci.lock
	 */
	spinlock_init(&vm->vpci.lock);
}

/**
 * @brief Release the vPCI related resource when the given VM is shutdown
 *
 * This function is called to release the vPCI related resource when the given VM is shutdown. It will call
 * each vPCI device's de-init function to implement this work.
 *
 * @param[in] vm A pointer to a VM instance whose vPCI resource is to be released.
 *
 * @return None.
 *
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is a public API called by vp-base.vm module.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation.
 */
void vpci_cleanup(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct pci_vdev *'.
	 *  - vdev representing a pointer to a vPCI device in the given VM, not initialized. */
	struct pci_vdev *vdev;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing an index used in the loop to probe the vPCI device in the given VM, not initialized. */
	uint32_t i;

	/** For each i ranging from 0 to vm->vpci.pci_vdev_cnt -1 [with a step of 1] */
	for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
		/** Set vdev to &(vm->vpci.pci_vdevs[i]) */
		vdev = &(vm->vpci.pci_vdevs[i]);

		/** Call vdev->vdev_ops->deinit_vdev with the following parameters, in order to invoke the callback
		 *  of the vPCI device, to do some de-init work.
		 *  - vdev */
		vdev->vdev_ops->deinit_vdev(vdev);
	}
}

/**
 * @brief Add the physical PCI device associated with the given vPCI into the IOMMU domain
 *
 * This function is called to add the physical PCI device associated with the given vPCI into the IOMMU domain,
 * which is allocated to the VM this PCI device belongs to.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init_pt_dev.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void assign_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the value returned by add_iommu_device, not initialized. */
	int32_t ret;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM the given vPCI blongs to, initialized as vdev->vpci->vm. */
	struct acrn_vm *vm = vdev->vpci->vm;

	/** Set ret to the value returned by add_iommu_device with vm->iommu, vdev->pbdf.fields.bus and
	 *  vdev->pbdf.fields.devfun being the parameters, which indicates whether the physical PCI device
	 *  associated with the given vPCI is added into the 'vm->iommu'
	 */
	ret = add_iommu_device(vm->iommu, vdev->pbdf.fields.bus, vdev->pbdf.fields.devfun);
	/** If ret is not 0, which means it is failed to add the physical PCI device into the IOMMU domain. */
	if (ret != 0) {
		/** Call panic with the following parameters, in order to output fatal failure log and could halt
		 *  current physical CPU.
		 *  - "failed to assign iommu device!"
		 */
		panic("failed to assign iommu device!");
	}
}

/**
 * @brief Remove the physical PCI device associated with the given vPCI from the IOMMU domain
 *
 * This function is called to remove the physical PCI device associated with the given vPCI from the IOMMU domain,
 * which is allocated to the VM this PCI device belongs to.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by vpci_deinit_pt_dev.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void remove_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the value returned by remove_iommu_device, not initialized. */
	int32_t ret;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the VM the given vPCI blongs to, initialized as vdev->vpci->vm. */
	struct acrn_vm *vm = vdev->vpci->vm;

	/** Set ret to the value returned by remove_iommu_device with vm->iommu, vdev->pbdf.fields.bus and
	 *  vdev->pbdf.fields.devfun being the parameters, which indicates whether the physical PCI device
	 *  associated with the given vPCI is removed from the 'vm->iommu' */
	ret = remove_iommu_device(vm->iommu, vdev->pbdf.fields.bus, vdev->pbdf.fields.devfun);
	/** If ret is not 0, which means it is failed to remove the physical PCI device from the IOMMU domain. */
	if (ret != 0) {
		/** Call panic with the following parameters, in order to output fatal failure log and could halt
		 *  current physical CPU.
		 *  - "failed to unassign iommu device!"
		 */
		panic("failed to unassign iommu device!");
	}
}

/**
 * @brief Initialize the given vPCI device's virtual configuration space registers
 *
 * This function is called to initialize the given vPCI virtual configuration space registers. First it reads all the
 * configuration space registers from the physical PCI device associated with the given vPCI, then it handles
 * some specific fields according to the physical PCI device class info. For example, it will hide other capabilities
 * except MSI and clean some unused fields to 00H. Now it just handles two PCI devices: USB/network controller.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init_pt_dev.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void init_default_cfg(struct pci_vdev *vdev)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pci_command representing the value of the PCI command register, not initialized. */
	uint16_t pci_command;
	/** Declare the following local variables of type 'const struct cap_info [2]'.
	 *  - caps representing two capabilities (PM and Advanced Features which will be hidden) ID and length,
	 *  initialized as {0x01U, 8U}, {0x13U, 4U}.
	 */
	const struct cap_info caps[2] = { {0x01U, 8U}, {0x13U, 4U} };
	/** Declare the following local variables of type 'const struct unused_fields'.
	 *  - eth_unused representing the device specific unused fields info of the ethernet controller, initialized
	 *  as {0x80U, 4U}, {0x84U, 4U}, {0x90U, 16U}.
	 */
	const struct unused_fields eth_unused[] = { {0x80U, 4U}, {0x84U, 4U}, {0x90U, 16U} };
	/** Declare the following local variables of type 'const struct unused_fields'.
	 *  - usb_unused representing the device specific unused fields info of the USB controller, initialized
	 *  as {0x90U, 4U}, {0x9CU, 4U}, {0xA0U, 2U}, {0xA8U, 8U}, {0xf8U, 4U}.
	 */
	const struct unused_fields usb_unused[] = { {0x90U, 4U}, {0x9CU, 4U}, {0xA0U, 2U}, {0xA8U, 8U}, {0xf8U, 4U} };

	/** Declare the following local variables of type uint8_t.
	 *  - offset representing the address of the PCI configuration register, not initialized.
	 *  - index representing an index used in different loop, not initialized.
	 */
	uint8_t offset, index;
	/** Declare the following local variables of type uint8_t.
	 *  - max_index representing MAX index of PCI configuration space as 4bytes counting, initialized as
	 *  '(PCI_REGMAX + 1U) >> 2U'.
	 */
	uint8_t max_index = (PCI_REGMAX + 1U) >> 2U;
	/** Declare the following local variables of type uint32_t.
	 *  - val representing the value as 4bytes reading from the PCI configuration space, not initialized. */
	uint32_t val;

	/** Set pci_command to the value returned by pci_pdev_read_cfg with vdev->pbdf, PCIR_COMMAND and 2H being the
	 *  parameters, which reads the command register for the physical PCI device associated with the vPCI device.
	 */
	pci_command = (uint16_t)pci_pdev_read_cfg(vdev->pbdf, PCIR_COMMAND, 2U);

	/** Bitwise OR pci_command by PCIM_CMD_INTXDIS, which is used to disable legacy interrupt. */
	pci_command |= PCIM_CMD_INTXDIS;
	/** Call pci_pdev_write_cfg with the following parameters, in order to update the command register
	 *  to the physical PCI device to disable the legacy interrupt.
	 *  - pbdf
	 *  - offset
	 *  - 2
	 *  - pci_command
	 */
	pci_pdev_write_cfg(vdev->pbdf, PCIR_COMMAND, 2U, pci_command);

	/** For each index ranging from 0 to max_index (63) [with a step of 1] */
	for (index = 0U; index < max_index; index++) {
		/** If index is not in the range of PCI BAR, then read the value from physical configuration space
		 *  and write it into its virtual configuration space.
		 */
		if ((index < (PCIR_BARS/4U)) || (index > (PCIR_BARS/4U + PCI_BAR_COUNT - 1U))) {
			/** Set offset to index << 2, to get the register's address in configuration space. */
			offset = (uint8_t)(index << 2U);
			/** Set val to the value returned by pci_pdev_read_cfg with vdev->pbdf, offset and 4 being the
			 *  parameters, which reads from the physical PCI configuration space.
			 */
			val = pci_pdev_read_cfg(vdev->pbdf, offset, 4U);
			/** Call pci_vdev_write_cfg with the following parameters, in order to write 'val' into the
			 *  given vPCI configuration space.
			 *  - vdev
			 *  - offset
			 *  - 4
			 *  - val
			 */
			pci_vdev_write_cfg(vdev, offset, 4U, val);
		}
	}

	/** Call pci_vdev_write_cfg with the following parameters, in order to write the Interrupt Pin register in
	 *  the virtual configuration space of the given vPCI device.
	 *  - vdev
	 *  - PCIR_INTERRUPT_PIN
	 *  - 1
	 *  - 0
	 */
	pci_vdev_write_cfg(vdev, PCIR_INTERRUPT_PIN, 1U, 0U);
	/** Set val to the value returned by pci_vdev_read_cfg with vdev, PCIR_CAP_PTR and 1 being the parameters,
	 *  to get the PCI device's class number. */
	val = pci_vdev_read_cfg(vdev, PCIR_CLASS, 1U);
	/** If val is PCI_USB_CONTROLLER, which indicates the given vPCI device is a USB controller. */
	if (val == PCI_USB_CONTROLLER) {
		/** For each index ranging from 0 to ARRAY_SIZE(usb_unused) - 1 [with a step of 1] */
		for (index = 0U; index < ARRAY_SIZE(usb_unused); index++) {
			/** Call memset with the following parameters, in order to clean the unused fields to 00H.
			 *  - vdev->cfgdata.data_8 + usb_unused[index].offset
			 *  - 0
			 *  - usb_unused[index].length
			 */
			memset(vdev->cfgdata.data_8 + usb_unused[index].offset, 0U, usb_unused[index].length);
		}
		/** If val is PCI_ETH_CONTROLLER, which indicates the given vPCI device is a ethernet controller. */
	} else if (val == PCI_ETH_CONTROLLER) {
		/** For each index ranging from 0 to ARRAY_SIZE(eth_unused) - 1 [with a step of 1] */
		for (index = 0U; index < ARRAY_SIZE(eth_unused); index++) {
			/** Call memset with the following parameters, in order to clean the unused fields to 00H.
			 *  - vdev->cfgdata.data_8 + eth_unused[index].offset
			 *  - 0
			 *  - eth_unused[index].length
			 */
			memset(vdev->cfgdata.data_8 + eth_unused[index].offset, 0U, eth_unused[index].length);
		}
	}

	/** Set offset to the value returned by pci_vdev_read_cfg with vdev, PCIR_CAP_PTR and 1 being the
	 *  parameters, to get the first capability offset */
	offset = pci_vdev_read_cfg(vdev, PCIR_CAP_PTR, 1U);
	/** Until offset is 0 or FFH, which means looping until no capability found. */
	while ((offset != 0U) && (offset != 0xFFU)) {
		/** Declare the following local variables of type uint8_t.
		 *  - cap representing the ID of one capability, initialized as the value returned by pci_vdev_read_cfg
		 *  with vdev, offset + PCICAP_ID and 1 being the parameters, which reads the capability's ID from the
		 *  configuration space.
		 */
		uint8_t cap = pci_vdev_read_cfg(vdev, offset + PCICAP_ID, 1U);
		/** Declare the following local variables of type uint8_t.
		 *  - next representing the offset for next capability, initialized as the value returned by
		 *  pci_vdev_read_cfg with vdev, offset + PCICAP_NEXTPTR and 1 being the parameters,
		 *  which reads the offset of next capability from the configuration space.
		 */
		uint8_t next = pci_vdev_read_cfg(vdev, offset + PCICAP_NEXTPTR, 1U);

		/** If cap is PCIY_MSI, it exposes MSI capability and hides others. */
		if (cap == PCIY_MSI) {
			/** Call pci_vdev_write_cfg with the following parameters, in order to write the MSI
			 *  capability's offset to the register of PCIR_CAP_PTR, which links the first capability
			 *  to the MSI capability.
			 *  - vdev
			 *  - PCIR_CAP_PTR
			 *  - 1
			 *  - offset
			 */
			pci_vdev_write_cfg(vdev, PCIR_CAP_PTR, 1U, offset);
		} else {
			/** If cap equals caps[0].id */
			if (cap == caps[0].id) {
				/** Set index to 0 */
				index = 0U;
				/** If cap equals caps[1].id */
			} else if (cap == caps[1].id) {
				/** Set index to 1 */
				index = 1U;
			} else {
				/** Logging the following information with a log level of LOG_FATAL.
				 *  - "CAP: %d, not handled, please check!\n"
				 *  - cap
				 */
				pr_fatal("CAP: %d, not handled, please check!\n", cap);
			}

			/** Call memset with the following parameters, in order to clean the hidden capability structure
			 *  fields to 00H.
			 *  - vdev->cfgdata.data_8 + offset
			 *  - 0
			 *  - caps[index].length
			 */
			memset(vdev->cfgdata.data_8 + offset, 0U, caps[index].length);
		}

		/** Set offset to next, to probe next capability */
		offset = next;
	}

	/** Set offset to the value returned by pci_vdev_read_cfg with vdev, PCIR_CAP_PTR and 1 being the parameters,
	 *  which should be the MSI capability offset and its next capability link is set to 00H.
	 */
	offset = pci_vdev_read_cfg(vdev, PCIR_CAP_PTR, 1U);
	/* If offset is less than 0xFF, which means the CAP_PTR register points to a capability structure (which should
	 * be MSI in normal cases) */
	if (offset < 0xFFU) {
		/** Call pci_vdev_write_cfg with the following parameters, in order to write 0 to MSI_CAP->next CAP
		 *  offset, for other capabilities are hidden.
		 *  - vdev
		 *  - offset + PCICAP_NEXTPTR
		 *  - 1
		 *  - 0
		 */
		pci_vdev_write_cfg(vdev, offset + PCICAP_NEXTPTR, 1U, 0U);
	}
}


/**
 * @brief Initialize the passthrough PCI device associated with the given vPCI
 *
 * This function is called to initialize the passthrough PCI device associated with the given vPCI device. It will
 * call internal functions to initialize the given vPCI's virtual configuration space, virtual MSI and BAR remapping.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal callback API called by vpci_init_vdev.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vpci_init_pt_dev(struct pci_vdev *vdev)
{
	/** Call init_default_cfg with the following parameters, in order to initialize the virtual configuration
	 *  space of the given vPCI device.
	 *  - vdev
	 */
	init_default_cfg(vdev);

	/** Call init_vmsi with the following parameters, in order to initialize the virtual MSI fields of the given
	 *  VPCI device.
	 *  - vdev
	 */
	init_vmsi(vdev);
	/** Call init_vdev_pt with the following parameters, in order to initialize the virtual BAR info of the
	 *  given vPCI device, and do the BAR mapping between the guest VM's memory space (GPA) and HPA according to
	 *  the guest VM's configuration for the vBAR base of the given vPCI device.
	 *  - vdev
	 */
	init_vdev_pt(vdev);

	/** Call assign_vdev_pt_iommu_domain with the following parameters, in order to add the physical PCI device
	 *  associated with the given vPCI to the VM's IOMMU domain.
	 *  - vdev
	 */
	assign_vdev_pt_iommu_domain(vdev);
}

/**
 * @brief Release the resource of the passthrough PCI device associated with the given vPCI device
 *
 * This function is called to release the resource of the passthrough PCI device associated with the given vPCI device.
 * It will call internal functions to remove the device from the VM's IOMMU domain, and remove MSI remapping.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 *
 * @return None.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal callback API called by vpci_cleanup.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vpci_deinit_pt_dev(struct pci_vdev *vdev)
{
	/** Call remove_vdev_pt_iommu_domain with the following parameters, in order to remove the physical
	 *  PCI device associated with the given vPCI device from its VM's IOMMU domain.
	 *  - vdev
	 */
	remove_vdev_pt_iommu_domain(vdev);
	/** Call deinit_vmsi with the following parameters, in order to remove the MSI remapping.
	 *  - vdev
	 */
	deinit_vmsi(vdev);
}

/**
 * @brief Get a BAR index according to its offset in the PCI configuration space
 *
 * This function is called to get a BAR index according to its offset in the PCI configuration space. The offset range
 * of the BAR registers is from 10H to 27H and each BAR's length is 4 bytes, so its index is from 0 to 5.
 *
 * @param[in] offset The BAR register offset in PCI configuration space
 *
 * @return The index of a BAR
 *
 * @pre offset >= 10H && offset < 28H
 * @pre (offset & 3H) == 0
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @remark It is an internal function called within vPCI.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static inline uint32_t pci_bar_index(uint32_t offset)
{
	/** Return the BAR index whose range is from 0 to 5 */
	return (offset - PCIR_BARS) >> 2U;
}

/**
 * @brief Write a value to a configuration register of the given vPCI device
 *
 * This function is called to write a value to a configuration register of the given vPCI device, which is associated
 * with a physical PCI device. It need handle MSI / BAR /command registers writing and igore others.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length of the register to write.
 * @param[in] val The value to write.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre offset < PCI_REGMAX
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal callback API called by write_cfg.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vpci_write_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/** If it is true returned by vbar_access with vdev and offset being the parameters, which indicates the
	 *  accessing register is a BAR register. */
	if (vbar_access(vdev, offset)) {
		/** If bytes is 4 and (offset & 3H) == 0), which means that BAR access must be 4 bytes and
		 *  offset must also be 4 bytes aligned.
		 */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			/** Call vdev_pt_write_vbar with the following parameters, in order to handle BAR register
			 *  writing operation, which could do BAR remapping.
			 *  - vdev
			 *  - pci_bar_index(offset)
			 *  - val
			 */
			vdev_pt_write_vbar(vdev, pci_bar_index(offset), val);
		}
	/** If it's true returned by msicap_access with vdev and offset being the parameters, which means the
	 *  accessing register is within the MSI capability */
	} else if (msicap_access(vdev, offset)) {
		/** Call vmsi_write_cfg with the following parameters, in order to handle MSI capability register
		 *  writing operation, which could do MSI interrupt remapping work.
		 * - vdev
		 * - offset
		 * - bytes
		 * - val
		 */
		vmsi_write_cfg(vdev, offset, bytes, val);
	/** If offset is in the range between 04H and 07H, which includes command and status registers. */
	} else if ((offset >= PCIR_COMMAND) && (offset < PCIR_REVID)) {
		/** If offset is 05H and bytes is 1, which means the high byte of the command register. */
		if ((offset == (PCIR_COMMAND + 1U)) && (bytes == 1U)) {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write command register
			 *  to the configuration space of the physical PCI device associated with the given vPCI device.
			 *  But the legacy interrupt should be disabled, so the bit2 should be set by default.
			 *  - vdev->pbdf
			 *  - offset
			 *  - bytes
			 *  - val | (PCIM_CMD_INTXDIS >> 16U)
			 */
			pci_pdev_write_cfg(vdev->pbdf, offset, bytes, val | (PCIM_CMD_INTXDIS >> 16U));
		/** If offset is 04H and bytes is 2 or 4, which means
		 *  all the bytes of command register or command and status registers. */
		} else if ((offset == PCIR_COMMAND) && ((bytes == 2U) || (bytes == 4U))) {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write command register
			 *  or may includes status register to the configuration space of the physical PCI device
			 *  associated with the given vPCI device. But the legacy interrupt should be disabled,
			 *  so the bit10 should be set by default.
			 *  - vdev->pbdf
			 *  - offset
			 *  - bytes
			 *  - val | PCIM_CMD_INTXDIS
			 */
			pci_pdev_write_cfg(vdev->pbdf, offset, bytes, val | PCIM_CMD_INTXDIS);
		} else {
			/** Call pci_pdev_write_cfg with the following parameters, in order to write command or status
			 *  register to the configuration space of the physical PCI device
			 *  associated with the given vPCI device.
			 *  - vdev->pbdf
			 *  - offset
			 *  - bytes
			 *  - val
			 */
			pci_pdev_write_cfg(vdev->pbdf, offset, bytes, val);
		}
	} else {
		/* ignore other writing */
		/** Logging the following information with a log level of LOG_DEBUG.
		 *  - "pci write: bdf=%d, offset=0x%x, val=0x%x, bytes=%d\n"
		 *  - vdev->pbdf
		 *  - offset
		 *  - val
		 *  - bytes
		 */
		pr_dbg("pci write: bdf=%d, offset=0x%x, val=0x%x, bytes=%d\n",
			vdev->pbdf, offset, val, bytes);
	}
}

/**
 * @brief Read a configuration register from the given vPCI device
 *
 * This function is called to read a configuration register from the given vPCI device, which is associated
 * with a physical PCI device. It need handle BAR / command registers separately.
 *
 * @param[in] vdev A vPCI device which is associated with a physical PCI device
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length of the register to read.
 * @param[out] val The pointer to store the value to read.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre offset < PCI_REGMAX
 * @pre bytes == 1 || bytes == 2 || bytes == 4
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal callback API called by read_cfg.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vdev->vpci->vm is different among parallel invocation.
 */
static void vpci_read_pt_dev_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/** If it is true returned by vbar_access with vdev and offset being the parameters, which indicates the
	 *  accessing register is a BAR register. */
	if (vbar_access(vdev, offset)) {
		/** If bytes is 4 and (offset & 3H) == 0), which means that BAR access must be 4 bytes and
		 *  offset must also be 4 bytes aligned.
		 */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			/** Set '*val' to the value returned by pci_vdev_read_bar with vdev and the return value of
			 *   pci_bar_index (with offset) being the parameters, which reads a BAR register of
			 *  the given vPCI device. */
			*val = pci_vdev_read_bar(vdev, pci_bar_index(offset));
		} else {
			/** Set '*val' to 0FFFFFFFFH, which is a default returned value if the access is invalid. */
			*val = ~0U;
		}
	/** If offset is in the range between 04H and 07H, which includes command and status registers. */
	} else if ((offset >= PCIR_COMMAND) && (offset < PCIR_REVID)) {
		/** Set '*val' to the value returned by pci_pdev_read_cfg with vdev->pbdf, offset and bytes being the
		 *  parameters, which reads the command/status registers from the configuration space of
		 *  the physical PCI device associated with the given vPCI device.
		 */
		*val = pci_pdev_read_cfg(vdev->pbdf, offset, bytes);
	} else {
		/** Set '*val' to the value returned by pci_vdev_read_cfg with vdev, offset and bytes being the
		 *  parameters, which reads other registers of the given vPCI device from its virtual
		 *  configuration space.
		 */
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
	}
}

/**
* @brief A set of callback functions used to operate the physical PCI device associated with a vPCI device.
**/
static const struct pci_vdev_ops pci_pt_dev_ops = {
	.init_vdev = vpci_init_pt_dev,   /**< The function to initialize the device. */
	.deinit_vdev = vpci_deinit_pt_dev,  /**< The function to de-init the device. */
	.write_vdev_cfg = vpci_write_pt_dev_cfg, /**< The function to write the configuration register. */
	.read_vdev_cfg = vpci_read_pt_dev_cfg,  /**< The function to read the configuration register. */
};

/**
 * @brief Read a configuration register from a vPCI device associated with the given BDF
 *
 * This function is called to read a configuration register from a vPCI device with the given BDF. It will
 * use the given BDF to probe the vPCI device from the given vPCI devices list first, and then call its
 * reading function to do the work.
 *
 * @param[in] vpci A pointer to a list of vPCI devices
 * @param[in] bdf The BDF of the vPCI device to operate
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length of the register to read.
 * @param[out] val The pointer to store the value to read.
 *
 * @return None
 *
 * @pre vpci != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by pci_cfgdata_io_read.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/** Declare the following local variables of type 'struct pci_vdev *'.
	 *  - vdev representing a vPCI device whose BDF is the given one, not initialized. */
	struct pci_vdev *vdev;

	/** Call spinlock_obtain with the following parameters, in order to avoid different vCPUs of one VM to access
	 *  its PCI configuration space in parallel.
	 *  - &vpci->lock
	 */
	spinlock_obtain(&vpci->lock);
	/** Set vdev to the value returned by pci_find_vdev with vpci and bdf being the parameters, which finds
	 *  a vPCI device with the given BDF. */
	vdev = pci_find_vdev(vpci, bdf);
	/** If 'vdev' is not NULL */
	if (vdev != NULL) {
		/** Call vdev->vdev_ops->read_vdev_cfg with the following parameters, in order to call its callback
		 *  function to read its register.
		 *  - vdev
		 *  - offset
		 *  - bytes
		 *  - val
		 */
		vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	}
	/** Call spinlock_release with the following parameters, in order to unlock the access to the vPCI
	 *  configuration space.
	 *  - &vpci->lock
	 */
	spinlock_release(&vpci->lock);
}

/**
 * @brief Write a value to a configuration register of a vPCI device with the given BDF
 *
 * This function is called to write a value to a configuration register of a vPCI device with the given BDF.
 * It will use the given BDF to probe the vPCI device from the given vPCI devices list first, and then call
 * its writing function to do the work.
 *
 * @param[in] vpci A pointer to a list of vPCI devices
 * @param[in] bdf The BDF of the vPCI device to operate
 * @param[in] offset The register offset in the PCI configuration space.
 * @param[in] bytes The length of the register to write.
 * @param[out] val The value to write.
 *
 * @return None
 *
 * @pre vpci != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal API called by pci_cfgdata_io_write.
 *
 * @reentrancy unspecified
 * @threadsafety Yes
 */
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/** Declare the following local variables of type 'struct pci_vdev *'.
	 *  - vdev representing a vPCI device whose BDF is the given one, not initialized. */
	struct pci_vdev *vdev;

	/** Call spinlock_obtain with the following parameters, in order to avoid different vCPUs of one VM to access
	 *  its PCI configuration space in parallel.
	 *  - &vpci->lock
	 */
	spinlock_obtain(&vpci->lock);
	/** Set vdev to the value returned by pci_find_vdev with vpci and bdf being the parameters, which is to find
	 *  a vPCI device with the given BDF. */
	vdev = pci_find_vdev(vpci, bdf);
	/** If 'vdev' is not NULL */
	if (vdev != NULL) {
		/** Call vdev->vdev_ops->write_vdev_cfg with the following parameters, in order to call its registered
		 *  function to write its register.
		 *  - vdev
		 *  - offset
		 *  - bytes
		 *  - val
		 */
		vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	}
	/** Call spinlock_release with the following parameters, in order to unlock the access to the vPCI
	 *  configuration space.
	 *  - &vpci->lock
	 */
	spinlock_release(&vpci->lock);
}

/**
 * @brief Allocate a vPCI instance with the info from the given device configuration
 *
 * This function is called to allocate a vPCI instance with the info from the given device configuration.
 * First it allocates an instance from the given vPCI devices list, then fills the info according to the given
 * device configuration.
 *
 * @param[inout] vpci A pointer to a list of vPCI devices
 * @param[in] dev_config The device configuration info of the VM.
 *
 * @return None
 *
 * @pre vpci != NULL
 * @pre vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init_vdevs.
 *
 * @reentrancy unspecified
 * @threadsafety When \a vpci->vm is different among parallel invocation.
 */
static void vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config)
{
	/** Declare the following local variables of type 'struct pci_vdev *'.
	 *  - vdev representing an instance allocated from vPCI devices list, initialized as
	 *  &vpci->pci_vdevs[vpci->pci_vdev_cnt].
	 */
	struct pci_vdev *vdev = &vpci->pci_vdevs[vpci->pci_vdev_cnt];

	/** Increment vpci->pci_vdev_cnt by 1 */
	vpci->pci_vdev_cnt++;
	/** Set vdev->vpci to vpci */
	vdev->vpci = vpci;
	/** Set vdev->bdf.value to dev_config->vbdf.value */
	vdev->bdf.value = dev_config->vbdf.value;
	/** Set vdev->pbdf to dev_config->pbdf */
	vdev->pbdf = dev_config->pbdf;
	/** Set vdev->pci_dev_config to dev_config */
	vdev->pci_dev_config = dev_config;

	/** If dev_config->vdev_ops is not NULL */
	if (dev_config->vdev_ops != NULL) {
		/** Set vdev->vdev_ops to dev_config->vdev_ops */
		vdev->vdev_ops = dev_config->vdev_ops;
	} else {
		/** Set vdev->vdev_ops to &pci_pt_dev_ops, which is for physical PCI device */
		vdev->vdev_ops = &pci_pt_dev_ops;
		/** Assert that dev_config->emu_type is PCI_DEV_TYPE_PTDEV */
		ASSERT(dev_config->emu_type == PCI_DEV_TYPE_PTDEV,
			"Only PCI_DEV_TYPE_PTDEV could not configuration vdev_ops");
	}

	/** Call vdev->vdev_ops->init_vdev with the following parameters, in order to initialzie the vPCI device.
	 *  - vdev */
	vdev->vdev_ops->init_vdev(vdev);
}

/**
 * @brief Initialize all the vPCI devices which belongs to the given VM
 *
 * This function is called to initialize all the vPCI devices which belongs to the given VM. It will get the vPCI
 * devices list configuration in the VM and then will call internal function to initialize the vPCI device one by one.
 *
 * @param[inout] vm A pointer to a VM instance whose vPCI devices are to be initialized.
 *
 * @return None.
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is an internal API called by vpci_init.
 *
 * @reentrancy unspecified
 *
 * @threadsafety When \a vm is different among parallel invocation.
 */
static void vpci_init_vdevs(struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint32_t.
	 *  - idx representing the index to probe the vPCI device in its configuration list, not initialized. */
	uint32_t idx;
	/** Declare the following local variables of type 'struct acrn_vpci *'.
	 *  - vpci representing the pointer to the vPCI list of the given VM, initialized as &(vm->vpci). */
	struct acrn_vpci *vpci = &(vm->vpci);
	/** Declare the following local variables of type 'const struct acrn_vm_config *'.
	 *  - vm_config representing the configuration data of the given VM, initialized as the value returned
	 *  by get_vm_config(vpci->vm->vm_id).
	 */
	const struct acrn_vm_config *vm_config = get_vm_config(vpci->vm->vm_id);

	/** For each idx ranging from 0 to vm_config->pci_dev_num - 1 [with a step of 1] */
	for (idx = 0U; idx < vm_config->pci_dev_num; idx++) {
		/** Call vpci_init_vdev with the following parameters, in order to initialize the vPCI device according
		 *  to its configuration data in the VM.
		 *  - vpci
		 *  - &vm_config->pci_devs[idx]
		 */
		vpci_init_vdev(vpci, &vm_config->pci_devs[idx]);
	}
}

/**
 * @}
 */
