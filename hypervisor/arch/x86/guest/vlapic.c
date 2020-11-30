/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
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

#define pr_prefix "vlapic: "

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <atomic.h>
#include <per_cpu.h>
#include <pgtable.h>
#include <lapic.h>
#include <vmcs.h>
#include <vlapic.h>
#include <ptdev.h>
#include <vmx.h>
#include <vm.h>
#include <ept.h>
#include <trace.h>
#include <logmsg.h>
#include "vlapic_priv.h"

/**
 * @defgroup vp-base vp-base
 * @brief The component that creates / destroys VMs and handles VM-exits
 *
 * The vp-base component is responsible for initializing resources of VMs, launching the VMs and handle VM exit events
 * triggered by the vCPUs.
 *
 * This component is above the boot, lib and hwmgmt component and at the same level as the vp-dm component. The
 * functionality of this component relies on the boot component for a proper C execution environment, the lib component
 * for mutual exclusion, bitmap and string operations and the hwmgmt component for updating virtual machine control
 * structures, operating DMA remapping units and scheduling vCPUs. It relies on the vp-dm component to handle VM exit
 * events specific to virtual peripherals or pass-through devices.
 *
 * Refer to section 10.3 of Software Architecture Design Specification for the detailed decomposition of this component
 * and section 11.4 for the external APIs of each module inside this component.
 */

/**
 * @defgroup vp-base_vlapic vp-base.vlapic
 * @ingroup vp-base
 * @brief The defination and implementation of virtual Local APIC related stuff.

 * It provides external APIs for creation of virtual LAPIC, calculation of the
 * destination vlapic of a specified MSI interrupt and processing function when
 * getting/setting virtual LAPIC registers happens.
 *
 * Usage:
 * - 'vp-base.vcpuid' depends on this module to get the APIC ID associated with the specified vCPU.
 * - 'vp-dm.ptirq' depends on this module to calculate the destination vlapic when delivering a specified
 *   MSI interrupt
 * - 'vp-base.vmsr' depends on this module to get/set vlapic MSR registers.
 * - 'vp-base.vcpu' depends ont this module to do vlapic initialization or reset.
 *
 * Dependency:
 * - This module depends on hwmgmt.cpu module to read/write physical CPU registers.
 * - This module depends on vp-base.virq module to send request to vCPU.
 * - This module depends on vp-base_vcpu module to do vCPU operations.
 * - This module depends on lib.bits module to do atomic operations.
 * - This module depends on hwmgmt.apic module to use APIC registers definition
 *
 * @{
 */

/**
 * @file
 * @brief This file contains the data structures and functions for the vp-base.vlapic module.
 *
 * This file implements all following external APIs that shall be provided by the vp-base.vlapic
 * module.
 * - vlapic_get_tsc_deadline_msr
 * - vlapic_get_tsc_deadline_msr
 * - vlapic_get_apicbase
 * - vlapic_x2apic_read
 * - vlapic_x2apic_write
 * - vlapic_get_apicid
 * - vlapic_create
 * - vlapic_reset
 * - vlapic_calc_dest
 *
 * This file also implements following helper functions to help realizing the features of
 * the external
 * APIs.
 * - vm_lapic_from_vcpu_id
 * - vm_apicid2vcpu_id
 * - vlapic_build_id
 * - vlapic_build_x2apic_id
 * - set_dest_mask_phys
 * - is_dest_field_matched
 * - vlapic_process_init_sipi
 * - vlapic_read
 * - vlapic_init
 * - x2apic_msr_to_regoff
 * - vlapic_x2apic_pt_icr_access
 */

#define APICBASE_BSP     0x00000100UL /**< mask of BSP bit of IA32_APIC_BASE MSR */
#define APICBASE_X2APIC  0x00000400U  /**< mask of X2APIC bit of IA32_APIC_BASE MSR */
#define APICBASE_ENABLED 0x00000800UL /**< mask of APIC global enable/disable bit of IA32_APIC_BASE MSR */
#define LOGICAL_ID_MASK  0xFU         /**< mask of logical ID bits in APIC ID */
#define CLUSTER_ID_MASK  0xFFFF0U     /**< mask of Cluster ID bits in APIC ID */
#define APIC_ICR_MASK    0x000C0FFFUL /**< mask of APIC ICR bits which can be changed */

#define ACRN_DBG_LAPIC 6U	      /**< This macro is used for virtual local APIC debug */

/**
 * @brief This function is for getting the acrn_vlapic structure based on the vcpu_id.
 *
 * @param[in] vm pointer to the virtual machine structure which the virtual local APIC belongs to.
 * @param[in] vcpu_id The ID marks the vCPU.
 *
 * @return the acrn_vlapic structure associated with the vcpu_id.
 *
 * @pre vm != NULL
 * @pre vcpu_id < MAX_VCPUS_PER_VM
 * @pre vm->hw.vcpu_array[vcpu_id].state != VCPU_OFFLINE
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static struct acrn_vlapic *vm_lapic_from_vcpu_id(struct acrn_vm *vm, uint16_t vcpu_id)
{
	/** Declare the following local variable of type 'struct acrn_vcpu *'
         *  - vcpu representing the vCPU structure associated with the vcpu_id, not initialized */
	struct acrn_vcpu *vcpu;

	/** Call vcpu_from_vid with the following parameters, in order to get the vCPU
	 *  structure associated with the vcpu_id and assign it to vcpu
	 *  - vm
	 *  - vcpu_id
	 */
	vcpu = vcpu_from_vid(vm, vcpu_id);

	/** Call vcpu_vlapic with the following parameters and return its return value,
	 *  in order to return the acrn_vlapic structure associated with the vcpu_id.
	 *  - vcpu
	 */
	return vcpu_vlapic(vcpu);
}

