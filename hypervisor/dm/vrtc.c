/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <logmsg.h>
#include <vm_reset.h>

/**
 * @defgroup vp-dm vp-dm
 * @brief The component implementing virtual peripherals
 *
 * The vp-dm component is responsible for handling accesses to virtual peripherals and pass-through devices.
 *
 * This component is above the boot, lib and hwmgmt component and at the same level as the vp-base component. The
 * functionality of this component relies on the boot component for a proper C execution environment, the lib component
 * for mutual exclusion, bitmap and string operations and the hwmgmt component for updating virtual machine control
 * structures, operating DMA remapping units and scheduling vCPUs. It relies on the vp-base component to get the values
 * in vCPU registers.
 *
 * Refer to section 10.4 of Software Architecture Design Specification for the detailed decomposition of this component
 * and section 11.5 for the external APIs of each module inside this component.
 */

/**
 * @defgroup vp-dm_vperipheral vp-dm.vperipheral
 * @ingroup vp-dm
 * @brief Implementation of all external APIs to virtualize RTC and PCI devices.
 *
 * This module implements the virtualization of RTC (vRTC) and PCI devices (vPCI). The vRTC and vPCI initial function
 * will register their port IO access callback functions. So when a guest VM accesses its RTC or PCI configuration
 * space by port IO, it would cause VM exit and then call their registered functions.
 *
 * The vRTC will read the physical RTC register value and return to the guest VM. For vPCI, it includes a emulated PCI
 * hostbridge (vhostbridge) and some passthrough PCI devices. A guest VM can read the vhostbridge configuration
 * space registers, but can't modify them. To a passthrough PCI device, the guest VM also can read all the registers
 * in the configuration space. And hypervisor would decide which register can be modified. The main registers are
 * related with interrupt (MSI) and BAR remapping implementation.
 *
 * Also vPCI just exposes the passthrough device's MSI capability to the guest VM and hide other capabilities. Some
 * unused registers in configuration space are initialized as 00H.
 *
 * Usage:
 * - 'vp-base.vm' module depends on this module to initialize a vRTC and vPCI devices for each VM.
 * - 'vp-base.hv_main' module depends on this module to access vRTC and vPCI configuration space.
 *
 * Dependency:
 * - This module depends on 'hwmgmt.io' module to access physical IO port.
 * - This module depends on 'lib.lock' module to lock/unlock when accessing physical IO port to avoid different
 * guest VMs to access the IO port in parallel.
 * - This module depends on 'hwmgmt.time' module to do some delay.
 * - This module depends on 'vp-base.vm' module to check whether a specified VM is a safety VM, and to shutdown a VM
 * if a fatal error happens.
 * - This module depends on 'vp-base.guest_mem' module to do BAR remapping for the passthrough PCI device.
 * - This module depends on 'vp-dm.io_req' module to register port IO access callback functions for vRTC and vPCI.
 * - This module depends on 'vp-dm.ptirq' module to do MSI remapping.
 * - This module depends on 'hwmgmt.iommu' module to add the passthrough PCI device into IOMMU domain.
 * - This module depends on 'hwmgmt.pci' module to access the passthrough PCI device's physical configuration space.
 * - This module depends on 'hwmgmt.configs' module to get VM configuration data of one VM's PCI devices.
 * - This module depends on 'lib.util' module to do memory initialization.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by vRTC.
 *
 * This file implements all external functions that shall be provided by the vRTC part of vp-dm.vperipheral module.
 * The vRTC implementation is based on the native physical RTC.
 *
 * It defines one initial function and two callback functions (read/write) registered for RTC port IO access.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines one decomposed function to improve the readability of the code.
 *
 * Helper functions include: cmos_read, cmos_update_in_progress.
 *
 * Decomposed function: cmos_get_reg_val
 */

#define CMOS_ADDR_PORT 0x70U /**< Pre-defined port IO to access RTC register address. */
#define CMOS_DATA_PORT 0x71U /**< Pre-defined port IO to access RTC register data. */

