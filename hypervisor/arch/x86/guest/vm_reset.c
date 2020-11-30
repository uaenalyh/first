/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <vm_reset.h>

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file defines the APIs of requesting and handling VM shutdown.
 *
 * This file is decomposed into the following functions:
 *     fatal_error_shutdown_vm  -Do partial shutdown operation on the vm which vcpu belongs.
 *     shutdown_vm_from_idle    -Shutdown the vm that masked to be shutdown.
 */

/**
 * @brief  Do partial shutdown operation on the vm which \a vcpu belongs.
 *
 * @param[in]   vcpu The vCPU that encounters a fatal error such as a triple fault
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API can be called only after launch_vms has been called once on the processor and
 * the physical CPU id of this processor is ffs64(get_vm_config(vcpu->vm->vm_id)->pcpu_bitmap)
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
void fatal_error_shutdown_vm(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct acrn_vm *.
	 *  - vm representing the virtual machine of the vcpu, initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;

	/** Call spinlock_obtain with the following parameter, in order to acquire the spinlock for protecting
	 *  simultaneous VM state transition requests
	 *  - &vm->vm_lock
	 */
	spinlock_obtain(&vm->vm_lock);

	/** Call pause_vm with the following parameters, in order to pause the virtual machine.
	 *  - vm */
	pause_vm(vm);

	/** Call spinlock_release with the following parameter, in order to release the spinlock for protecting
	 *  simultaneous VM state transition requests
	 *  - &vm->vm_lock
	 */
	spinlock_release(&vm->vm_lock);

	/** set the shutdown_vm_id field of per-CPU region of the cpu to the id of the virtual machine
	 *  which will be shutdown by hypervisor, where the cpu is where the given vcpu runs on. */
	per_cpu(shutdown_vm_id, pcpuid_from_vcpu(vcpu)) = vm->vm_id;
	/** Call make_shutdown_vm_request with the following parameters, in order to send shutdown message to the
	 *  physical cpu that the given vcpu runs on.
	 *  - pcpuid_from_vcpu(vcpu) */
	make_shutdown_vm_request(pcpuid_from_vcpu(vcpu));
}

/**
 * @brief shutdown the vm that masked to be shutdown
 *
 * The vm that masked to be shutdown is the one whose vm id equals to the value of shutdown_vm_id field of the
 * per-CPU region of physical cpu whose id is pcpu_id
 *
 * @param[in]    pcpu_id The physical CPU ID whose per-CPU region records the ID of the VM to be shut down
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API can be called only after launch_vms has been called once on the processor and
 * the physical CPU id of this processor is ffs64(get_vm_config(vcpu->vm->vm_id)->pcpu_bitmap)
 *
 * @reentrancy unspecified
 * @threadsafety unspecified
 */
void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	/** Declare the following local variables of type struct acrn_vm *.
	 *  - vm representing the virtual machine whose VM ID equals to the value of shutdown_vm_id field of per-CPU
	 *    region of physical cpu whose id is pcpu_id, initialized as
	 *    get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id). */
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));

	/** Call spinlock_obtain with the following parameter, in order to acquire the spinlock for protecting
	 *  simultaneous VM state transition requests
	 *  - &vm->vm_lock
	 */
	spinlock_obtain(&vm->vm_lock);

	/** If vm->state is VM_PAUSED, indicating that the VM has been paused. */
	if (vm->state == VM_PAUSED) {
		/** Call shutdown_vm with the following parameters, in order to shutdown the virtual machine, and
		 *  discard its return value because the return value of shutdown_vm is always 0.
		 *  - vm */
		(void)shutdown_vm(vm);
	}

	/** Call spinlock_release with the following parameter, in order to release the spinlock for protecting
	 *  simultaneous VM state transition requests
	 *  - &vm->vm_lock
	 */
	spinlock_release(&vm->vm_lock);
}

/**
 * @}
 */