/**
 * @brief This function is for converting the Local APIC ID to its vCPU ID .
 *
 * @param[in] vm pointer to the virtual machine structure which the virtual local APIC belongs to.
 * @param[in] lapicid The Local APIC ID.
 *
 * @return the converted vCPU ID.
 *
 * @retval INVALID_CPU_ID when there is no vCPU in \a vm that has Local APIC ID \a lapicid.
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static uint16_t vm_apicid2vcpu_id(struct acrn_vm *vm, uint32_t lapicid)
{
	/** Declare the following local variable of type uint16_t
         *  - i representing the ith vCPU, not initialized */
	uint16_t i;
	/** Declare the following local variable of type 'struct acrn_vcpu *'
         *  - vcpu representing the vCPU whose LAPIC ID is lapicid, not initialized */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variable of type uint16_t
         *  - cpu_id representing converted vCPU ID, initialized as INVALID_CPU_ID */
	uint16_t cpu_id = INVALID_CPU_ID;

	/** loop for each non-offline vCPU, from 0 to (vm->hw.created_vcpus - 1) */
	foreach_vcpu (i, vm, vcpu) {
		/** Declare the following local variable of type 'struct acrn_vlapic *'
	         *  - vlapic representing the Local APIC structure of vcpu, initialized as vcpu_vlapic(vcpu) */
		const struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
		/** Get virtual Local APIC ID from the Local APIC structure and if it equals to the lapicid */
		if (vlapic_get_apicid(vlapic) == lapicid) {
			/** Set cpu_id to be vcpu->vcpu_id */
			cpu_id = vcpu->vcpu_id;
			/** Go out the loop */
			break;
		}
	}

	/** If the cpu_id equals to INVALID_CPU_ID */
	if (cpu_id == INVALID_CPU_ID) {
		/** Print a error message */
		pr_err("%s: bad lapicid %lu", __func__, lapicid);
	}

	/** Return the converted vCPU ID */
	return cpu_id;
}

/**
 * @brief This function is for getting virtual Local APIC ID from a given Local APIC structure.
 *
 * @param[in] vlapic pointer to the virtual Local APIC structure for Local APIC ID extraction.
 *
 * @return the virtual Local APIC ID.
 *
 * @pre vlapic != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
uint32_t vlapic_get_apicid(const struct acrn_vlapic *vlapic)
{
	/** Declare the following local variable of type uint32_t
         *  - apicid representing virtual Local APIC ID, not initialized */
	uint32_t apicid;
	/** Assign vlapic->apic_page.id.v to apicid */
	apicid = vlapic->apic_page.id.v;

	/** Return apicid */
	return apicid;
}

/**
 * @brief This function is for building the virtual Local APIC ID.
 *
 * The method is to use the vCPU ID as the virtual Local APIC ID.
 *
 * @param[in] vlapic pointer to the virtual Local APIC structure
 *            whose Local APIC ID is to be generated.
 *
 * @return the generated virtual Local APIC ID.
 *
 * @pre vlapic != NULL
 * @pre vlapic->vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
static inline uint32_t vlapic_build_id(const struct acrn_vlapic *vlapic)
{
	/** Declare the following local variable of type 'const struct acrn_vcpu *'
         *  - vcpu representing the vCPU the vlapic belongs to, initialized as vlapic->vcpu */
	const struct acrn_vcpu *vcpu = vlapic->vcpu;
	/** Declare the following local variable of type uint32_t
         *  - lapic_regs_id representing the generated the virtual Local APIC ID, not initialized */
	uint32_t lapic_regs_id;

	/** Set the lapic_regs_id to be (uint32_t)vcpu->vcpu_id */
	lapic_regs_id = (uint32_t)vcpu->vcpu_id;

	/** Print out lapic_regs_id for debug purpose */
	dev_dbg(ACRN_DBG_LAPIC, "vlapic APIC PAGE ID : 0x%08x", lapic_regs_id);

	/** Return lapic_regs_id */
	return lapic_regs_id;
}

/**
 * @brief This function is for initializing the virtual Local APIC ID Register
 *        and Logical Destination Register in x2APIC mode.
 *
 * @param[in] vlapic pointer to the virtual Local APIC structure for configuration.
 *
 * @return None.
 *
 * @pre vlapic != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
static inline void vlapic_build_x2apic_id(struct acrn_vlapic *vlapic)
{
	/** Declare the following local variable of type 'struct lapic_regs *'
         *  - lapic representing the registers structure of \a vlapic, not initialized */
	struct lapic_regs *lapic;
	/** Declare the following local variable of type uint32_t
         *  - logical_id representing the Logical ID of \a vlapic, not initialized.
         *  - cluster_id representing the Cluster ID of \a vlapic, not initialized */
	uint32_t logical_id, cluster_id;

	/** Set lapic to be &(vlapic->apic_page) */
	lapic = &(vlapic->apic_page);
	/** Set lapic->id.v to be vlapic_build_id(vlapic) */
	lapic->id.v = vlapic_build_id(vlapic);
	/** Set logical_id to be lapic->id.v & LOGICAL_ID_MASK */
	logical_id = lapic->id.v & LOGICAL_ID_MASK;
	/** Set cluster_id to be (lapic->id.v & CLUSTER_ID_MASK) >> 4U */
	cluster_id = (lapic->id.v & CLUSTER_ID_MASK) >> 4U;
	/** Set lapic->ldr.v to be (cluster_id << 16U) | (1U << logical_id) */
	lapic->ldr.v = (cluster_id << 16U) | (1U << logical_id);
}