#define RTC_STATUSA 0x0AU /**< Pre-defined RTC status register. */
#define RTCSA_TUP   0x80U /**< Pre-defined the value to indicate that RTC is in updating status. */

/**
* @brief Local spinlock_t variable used to avoid the physical RTC is accessed by different guest VMs in parallel.
**/
static spinlock_t cmos_lock;

/**
 * @brief Read a value from RTC register according to the given register address
 *
 * This function is called to read a value from RTC register according to the given register address.
 *
 * @param[in] addr The address of the RTC register to read.
 *
 * @return The value of a RTC register.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal function called within vRTC.
 *
 * @reentrancy unspecified
 *
 * @threadsafety No
 */
static uint8_t cmos_read(uint8_t addr)
{
	/** Call pio_write8 with the following parameters, in order to write the RTC register address into its
	 *  corresponding IO port.
	 *  - addr
	 *  - CMOS_ADDR_PORT
	 */
	pio_write8(addr, CMOS_ADDR_PORT);
	/** Return the register's value returned by pio_read8(CMOS_DATA_PORT) */
	return pio_read8(CMOS_DATA_PORT);
}


/**
 * @brief Check whether the RTC register is in updating status
 *
 * This function is called to check whether the RTC register is in updating status.
 *
 * @return A boolean value to indicate whether the RTC register is in updating status
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal function called within vRTC.
 *
 * @reentrancy unspecified
 *
 * @threadsafety No
 */
static bool cmos_update_in_progress(void)
{
	/** Return true if and only if the RTC status register's value (returned by cmos_read with RTC_STATUSA being
	 *  the parameter) masked by RTCSA_TUP is not 0H. */
	return (cmos_read(RTC_STATUSA) & RTCSA_TUP) ? true : false;
}

