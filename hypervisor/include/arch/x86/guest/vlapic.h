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

#ifndef VLAPIC_H
#define VLAPIC_H

#include <page.h>
#include <timer.h>
#include <apicreg.h>

/**
 * @addtogroup vp-base_vlapic
 *
 * @{
 */

/**
 * @file
 * @brief This file contains the external data structures and APIs for the vp-base.vlapic module
 *
 */

/**
 * @brief Data structure to define the vlapic used in ACRN.
 *
 * This stucture contains \a apic_page field to cache the virtual LAPIC registers,
 * \a vm pointer and \a vcpu pointer link to the virtual machine and vCPU
 * where the virtual LAPIC belongs to. msr_apicbase caches the virtual
 * IA32_APIC_BASE MSR which associates with the virtual LAPIC.
 *
 * @consistency N/A
 * @alignment 4096
 *
 * @remark N/A
 */
struct acrn_vlapic {
	/**
	 * @brief This field is used to cache the virtual LAPIC registers.
	 */
	struct lapic_regs apic_page;
	/**
	 * @brief This field points to the virtual machine which holds the virual LAPIC.
	 */
	struct acrn_vm *vm;
	/**
	 * @brief This field points to the vCPU which holds the virual LAPIC.
	 */
	struct acrn_vcpu *vcpu;
	/**
	 * @brief This field caches the virtual IA32_APIC_BASE MSR which associates
	 *        with the virtual LAPIC.
	 */
	uint64_t msr_apicbase;
} __aligned(PAGE_SIZE);

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
uint64_t vlapic_get_tsc_deadline_msr(const struct acrn_vlapic *vlapic);

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
void vlapic_set_tsc_deadline_msr(struct acrn_vlapic *vlapic, uint64_t val_arg);

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
uint64_t vlapic_get_apicbase(const struct acrn_vlapic *vlapic);

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
int32_t vlapic_x2apic_read(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val);

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
int32_t vlapic_x2apic_write(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val);

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
uint32_t vlapic_get_apicid(const struct acrn_vlapic *vlapic);

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
void vlapic_create(struct acrn_vcpu *vcpu);

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
void vlapic_reset(struct acrn_vlapic *vlapic);

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
void vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast, uint32_t dest, bool phys, bool lowprio);

/**
 * @}
 */

#endif /* VLAPIC_H */