/**
 * @brief public API for getting the value of virtual IA32_TSC_DEADLINE MSR of \a vlapic.
 *
 * @param[inout] vlapic pointer to the vlapic whose MSR IA32_TSC_DEADLINE is to be read.
 *
 * @return the value of MSR IA32_TSC_DEADLINE of \a vlapic.
 *
 * @pre vlapic->vcpu != NULL
 * @pre vlapic != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
uint64_t vlapic_get_tsc_deadline_msr(const struct acrn_vlapic *vlapic)
{
	/** Declare the following local variable of type uint64_t
         *  - ret representing the value of IA32_TSC_DEADLINE MSR of \a vlapic, not initialized */
	uint64_t ret;
	/** If physical TSC_DEADLINE is zero which means it's not armed (automatically disarmed
	 *  after timer triggered) */
	if (msr_read(MSR_IA32_TSC_DEADLINE) == 0UL) {
		/** Call vcpu_set_guest_msr with the following parameters, in order to
	         *  set the virtual IA32_TSC_DEADLINE of a given vcpu to be 0UL
	         *  - vlapic->vcpu
	         *  - 0UL
	         */
		vcpu_set_guest_msr(vlapic->vcpu, MSR_IA32_TSC_DEADLINE, 0UL);
		/** Set the ret to be 0UL */
		ret = 0UL;
	/**  If the value of physical IA32_TSC_DEADLINE MSR is not zero */
	} else {
		/** Call vcpu_get_guest_msr with the following parameters, in order to
		 *  set the ret to be the return value of vcpu_get_guest_msr
		 *  - vlapic->vcpu
		 *  - MSR_IA32_TSC_DEADLINE
		 */
		ret = vcpu_get_guest_msr(vlapic->vcpu, MSR_IA32_TSC_DEADLINE);
	}

	/** Return ret as the virtual TSC_DEADLINE value */
	return ret;
}

/**
 * @brief public API for setting the value of virtual IA32_TSC_DEADLINE MSR of \a vlapic.
 *
 * @param[out] vlapic pointer to the vlapic whose MSR IA32_TSC_DEADLINE is to be written.
 * @param[in] val_arg the value to be set to the vlapic MSR IA32_TSC_DEADLINE.
 *
 * @return None.
 *
 * @pre vlapic->vcpu != NULL
 * @pre vlapic != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
void vlapic_set_tsc_deadline_msr(struct acrn_vlapic *vlapic, uint64_t val_arg)
{
	/** Declare the following local variable of type uint64_t
         *  - val representing the value to be set to the vlapic MSR IA32_TSC_DEADLINE,
         *        initialized as val_arg */
	uint64_t val = val_arg;

	/** Call vcpu_set_guest_msr with the following parameters, in order to
	 *  set virtual IA32_TSC_DEADLINE value to be val
	 *  - vlapic->vcpu
	 *  - MSR_IA32_TSC_DEADLINE
	 *  - val
	 */
	vcpu_set_guest_msr(vlapic->vcpu, MSR_IA32_TSC_DEADLINE, val);
	/** If val is not zero, which mean guest intends to arm the tsc_deadline timer */
	if (val != 0UL) {
		/** Set val to be (val - exec_vmread64(VMX_TSC_OFFSET_FULL)) which is for
	         *  its corresponding physical IA32_TSC_DEADLINE MSR configuration */
		val -= exec_vmread64(VMX_TSC_OFFSET_FULL);
		/** if the calculated value to write to the physical TSC_DEADLINE msr is zero */
		if (val == 0UL) {
			/** Plus 1 to not disarm the physcial timer falsely */
			val += 1UL;
		}
		/** Call msr_write with the following parameters, in order to
		 *  configure the physical IA32_TSC_DEADLINE MSR to be val
		 *  - MSR_IA32_TSC_DEADLINE
		 *  - val
		 */
		msr_write(MSR_IA32_TSC_DEADLINE, val);
	/** If val is zero, which means guest intends to disarm the tsc_deadline timer */
	} else {
		/** Call msr_write with the following parameters, in order to
		 *  configure the physical IA32_TSC_DEADLINE MSR to be 0UL to
		 *  disarm the physical timer
		 *  - MSR_IA32_TSC_DEADLINE
		 *  - 0UL
		 */
		msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
	}
}

/**
 * @brief This function sets \a dmask to mask the target vcpu in Physical Destination Mode.
 *
 * @param[in] vm pointer to the virtual machine structure in which the destination vCPU belongs to.
 * @param[out] dmask The mask of the target vcpu.
 * @param[in] dest The Local APIC ID for locating the destination vcpu.
 *
 * @return None.
 *
 * @pre vm != NULL
 * @pre dmask != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static inline void set_dest_mask_phys(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest)
{
	/** Declare the following local variable of type uint16_t
         *  - vcpu_id representing the target vcpu to be masked in \a dmask, not initialized */
	uint16_t vcpu_id;

	/** Call vm_apicid2vcpu_id with the following parameters, in order to
	 *  convert dest to the corresponding vCPU ID and assign it to vcpu_id
	 *  - vm
	 *  - dest
	 */
	vcpu_id = vm_apicid2vcpu_id(vm, dest);
	/** If vcpu_id is less than vm->hw.created_vcpus */
	if (vcpu_id < vm->hw.created_vcpus) {
		/** Call bitmap_set_nolock with the following parameters, in order to
		 *  mask the target vCPU in dmask
		 *  - vcpu_id
		 *  - dmask
		 */
		bitmap_set_nolock(vcpu_id, dmask);
	}
}

 /**
  * @brief This function tells if a vlapic belongs to the destination.
  *
  * @param[in] vlapic is pointer to the virtual machine structure which is going to be matched.
  * @param[in] dest The desniation of \a vlapic to be matched. It equals to local
  *            APIC ID in Physical Destination mode or APICs mask bits in Logical Destination
  *            Mode.
  *
  * @return If yes, return true, otherwise reture false.
  *
  * @pre vlapic != NULL
  *
  * @post N/A
  *
  * @mode HV_OPERATIONAL
  *
  * @remark N/A
  *
  * @reentrancy unspecified
  * @threadsafety when \a vlapic is different among parallel invocation
  */