/**
 * @brief Get a physical RTC register's value
 *
 * This function is called to read a physical RTC register's value. It will check the physical RTC is updating first,
 * and if not, then read it. A spin lock is used to avoid that the physical RTC is accessed by different guest VMs
 * in parallel.
 *
 * @param[in] addr The address of the RTC register to read.
 * @param[out] value The pointer to save the value read from the RTC register.
 *
 * @return A boolean value to indicate whether it is successful to read the RTC register.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is an internal function called by vrtc_read.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static bool cmos_get_reg_val(uint8_t addr, uint32_t *value)
{
	/** Declare the following local variables of type bool.
	 *  - result representing the status of reading the physical RTC register, initialized as true. */
	bool result = true;
	/** Declare the following local variables of type int32_t.
	 *  - tries representing the total times to check the RTC updating status, initialized as 3000. */
	int32_t tries = 3000;
	/** Declare the following local variables of type bool.
	 *  - is_default representing whether a default value of the RTC register at index \a addr is specified,
	 *  initialized as false. */
	bool is_default = false;

	/** Depending on the address of the RTC register. */
	switch (addr) {
	/** \a addr is 0h, representing the RTC register whose functionality is to get the current second. */
	case 0x0U:
		/** End of case */
		break;
	/** \a addr is 1h, representing the RTC register whose functionality is to alarm second. */
	case 0x1U:
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0U;
		/** End of case */
		break;
	/** \a addr is 2h, representing the RTC register whose functionality is to get the current minute. */
	case 0x2U:
		/** End of case */
		break;
	/** \a addr is 3h, representing the RTC register whose functionality is to alarm minute. */
	case 0x3U:
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0U;
		/** End of case */
		break;
	/** \a addr is 4h, representing the RTC register whose functionality is to get the current hour. */
	case 0x4U:
		/** End of case */
		break;
	/** \a addr is 5h, representing the RTC register whose functionality is to alarm hour. */
	case 0x5U:
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	/** \a addr is 6h, representing the RTC register whose functionality is to get the current day of
	 * week(1=Sunday). */
	case 0x6U:
	/** \a addr is 7h, representing the RTC register whose functionality is to get the current date of month. */
	case 0x7U:
	/** \a addr is 8h, representing the RTC register whose functionality is to get the current month. */
	case 0x8U:
	/** \a addr is 9h, representing the RTC register whose functionality is to get the current year. */
	case 0x9U:
		/** End of case */
		break;
	/** \a addr is Ah, representing the RTC register used for general configuration of the RTC functions. */
	case 0xAU:
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0x0U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	/** \a addr is Bh, representing the RTC register used for general configuration of the RTC functions. */
	case 0xBU:
		/** Set *value to 2h, which means this RTC register has a default value 2h. */
		*value = 0x2U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	/** \a addr is Ch, representing the RTC Flag Register. */
	case 0xCU:
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0x0U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	/** \a addr is Dh, representing the RTC Flag Register. */
	case 0xDU:
		/** Set *value to 80h, which means this RTC register has a default value 80h. */
		*value = 0x80U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	/** \a addr is not matched, representing invalid RTC register. */
	default:
		/** Set *value to 0h, which means this RTC register has a default value 0h. */
		*value = 0U;
		/** Set is_default to true, which means this RTC register has a default value prepared for guest. */
		is_default = true;
		/** End of case */
		break;
	}

	/** If the RTC register specified by \a addr doesn't have default value. */
	if (!is_default) {
		/** Call spinlock_obtain with the following parameters, in order to avoid the physical RTC register is
		 *  accessed by different guest VMs in parallel.
		 *  - &cmos_lock
		 */
		spinlock_obtain(&cmos_lock);

		/** Until it's false of the value returned by cmos_update_in_progress or the 'tries' count is reduced
		 * to 0; the first condition means the RTC register updating is done, the second condition indicates
		 * that the RTC is in updating status for too long time.
		 */
		while (cmos_update_in_progress() && (tries != 0)) {
			/** Decrement tries by 1 */
			tries -= 1;
			/** Call udelay with the following parameters, in order to delay 10 microseconds.
			 *  - 10
			 */
			udelay(10U);
		}

		/** If 'tries' is less than 0 or equals to 0, which means it has tried for a long time. */
		if (tries <= 0) {
			/** Set result to false, which means it is failed to read the physical RTC register this
			 * time. */
			result = false;
		} else {
			/** Set '*value' to the value returned by cmos_read with addr being the parameter, which is
			 *  to read the physical RTC register. */
			*value = cmos_read(addr);
		}

		/** Call spinlock_release with the following parameters, in order to release the lock, then other guest
		 * VM can access the physical RTC register.
		 *  - &cmos_lock
		 */
		spinlock_release(&cmos_lock);
	}
	/** Return 'result' to indicate whether this reading is successful. */
	return result;
}


/**
 * @brief Read a RTC register value
 *
 * This is a vRTC port IO registered function and is called when a guest VM reads a register of its RTC.
 * It can read the address port register or the data port register.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to read its vRTC register.
 * @param[in] addr The IO address of the accessing port.
 * @param[in] width The data width to read, not used here.
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 70H || addr == 71H
 * @pre width == 1H || width == 2H || width == 4H
 * @pre vcpu->vm->vrtc_offset >= 0H && vcpu->vm->vrtc_offset <= FFH
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is the registered function called when a guest VM reads its RTC IO ports.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void vrtc_read(struct acrn_vcpu *vcpu, uint16_t addr, __unused size_t width)
{
	/** Declare the following local variables of type uint8_t.
	 *  - offset representing the RTC register to read, not initialized. */
	uint8_t offset;
	/** Declare the following local variables of type 'struct pio_request *'.
	 *  - pio_req representing a port IO request from the given vCPU, initialized as &vcpu->req.reqs.pio. */
	struct pio_request *pio_req = &vcpu->req.reqs.pio;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing the pointer to the VM instance the given vCPU belongs to, initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;

	/** Set offset to vm->vrtc_offset, which is set when a guest VM writes the register to its address port. */
	offset = vm->vrtc_offset;

	/** If the given 'addr' is CMOS_ADDR_PORT, to indicate that the guest VM is reading the address port. */
	if (addr == CMOS_ADDR_PORT) {
		/** Set pio_req->value to 0, the default value of the RTC register whose address is 70h is 0h.
		 */
		pio_req->value = 0U;
	} else {
		/** Declare the following local variables of type bool.
		 *  - ret representing the status returned by cmos_get_reg_val with offset and &(pio_reg->value) being
		 *  the parameters, which is to read the physical RTC's data register, initialized as the function's
		 *  returned value.
		 */
		bool ret = cmos_get_reg_val(offset, &(pio_req->value));

		/** If 'ret' is false, which means it is failed to read the physical RTC's data register. */
		if (!ret) {
			/** If 'vm' is safety VM (determined by calling is_safety_vm with vm) */
			if (is_safety_vm(vm)) {
				/** Call panic with the following parameters, in order to output log info and
				 *  suspend this vCPU.
				 *  -"read rtc timeout, system exception!"
				 */
				panic("read rtc timeout, system exception!");
			} else {
				/** Call fatal_error_shutdown_vm with the following parameters, in order to shutdown the
				 *  VM (corresponding to \a vcpu) if the VM is not safety VM.
				 *  - vcpu
				 */
				fatal_error_shutdown_vm(vcpu);
			}
		}
	}
}

/**
 * @brief Write a value to a RTC register
 *
 * This is a vRTC port IO registered function and is called when a guest VM writes a value to a register of its RTC.
 * It just supports to write the address port register.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write its vRTC register.
 * @param[in] addr The IO address of the accessing port.
 * @param[in] width The data width to write.
 * @param[in] value The value to write.
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre addr == 70H || addr == 71H
 * @pre width == 1H || width == 2H || width == 4H
 * @pre vcpu->vm->vrtc_offset >= 0H && vcpu->vm->vrtc_offset <= FFH
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark It is the registered function called when a guest VM write its RTC IO ports.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
static void vrtc_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width, uint32_t value)
{
	/** If 'width' is 1 and 'addr' is CMOS_ADDR_PORT, which means it just support to write its address IO port. */
	if ((width == 1U) && (addr == CMOS_ADDR_PORT)) {
		/** Set vcpu->vm->vrtc_offset to 'value & 0x7FU', which filters the valid address register value. */
		vcpu->vm->vrtc_offset = (uint8_t)value & 0x7FU;
	}
}

/**
 * @brief Initialize the vRTC of the given VM
 *
 * This function is called to initialize the vRTC of the given VM. It regiters the vRTC port IO read/write
 * callback functions, which are called when the guest VM accesses its RTC port IO.
 *
 * @param[inout] vm A pointer to a virtual machine data structure whose vRTC is to be initialized.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark It is a public API called by vp-base.vm module.
 *
 * @reentrancy unspecified
 *
 * @threadsafety Yes
 */
void vrtc_init(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct vm_io_range'.
	 *  - range representing the RTC port IO range, initialized as '.base=0x70, .len=2'. */
	struct vm_io_range range = { .base = CMOS_ADDR_PORT, .len = 2U };

	/** Set vm->vrtc_offset to 0, which is to initialize the address (the offset of the RTC CMOS RAM) to 0 */
	vm->vrtc_offset = 0U;

	/** Call register_pio_emulation_handler with the following parameters, in order to register the given VM's RTC
	 *  port IO access callback functions.
	 *  - vm
	 *  - RTC_PIO_IDX
	 *  - &range
	 *  - vrtc_read
	 *  - vrtc_write
	 */
	register_pio_emulation_handler(vm, RTC_PIO_IDX, &range, vrtc_read, vrtc_write);

	/** Call spinlock_init with the following parameters, in order to initialize the RTC lock
	 *  to prevent simultaneous access to the physical RTC.
	 *  - &cmos_lock
	 */
	spinlock_init(&cmos_lock);
}

/**
 * @}
 */