static inline bool is_dest_field_matched(const struct acrn_vlapic *vlapic, uint32_t dest)
{
	/** Declare the following local variable of type uint32_t
         *  - logical_id representing the Logical ID of \a vlapic, not initialized
         *  - cluster_id representing the Cluster ID of \a vlapic, not initialized
         *  - dest_logical_id representing the Logical ID of \a dest, not initialized
         *  - dest_cluster_id representing the Cluster ID of \a dest, not initialized*/
	uint32_t logical_id, cluster_id, dest_logical_id, dest_cluster_id;
	/** Declare the following local variable of type uint32_t
	 *  - ldr representing the temp value of virtual Local destination register,
	 *    initialized as vlapic->apic_page.ldr.v */
	uint32_t ldr = vlapic->apic_page.ldr.v;
	/** Declare the following local variable of type bool
	 *  - ret representing the function return value, initialized as false */
	bool ret = false;

	/** Set the logical_id to be ldr & 0xFFFFU */
	logical_id = ldr & 0xFFFFU;
	/** Set the cluster_id to be (ldr >> 16U) & 0xFFFFU */
	cluster_id = (ldr >> 16U) & 0xFFFFU;
	/** Set dest_logical_id to be dest & 0xFFFFU */
	dest_logical_id = dest & 0xFFFFU;
	/** Set dest_cluster_id to be (dest >> 16U) & 0xFFFFU */
	dest_cluster_id = (dest >> 16U) & 0xFFFFU;
	/** If cluster_id equals to dest_cluster_id and (logical_id & dest_logical_id) != 0U */
	if ((cluster_id == dest_cluster_id) && ((logical_id & dest_logical_id) != 0U)) {
		/** Set the ret to be true */
		ret = true;
	}

	/** Return ret */
	return ret;
}

/**
 * @brief The public API that populates \a dmask with the set of vcpus that match the addressing specified
 *        by the (dest, phys, lowprio) tuple.
 *
 * @param[in] vm pointer to the virtual machine structure which the virtual local APIC belongs to.
 * @param[out] dmask The mask of the target vcpu, which is calculated from (dest, phys, lowprio) tuple.
 * @param[in] is_broadcast whether broadcast the interrupt to all vcpu. Yes for true, otherwise false.
 * @param[in] dest The value for locating the destination of vcpu.
 * @param[in] phys whether it is physical destination mode. Yes for true, otherwise false.
 * @param[in] lowprio whether it is lowest priority delivery mode. Yes for true, otherwise false.
 *
 * @return None.
 *
 * @pre vm != NULL
 * @pre dmask != NULL
 * @pre lowprio == false
 * @pre is_broadcast == false
 * @pre phys == true
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
void vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast, uint32_t dest, bool phys, bool lowprio)
{
	/** Declare the following local variable of type 'struct acrn_vlapic *'
	 *  - vlapic representing the virtual LAPIC of the destination, not initialized
	 *  - lowprio_dest representing the virtual LAPIC of the destination in
	 *    Lowest Priority Delivery mode initialized as NULL */
	struct acrn_vlapic *vlapic, *lowprio_dest = NULL;
	/** Declare the following local variable of type 'struct acrn_vcpu *'
	 *  - vcpu representing vcpu structure for destination calculation, not initialized */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variable of type uint16_t
	 *  - vcpu_id representing the virtual CPU ID, not initialized */
	uint16_t vcpu_id;

	/** Set the *dmask to be 0UL */
	*dmask = 0UL;
	/** If this is broadcast mode */
	if (is_broadcast) {
		/** Call vm_active_cpus with the following parameters, in order to
		 *  set *dmask to be its return value, meaning Broadcast in both
		 *  logical and physical modes.
		 *  - vm
		 */
		*dmask = vm_active_cpus(vm);
	/** If this is physical Destination mode */
	} else if (phys) {
		/** Call set_dest_mask_phys with the following parameters, in order to
		 *  set dmask in physical mode ("dest" is local APIC ID).
		 *  - vm
		 *  - dmask
		 *  - dest
		 */
		set_dest_mask_phys(vm, dmask, dest);
	/** If it is in Logical Destination mode */
	} else {
		/** Go through all vCPU in the VM to find the destination vCPU.
		 *  In Logical mode: "dest" is message destination addr
		 *  to be compared with the logical APIC ID in LDR. */
		foreach_vcpu (vcpu_id, vm, vcpu) {
			/** Call vm_lapic_from_vcpu_id with the following parameters, in order to
			 *  get the vlapic structure and assign it to vlapic
			 *  - vm
			 *  - vcpu_id
			 */
			vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
			/** If this is not the destination virtual LAPIC */
			if (!is_dest_field_matched(vlapic, dest)) {
				/** Continue to next iteration */
				continue;
			}

			/** If this is the Lowest Priority Delivery Mode */
			if (lowprio) {
				/** If lowprio_dest equals to NULL */
				if (lowprio_dest == NULL) {
					/** Set lowprio_dest to be vlapic */
					lowprio_dest = vlapic;
				/** If the lowprio_dest->apic_page.ppr.v is bigger than
				 *  vlapic->apic_page.ppr.v */
				} else if (lowprio_dest->apic_page.ppr.v > vlapic->apic_page.ppr.v) {
					/** Set lowprio_dest to be vlapic, for lowprio delivery mode,
					 *  the lowest-priority one among all "dest" matched processors
					 *  accepts the intr. */
					lowprio_dest = vlapic;
				/** If it is not in above cases */
				} else {
					/** No other state currently, do nothing */
				}
			/** If this is not Lowest Priority Delivery Mode */
			} else {
				/** Call bitmap_set_nolock with the following parameters, in order to
		                 *  mask the target vCPU in dmask
		                 *  - vcpu_id
		                 *  - dmask
		                 */
				bitmap_set_nolock(vcpu_id, dmask);
			}
		}

		/** If this is the Lowest Priority Delivery Mode and lowprio_dest does not
		 *  equals to NULL */
		if (lowprio && (lowprio_dest != NULL)) {
			/** Call bitmap_set_nolock with the following parameters, in order to
		         *  mask the target vCPU in dmask
		         *  - lowprio_dest->vcpu->vcpu_id
		         *  - dmask
		         */
			bitmap_set_nolock(lowprio_dest->vcpu->vcpu_id, dmask);
		}
	}
}

/**
 * @brief This function is for handling writing to guest Local APIC ICR in \a target_vcpu with a
 *        delivery mode of INIT or STARTUP
 *
 * @param[inout] target_vcpu pointer to the virtual CPU that attempts to write guest Local APIC ICR.
 * @param[in] mode the mode INIT IPI or STARTUP IPI.
 * @param[in] icr_low the lower 32 bits of the ICR.
 *
 * @return None
 *
 * @pre target_vcpu != NULL
 * @pre mode == APIC_DELMODE_INIT || mode == APIC_DELMODE_SIPI
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a target_vcpu is different among parallel invocation
 */
static void vlapic_process_init_sipi(struct acrn_vcpu *target_vcpu, uint32_t mode, uint32_t icr_low)
{
	/** If the Delivery Mode is INIT */
	if (mode == APIC_DELMODE_INIT) {
		/** Print a debug message to show which vCPU the INIT IPI is sent to */
		dev_dbg(ACRN_DBG_LAPIC, "Sending INIT to %hu", target_vcpu->vcpu_id);

		/** Call spinlock_obtain with the following parameter, in order to acquire the spinlock for protecting
		 *  simultaneous VM state transition requests
		 *  - &target_vcpu->vm->vm_lock
		 */
		spinlock_obtain(&target_vcpu->vm->vm_lock);

		/** Call pause_vcpu with the following parameters, in order to
		 *  put target vcpu to VCPU_ZOMBIE state so that it can be safely reset
		 *  - target_vcpu
		 */
		pause_vcpu(target_vcpu);

		/** Call reset_vcpu with the following parameters, in order to
		 *  reset the vCPU
		 *  - target_vcpu
		 */
		reset_vcpu(target_vcpu);

		/** Set target_vcpu->arch.nr_sipi to be 1U.
		 *  new cpu model only need one SIPI to kick AP run,
		 *  the second SIPI will be ignored as it move out of
		 *  wait-for-SIPI state.
		 */
		target_vcpu->arch.nr_sipi = 1U;

		/** Call spinlock_release with the following parameter, in order to release the spinlock for protecting
		 *  simultaneous VM state transition requests
		 *  - &target_vcpu->vm->vm_lock
		 */
		spinlock_release(&target_vcpu->vm->vm_lock);
	/** If the Delivery Mode is STARTUP */
	} else if (mode == APIC_DELMODE_STARTUP) {
		/** Call spinlock_obtain with the following parameter, in order to acquire the spinlock for protecting
		 *  simultaneous VM state transition requests
		 *  - &target_vcpu->vm->vm_lock
		 */
		spinlock_obtain(&target_vcpu->vm->vm_lock);

		/** If target_vcpu->state equals to VCPU_INIT and target_vcpu->arch.nr_sipi
		 *  does not equal to 0U, i.e.. ignore SIPIs in any state other than wait-for-SIPI */
		if ((target_vcpu->state == VCPU_INIT) && (target_vcpu->arch.nr_sipi != 0U)) {
			/** Print a debug message to show the vCPU the SIPI is sent to and the vector it is sent with */
			dev_dbg(ACRN_DBG_LAPIC, "Sending SIPI to %hu with vector %u", target_vcpu->vcpu_id,
				(icr_low & APIC_VECTOR_MASK));

			/** Set target_vcpu->arch.nr_sipi to be (target_vcpu->arch.nr_sipi - 1) */
			target_vcpu->arch.nr_sipi--;

			/** Logging the following information with a log level of LOG_ERROR.
			 *  - target_vcpu->vcpu_id
			 *  - target_vcpu->vm->vm_id
			 */
			pr_err("Start Secondary VCPU%hu for VM[%d]...", target_vcpu->vcpu_id,
				target_vcpu->vm->vm_id);
			/** Call set_vcpu_startup_entry with the following parameters, in order to
			 *  set start up entry for \a target_vcpu.
			 *  - target_vcpu
			 *  - (icr_low & APIC_VECTOR_MASK) << 12U
			 */
			set_vcpu_startup_entry(target_vcpu, (icr_low & APIC_VECTOR_MASK) << 12U);
			/** Call vcpu_make_request with the following parameters, in order to
			 *  request ACRN_REQUEST_INIT_VMCS for \a target_vcpu.
			 *  - target_vcpu
			 *  - ACRN_REQUEST_INIT_VMCS
			 */
			vcpu_make_request(target_vcpu, ACRN_REQUEST_INIT_VMCS);
			/** Call launch_vcpu with the following parameters, in order to
			 *  launch \a target_vcpu.
			 *  - target_vcpu
			 */
			launch_vcpu(target_vcpu);
		}

		/** Call spinlock_release with the following parameter, in order to release the spinlock for protecting
		 *  simultaneous VM state transition requests
		 *  - &target_vcpu->vm->vm_lock
		 */
		spinlock_release(&target_vcpu->vm->vm_lock);
	/** If the Delivery Mode is not INIT or STARTUP */
	} else {
		/** No other state currently, do nothing */
	}
}

/**
 * @brief This function is for reading the virtual LAPIC registers from LAPIC register cache page.
 *
 * @param[in] vlapic pointer to the virtual LAPIC which is being read.
 * @param[in] offset_arg the register offset in the LAPIC register cache page.
 * @param[out] data the read result of the register.
 *
 * @return 0 if no error happens otherwise return -EACCES.
 *
 * @pre vlapic != NULL
 * @pre (offset_arg == 20H) || (offset_arg == D0H) || (offset_arg == 300H)
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
static int32_t vlapic_read(struct acrn_vlapic *vlapic, uint32_t offset_arg, uint64_t *data)
{
	/** Declare the following local variable of type int32_t
	 *  - ret representing the return value of this function, initialized as 0 */
	int32_t ret = 0;
	/** Declare the following local variable of type 'struct lapic_regs *'
	 *  - lapic representing LAPIC register cache page, initialized as &(vlapic->apic_page) */
	struct lapic_regs *lapic = &(vlapic->apic_page);
	/** Declare the following local variable of type int32_t
	 *  - offset representing the register offset in the LAPIC register cache page,
	 *    initialized as offset_arg */
	uint32_t offset = offset_arg;
	/** Set the *data to be 0UL */
	*data = 0UL;

	/** If the offset exceed the size of LAPIC register cache page */
	if (offset > sizeof(*lapic)) {
		/** Set ret to be -EACCES */
		ret = -EACCES;
	/** If the offset does not exceed the size of LAPIC register cache page */
	} else {
		/** Set offset to be (offset & ~0x3UL) */
		offset &= ~0x3UL;
		/** Depend on the value of offset */
		switch (offset) {
		/** If the offset == APIC_OFFSET_ID */
		case APIC_OFFSET_ID:
			/** Set the *data to be lapic->id.v */
			*data = lapic->id.v;
			/** End of case */
			break;
		/** If the offset == APIC_OFFSET_LDR */
		case APIC_OFFSET_LDR:
			/** Set the *data to be lapic->ldr.v */
			*data = lapic->ldr.v;
			/** End of case */
			break;
		/** If the offset == APIC_OFFSET_ICR_LOW */
		case APIC_OFFSET_ICR_LOW:
			/** Set the *data to be ((uint64_t)lapic->icr_lo.v) | (((uint64_t)lapic->icr_hi.v) << 32UL) */
			*data = ((uint64_t)lapic->icr_lo.v) | (((uint64_t)lapic->icr_hi.v) << 32UL);
			/** End of case */
			break;
		/** Otherwise */
		default:
			/** Set the ret to be -EACCES */
			ret = -EACCES;
			/** End of case */
			break;
		}
	}

	/** Print offset and *data for debug purpose */
	dev_dbg(ACRN_DBG_LAPIC, "vlapic read offset %x, data %lx", offset, *data);
	/** Return ret */
	return ret;
}

/**
 * @brief This function is for doing virtual Local APIC reset.
 *
 * @param[inout] vlapic pointer to the virtual Local APIC which is going to be reset.
 *
 * @return None.
 *
 * @pre vlapic != NULL
 * @pre vlapic->vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
void vlapic_reset(struct acrn_vlapic *vlapic)
{
	/** Declare the following local variable of type 'struct lapic_regs *'
	 *  - lapic representing LAPIC register cache page, not initialized */
	struct lapic_regs *lapic;

	/** Set the vlapic->msr_apicbase to be (DEFAULT_APIC_BASE | APICBASE_ENABLED | APICBASE_X2APIC) */
	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED | APICBASE_X2APIC;

	/** If this the boot vCPU */
	if (vlapic->vcpu->vcpu_id == BOOT_CPU_ID) {
		/** Set the BSP bit in IA32_APIC_BASE MSR */
		vlapic->msr_apicbase |= APICBASE_BSP;
	}

	/** Set the lapic to be &(vlapic->apic_page) */
	lapic = &(vlapic->apic_page);
	/** Call memset with the following parameters, in order to set LAPIC register
	 *  cache page to be all zeros
	 *  - (void *)lapic
	 *  - 0U
	 *  - sizeof(struct lapic_regs)
	 */
	(void)memset((void *)lapic, 0U, sizeof(struct lapic_regs));

	/** Call vlapic_build_x2apic_id with the following parameters, in order to
	 *  configure the virtual Local APIC ID Register and Logical Destination
	 *  Register in x2APIC for \a vlapic mode.
	 *  - vlapic
	 */
	vlapic_build_x2apic_id(vlapic);
}

/**
 * @brief This function is for virtual Local APIC initialization. This function actually call
 *        vlapic_reset to finish the initialization.
 *
 * @param[inout] vlapic pointer to the virual Local APIC which is going to be initialized.
 *
 * @return None.
 *
 * @pre vlapic != NULL
 * @pre vlapic->vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
static void vlapic_init(struct acrn_vlapic *vlapic)
{
	/** Call vlapic_reset with the following parameters, in order to finish the initialization
         *  on \a vlapic.
         *  - vlapic
         */
	vlapic_reset(vlapic);
}

/**
 * @brief public API for getting the value of MSR IA32_APIC_BASE associates with \a vlapic.
 *
 * @param[in] vlapic pointer to the vlapic whose associated MSR IA32_APIC_BASE is to be read.
 *
 * @return the value of MSR IA32_APIC_BASE associates with \a vlapic.
 *
 * @pre vlapic != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vlapic is different among parallel invocation
 */
uint64_t vlapic_get_apicbase(const struct acrn_vlapic *vlapic)
{
	/** Return vlapic->msr_apicbase */
	return vlapic->msr_apicbase;
}

/**
 * @brief This function is for converting the x2APIC MSR address to its corresponding
 *        offset in LAPIC register cache page.
 *
 * @param[in] msr the msr to be converted.
 *
 * @return The offset in LAPIC register cache page of \a msr.
 *
 * @pre "msr >= 800H && msr < 900H"
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static inline uint32_t x2apic_msr_to_regoff(uint32_t msr)
{
	/** LAPIC register cache page is 4K page, the offset shall not big than that.
	 *  So return (((msr - 0x800U) & 0x3FFU) << 4U)
	 */
	return (((msr - 0x800U) & 0x3FFU) << 4U);
}

/**
 * @brief This function is for handling writing to virtual LAPIC ICR.
 *
 * If x2apic is pass-thru to guests, we have to emulate the following
 *   1. INIT Delivery mode
 *   2. SIPI Delivery mode
 * For all other cases, send IPI on the wire.
 * No shorthand and Physical destination mode are only supported.
 *
 * @param[in] vm pointer to the virtual machine structure whose virtual LAPIC ICR is to be accessed.
 * @param[in] val the value written to the virtual LAPIC ICR.
 *
 * @return 0 if no error happens otherwise return -1.
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static int32_t vlapic_x2apic_pt_icr_access(struct acrn_vm *vm, uint64_t val)
{
	/** Declare the following local variable of type uint32_t
	 *  - papic_id representing the physical LAPIC ID, not initialized
	 *  - vapic_id representing the virtual LAPIC ID specified in val, initialized as (uint32_t)(val >> 32U) */
	uint32_t papic_id, vapic_id = (uint32_t)(val >> 32U);
	/** Declare the following local variable of type uint32_t
	 *  - uint32_t representing lower 32 bits of ICR register, initialized as (uint32_t)val */
	uint32_t icr_low = (uint32_t)val;
	/** Declare the following local variable of type uint32_t
	 * - mode representing the Delivery mode to be written, initialized as (icr_low & APIC_DELMODE_MASK) */
	uint32_t mode = icr_low & APIC_DELMODE_MASK;
	/** Declare the following local variable of type uint16_t
	 *  - vcpu_id representing the vCPU ID of the destination vCPU, not initialized */
	uint16_t vcpu_id;
	/** Declare the following local variable of type 'struct acrn_vcpu *'
	 *  - target_vcpu representing the target vCPU to receive the IPI, not initialized */
	struct acrn_vcpu *target_vcpu;
	/** Declare the following local variable of type bool
	 *  - phys representing the Destination Mode field of the ICR, not initialized */
	bool phys;
	/** Declare the following local variable of type uint32_t
	 *  - shorthand representing the Destination Shorthand field of ICR, not initialized */
	uint32_t shorthand;
	/** Declare the following local variable of type uint32_t
	 *  - reserved_bits representing ICR bits that can not be changed, not initialized */
	uint32_t reserved_bits;
	/** Declare the following local variable of type int32_t
	 *  - ret representing the return value of this function, not initialized */
	int32_t ret;

	/** Set the reserved_bits to be ~(APIC_VECTOR_MASK | APIC_DELMODE_MASK |
	 *  APIC_DESTMODE_LOG | APIC_LEVEL_MASK | APIC_TRIGMOD_MASK | APIC_DEST_MASK) */
	reserved_bits = ~(APIC_VECTOR_MASK | APIC_DELMODE_MASK | APIC_DESTMODE_LOG
			| APIC_LEVEL_MASK | APIC_TRIGMOD_MASK | APIC_DEST_MASK);
	/** If the icr_low has any reserved bits set */
	if ((icr_low & reserved_bits) != 0U) {
		/** Print an error message */
		pr_err("Setting reserved bits in ICR");
		/** Set the ret to be -1 */
		ret = -1;
	/** If the icr_low does not have any reserved bits set */
	} else {
		/** Check if icr_low has the Destination Mode bit set. if yes,
		 *  set phys to be false, otherwise set it to be true */
		phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
		/** Set shorthand to be (icr_low & APIC_DEST_MASK) */
		shorthand = icr_low & APIC_DEST_MASK;

		/** If it is not physical Destination Mode or (shorthand != APIC_DEST_DESTFLD) */
		if ((phys == false) || (shorthand != APIC_DEST_DESTFLD)) {
			/** Print an error message to say this is not supported */
			pr_err("Logical destination mode or shorthands \
				not supported in ICR for partition mode\n");
		/** If the setting of ICR is supported */
		} else {
			/** Call vm_apicid2vcpu_id with the following parameters and
			 *  set vcpu_id to be its return value
			 *  - vm
			 *  - vapic_id
			 */
			vcpu_id = vm_apicid2vcpu_id(vm, vapic_id);
			/** If vcpu_id is less than vm->hw.created_vcpus and
			 *  vm->hw.vcpu_array[vcpu_id].state does not equals to VCPU_OFFLINE */
			if ((vcpu_id < vm->hw.created_vcpus) && (vm->hw.vcpu_array[vcpu_id].state != VCPU_OFFLINE)) {
				/** Call vcpu_from_vid with the following parameters, in order to
				 *  get the target vCPU structure from vcpu_id.
				 *  - vm
				 *  - vcpu_id
				 */
				target_vcpu = vcpu_from_vid(vm, vcpu_id);

				/** Depend on the value of mode */
				switch (mode) {
				/** If Delivery Mode is INIT */
				case APIC_DELMODE_INIT:
					/** Call vlapic_process_init_sipi with the following parameters, in order to
				         *  process the INIT Delivery Mode case.
				         *  - target_vcpu
				         *  - mode
				         *  - icr_low
				         */
					vlapic_process_init_sipi(target_vcpu, mode, icr_low);
					/** End of case */
					break;
				/** If Delivery Mode is Start Up */
				case APIC_DELMODE_STARTUP:
					/** Call vlapic_process_init_sipi with the following parameters, in order to
					 *  process the STARTUP Delivery Mode case.
					 *  - target_vcpu
					 *  - mode
					 *  - icr_low
					 */
					vlapic_process_init_sipi(target_vcpu, mode, icr_low);
					/** End of case */
					break;
				/** Otherwise */
				default:
					/** convert the dest from virtual apic_id to physical apic_id
				         *  by setting papic_id to be per_cpu(lapic_id, pcpuid_from_vcpu(target_vcpu)) */
					papic_id = per_cpu(lapic_id, pcpuid_from_vcpu(target_vcpu));
					/** Print out the value of vapic_id, papic_id and icr_low for debug purpose */
					dev_dbg(ACRN_DBG_LAPICPT,
						"%s vapic_id: 0x%08lx papic_id: 0x%08lx icr_low:0x%08lx",
						__func__, vapic_id, papic_id, icr_low);
					/** Call msr_write with the following parameters, in order to
					 *  set MSR IA32_EXT_APIC_ICR with a given value.
					 *  - MSR_IA32_EXT_APIC_ICR
					 *  - (((uint64_t)papic_id) << 32U) | icr_low
					 */
					msr_write(MSR_IA32_EXT_APIC_ICR, (((uint64_t)papic_id) << 32U) | icr_low);
					/** End of case */
					break;
				}
			}
		}

		/** Set the ret to be 0 */
		ret = 0;
	}

	/** Return ret */
	return ret;
}

/**
 * @brief public API for reading the x2APIC registers.
 *
 * @param[in] vcpu pointer to the virtual CPU which the x2APIC belongs to.
 * @param[in] msr the msr to be read.
 * @param[out] val the read result of msr.
 *
 * @return 0 if no error happens otherwise return -EACCES.
 *
 * @pre vcpu != NULL
 * @pre val != NULL
 * @pre (msr == 802H) || (msr == 80DH) || (msr == 830H)
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
int32_t vlapic_x2apic_read(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val)
{
	/** Declare the following local variable of type 'struct acrn_vlapic *'
	 *  - vlapic representing the virtual LAPIC whose register to be read, not initialized */
	struct acrn_vlapic *vlapic;
	/** Declare the following local variable of type uint32_t
	 *  - offset representing the register offset in the LAPIC register cache page of
	 *    corresponding msr, not initialized */
	uint32_t offset;
	/** Declare the following local variable of type int32_t
	 *  - error representing the error code of this function, initialized as -1 */
	int32_t error = -1;

	/** call vcpu_vlapic with the following parameters, in order to
	 *  get the virtual LAPIC structure pointer from vCPU structure
	 *  and assign it to vlapic.
	 *  - vcpu
	 */
	vlapic = vcpu_vlapic(vcpu);
	/** Depend on the value of msr */
	switch (msr) {
	/** If msr == MSR_IA32_EXT_APIC_LDR */
	case MSR_IA32_EXT_APIC_LDR:
	/** If msr == MSR_IA32_EXT_XAPICID */
	case MSR_IA32_EXT_XAPICID:
	/** If msr == MSR_IA32_EXT_APIC_ICR */
	case MSR_IA32_EXT_APIC_ICR:
		/** Call x2apic_msr_to_regoff with the following parameters, in order to
		 *  convert the x2APIC MSR address to its corresponding
		 *  offset in LAPIC register cache page and assign it to offset
		 *  - msr
		 */
		offset = x2apic_msr_to_regoff(msr);
		/** Call vlapic_read with the following parameters, in order to
		 *  read the register at the offset into \a val and
	         *  assign the return value of vlapic_read to error
	         *  - vlapic
	         *  - offset
	         *  - val
	         */
		error = vlapic_read(vlapic, offset, val);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Print the value of msr to indicate the error */
		pr_err("%s: unexpected MSR[0x%x] read with lapic_pt", __func__, msr);
		/** End of case */
		break;
	}

	/** Return error */
	return error;
}

/**
 * @brief public API for writing the x2APIC registers.
 *
 * @param[inout] vcpu pointer to the virtual CPU which the x2APIC belongs to.
 * @param[in] msr the msr to be written.
 * @param[in] val the value to be written to the msr.
 *
 * @return 0 if no error happens otherwise return -1.
 *
 * @pre vcpu != NULL
 * @pre val != NULL
 * @pre msr == 830H
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
int32_t vlapic_x2apic_write(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val)
{
	/** Declare the following local variable of type 'struct acrn_vlapic *'
	 *  - vlapic representing the virtual LAPIC whose register to be written, not initialized */
	struct acrn_vlapic *vlapic;
	/** Declare the following local variable of type 'struct lapic_regs *'
	 *  - lapic representing LAPIC register cache page, not initialized */
	struct lapic_regs *lapic;
	/** Declare the following local variable of type int32_t
	 *  - error representing the error code of this function, initialized as -1 */
	int32_t error = -1;

	/** call vcpu_vlapic with the following parameters, in order to
	 *  get the virtual LAPIC structure pointer from vCPU structure
	 *  and assign it to vlapic
	 *  - vcpu
	 */
	vlapic = vcpu_vlapic(vcpu);
	/** Set lapic to be &(vlapic->apic_page) */
	lapic = &(vlapic->apic_page);
	/** Depend on the value of msr */
	switch (msr) {
	/** If msr == MSR_IA32_EXT_APIC_ICR */
	case MSR_IA32_EXT_APIC_ICR:
		/** Set lapic->icr_hi.v to be (uint32_t)(val >> 32UL) */
		lapic->icr_hi.v = (uint32_t)(val >> 32UL);
		/** Set lapic->icr_lo.v to be (uint32_t)(val & APIC_ICR_MASK) */
		lapic->icr_lo.v = (uint32_t)(val & APIC_ICR_MASK);
		/** Call vlapic_x2apic_pt_icr_access with the following parameters, in order to
		 *  handle writing to virtual LAPIC ICR and assign the return value of
		 *  vlapic_x2apic_pt_icr_access to error
		 *  - vcpu->vm
		 *  - val
		 */
		error = vlapic_x2apic_pt_icr_access(vcpu->vm, val);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Print the value of msr to indicate the error */
		pr_err("%s: unexpected MSR[0x%x] write with lapic_pt", __func__, msr);
		/** End of case */
		break;
	}

	/** Return the error */
	return error;
}

/**
 * @brief public API for creating virtual Local APIC of a \a vcpu.
 *
 * @param[in] vcpu pointer to the vcpu whose virual Local APIC is going to be created.
 *
 * @return None.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
void vlapic_create(struct acrn_vcpu *vcpu)
{
	/** Set vcpu->arch.vlapic.vm to be vcpu->vm */
	vcpu->arch.vlapic.vm = vcpu->vm;
	/** Set vcpu->arch.vlapic.vcpu to be \a vcpu */
	vcpu->arch.vlapic.vcpu = vcpu;

	/** Call vlapic_init with the following parameters, in order to
	 *  finish the vlapic creation for \a vcpu
	 *  - vcpu_vlapic(vcpu)
	 */
	vlapic_init(vcpu_vlapic(vcpu));
}

/**
 * @}
 */
