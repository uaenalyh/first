/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vmx.h>
#include <virq.h>
#include <mmu.h>
#include <vcpu.h>
#include <vm.h>
#include <vmexit.h>
#include <vm_reset.h>
#include <vmx_io.h>
#include <ept.h>
#include <vtd.h>
#include <vcpuid.h>
#include <trace.h>
#include <vmsr.h>

/**
 * @addtogroup vp-base_hv-main
 *
 * @{
 */

/**
 * @file
 * @brief This file define all these VM-exit related handlers except CR access, IO instruction
 * and MSR related handlers.
 *
 */

/*
 * According to "SDM APPENDIX C VMX BASIC EXIT REASONS",
 * there are 65 Basic Exit Reasons.
 */
#define NR_VMX_EXIT_REASONS 65U /**< number of all these VM-exit reasons */

static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t unexpected_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t init_signal_vmexit_handler(__unused struct acrn_vcpu *vcpu);
static int32_t taskswitch_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t invd_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t movdr_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @brief This structure array represents VM Dispatch table for Exit condition handling.
 *
 * It stores handlers and qualification information for
 * all these VM-exits.
 */
const struct vm_exit_dispatch dispatch_table[NR_VMX_EXIT_REASONS] = {
	/**
	 * @brief VM exit handler for Exception or non-maskable interrupt (NMI)
	 * is exception_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_EXCEPTION_OR_NMI] = {
		.handler = exception_vmexit_handler},
	/**
	 * @brief VM exit handler for External interrupt
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_EXTERNAL_INTERRUPT] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Triple fault
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_TRIPLE_FAULT] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for INIT signal
	 * is init_signal_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_INIT_SIGNAL] = {
		.handler = init_signal_vmexit_handler},
	/**
	 * @brief VM exit handler for Start-up IPI (SIPI)
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_STARTUP_IPI] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for I/O system-management interrupt (SMI)
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_IO_SMI] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Other SMI
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_OTHER_SMI] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Interrupt window
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_INTERRUPT_WINDOW] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for NMI window
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_NMI_WINDOW] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Task switch
	 * is taskswitch_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_TASK_SWITCH] = {
		.handler = taskswitch_vmexit_handler},
	/**
	 * @brief VM exit handler for CPUID instruction
	 * is cpuid_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_CPUID] = {
		.handler = cpuid_vmexit_handler},
	/**
	 * @brief VM exit handler for GETSEC instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_GETSEC] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for HLT instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.
	 */
	[VMX_EXIT_REASON_HLT] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for INVD instruction
	 * is invd_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_INVD] = {
		.handler = invd_vmexit_handler},
	/**
	 * @brief VM exit handler for INVLPG instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_INVLPG] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for RDPMC instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RDPMC] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for RDTSC instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RDTSC] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for RSM instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RSM] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for VMCALL instruction
	 * is exception_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMCALL] = {
		.handler = undefined_vmexit_handler
		},
	/**
	 * @brief VM exit handler for VMCLEAR instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMCLEAR] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMLAUNCH instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMLAUNCH] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMPTRLD instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMPTRLD] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMPTRST instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMPTRST] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMREAD instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMREAD] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMRESUME instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMRESUME] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMWRITE instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMWRITE] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMXOFF instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMXOFF] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VMXON instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMXON] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for CR accesses
	 * is cr_access_vmexit_handler and the exit qualification information is
	 * necessary for the handler.
     */
	[VMX_EXIT_REASON_CR_ACCESS] = {
		.handler = cr_access_vmexit_handler,
		.need_exit_qualification = 1U},
	/**
	 * @brief VM exit handler for DR accesses
	 * is movdr_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_DR_ACCESS] = {
		.handler = movdr_vmexit_handler},
	/**
	 * @brief VM exit handler for I/O instruction
	 * is pio_instr_vmexit_handler and the exit qualification information is
	 *  necessary for the handler. */
	[VMX_EXIT_REASON_IO_INSTRUCTION] = {
		.handler = pio_instr_vmexit_handler,
		.need_exit_qualification = 1U},
	/**
	 * @brief VM exit handler for RDMSR instruction
	 * is rdmsr_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RDMSR] = {
		.handler = rdmsr_vmexit_handler},
	/**
	 * @brief VM exit handler for WRMSR instruction
	 * is wrmsr_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_WRMSR] = {
		.handler = wrmsr_vmexit_handler},
	/**
	 * @brief VM exit handler for VM-entry failure due to invalid guest state
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for for VM-entry failure due to MSR loading
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for MWAIT instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_MWAIT] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for Monitor trap flag
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_MONITOR_TRAP] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for MONITOR instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_MONITOR] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for VM-entry failure due to machine-check event
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_PAUSE] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for VM-entry failure due to machine-check event
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for TPR below threshold
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_TPR_BELOW_THRESHOLD] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for APIC access
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_APIC_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Virtualized EOI
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.  */
	[VMX_EXIT_REASON_VIRTUALIZED_EOI] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Access to GDTR or IDTR
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_GDTR_IDTR_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Access to LDTR or TR
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_LDTR_TR_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for EPT violation
	 * is ept_violation_vmexit_handler and the exit qualification information is
	 * necessary for the handler.  */
	[VMX_EXIT_REASON_EPT_VIOLATION] = {
		.handler = ept_violation_vmexit_handler,
		.need_exit_qualification = 1U},
	/**
	 * @brief VM exit handler for EPT misconfiguration
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_EPT_MISCONFIGURATION] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for INVEPT instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.  */
	[VMX_EXIT_REASON_INVEPT] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for RDTSCP instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RDTSCP] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for VMX-preemption timer expired
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.  */
	[VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for INVVPID instruction
	 * is undefined_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.  */
	[VMX_EXIT_REASON_INVVPID] = {
		.handler = undefined_vmexit_handler},
	/**
	 * @brief VM exit handler for for WBINVD instruction
	 * is wbinvd_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_WBINVD] = {
		.handler = wbinvd_vmexit_handler},
	/**
	 * @brief VM exit handler for XSETBV instruction
	 * is xsetbv_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_XSETBV] = {
		.handler = xsetbv_vmexit_handler},
	/**
	 * @brief VM exit handler for APIC write
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_APIC_WRITE] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for RDRAND instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler.  */
	[VMX_EXIT_REASON_RDRAND] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for INVPCID instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_INVPCID] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for VMFUNC instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_VMFUNC] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for ENCLS instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_ENCLS] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for RDSEED instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_RDSEED] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for Page-modification log ful
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_PAGE_MODIFICATION_LOG_FULL] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for XSAVES instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_XSAVES] = {
		.handler = unexpected_vmexit_handler},
	/**
	 * @brief VM exit handler for XRSTORS instruction
	 * is unexpected_vmexit_handler and the exit qualification information is
	 * not necessary for the handler. */
	[VMX_EXIT_REASON_XRSTORS] = {
		.handler = unexpected_vmexit_handler}
};

/**
 * @brief This function is used to handle the VM-exits of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose vmexit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 *
 * @retval -EINVAL when ID of the current running physical CPU(by calling get_pcpu_id()) is
 * different with ID of the physical CPU associated with the given \a vcpu(by calling
 * pcpuid_from_vcpu() with \a vcpu as parameter).
 *
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu ！= NULL
 *
 * @post n/a
 *
 * @mode HV_OPERATIONAL
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation
 */
int32_t vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct vm_exit_dispatch *.
	 *  - dispatch representing pointer which points to structure vm_exit_dispatch
	 *  which stores the VM-exit handler and qualification information,
	 *  initialized as NULL.*/
	struct vm_exit_dispatch *dispatch = NULL;
	/** Declare the following local variables of type uint16_t.
	 *  - basic_exit_reason representing the vm basic exit reason */
	uint16_t basic_exit_reason;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the return value */
	int32_t ret;
	/** If ID of the current running physical CPU(by calling get_pcpu_id()) is
	 *  different with ID of the physical CPU associated with the given \a vcpu(by
	 *  calling pcpuid_from_vcpu() with \a vcpu as parameter).
	 */
	if (get_pcpu_id() != pcpuid_from_vcpu(vcpu)) {
		/** Logging the following information with a log level of LOG_FATAL.
		 *  - "vcpu is not running on its pcpu!" */
		pr_fatal("vcpu is not running on its pcpu!");
		/** Set ret to -EINVAL */
		ret = -EINVAL;
	} else {
		/** Set vcpu->arch.idt_vectoring_info to exec_vmread32(VMX_IDT_VEC_INFO_FIELD)*/
		vcpu->arch.idt_vectoring_info = exec_vmread32(VMX_IDT_VEC_INFO_FIELD);
		/** If bit 31 of vcpu->arch.idt_vectoring_info is 1 */
		if ((vcpu->arch.idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
			/** Declare the following local variables of type uint32_t.
			 *  - vector_info representing vector information, initialized as
			 *  vcpu->arch.idt_vectoring_info. */
			uint32_t vector_info = vcpu->arch.idt_vectoring_info;
			/** Declare the following local variables of type uint32_t.
			 *  - vector representing vector number, initialized as bit 0-7 of the
			 *  VM-Exit Interruption-Information Field. */
			uint32_t vector = vector_info & 0xffU;
			/** Declare the following local variables of type uint32_t.
			 *  - type representing vector type, initialized as bit 8-10 of the
			 *  VM-Exit Interruption-Information Field. */
			uint32_t type = (vector_info & VMX_INT_TYPE_MASK) >> 8U;
			/** Declare the following local variables of type uint32_t.
			 *  - err_code representing error code, initialized as 0. */
			uint32_t err_code = 0U;

			/** If type is VMX_INT_TYPE_HW_EXP */
			if (type == VMX_INT_TYPE_HW_EXP) {
				/** If bit 11 of vector_info is 1 */
				if ((vector_info & VMX_INT_INFO_ERR_CODE_VALID) != 0U) {
					/** Set err_code to exec_vmread32(VMX_IDT_VEC_ERROR_CODE) */
					err_code = exec_vmread32(VMX_IDT_VEC_ERROR_CODE);
				}
				/** Call vcpu_queue_exception() with the following parameters, in order to
				 *  prepare to inject exception into vcpu.
				 *  - vcpu
				 *  - vector
				 *  - err_code */
				vcpu_queue_exception(vcpu, vector, err_code);
				/** Set vcpu->arch.idt_vectoring_info to 0 */
				vcpu->arch.idt_vectoring_info = 0U;
			/** If type is VMX_INT_TYPE_NMI */
			} else if (type == VMX_INT_TYPE_NMI) {
				/** Call vcpu_make_request() with the following parameters, in order to
				 *  submit a NMI request to \a vcpu.
				 *  - vcpu
				 *  - ACRN_REQUEST_NMI */
				vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
				/** Set vcpu->arch.idt_vectoring_info to 0 */
				vcpu->arch.idt_vectoring_info = 0U;
			} else {
				/** No action on EXT_INT or SW exception. */
			}
		}

		/* Calculate basic exit reason (low 16-bits) */
		/** Set basic_exit_reason to low 16 bits of vcpu->arch.exit_reason*/
		basic_exit_reason = (uint16_t)(vcpu->arch.exit_reason & 0xFFFFU);
		/** Logging the following information with a log level of LOG_DEBUG.
		 *  - vcpu->arch.exit_reason */
		pr_dbg("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
		/** If basic_exit_reason is larger than or equals to ARRAY_SIZE(dispatch_table),
		 *  indicating that it's invalid */
		if (basic_exit_reason >= ARRAY_SIZE(dispatch_table)) {
			/** Logging the following information with a log level of LOG_ERROR.
			  *  - vcpu->arch.exit_reason */
			pr_err("Invalid Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
			/** Set ret to -EINVAL */
			ret = -EINVAL;
		} else {
			/** Set dispatch to dispatch table entry of the relative exit reason,
			 *  which is the element with the index 'basic_exit_reason' in the
			 *  array 'dispatch_table'. */
			dispatch = (struct vm_exit_dispatch *)(dispatch_table + basic_exit_reason);
			/** If 'dispatch->need_exit_qualification' is not 0, indicating that the
			 *  exit qualification is necessary in order to handle the VM exit caused by
			 *  basic_exit_reason. */
			if (dispatch->need_exit_qualification != 0U) {
				/** Set vcpu->arch.exit_qualification to the return value of
				 *  exec_vmread(VMX_EXIT_QUALIFICATION). */
				vcpu->arch.exit_qualification = exec_vmread(VMX_EXIT_QUALIFICATION);
			}
			/** Set ret to the return value of dispatch->handler(vcpu) */
			ret = dispatch->handler(vcpu);
		}
	}

	/** Return ret */
	return ret;
}

/**
 * @brief This function is used to handle unexpected VM exits in fusa use case, caused by external
 * interrupt, triple fault, SIPI, SMI, Interrupt window, NMI window, Task switch, GETSEC, HLT,
 * INVLPG, RDPMC, RDTSC, RSM, WRMSR RDMSR MWAIT MONITOR， PAUSE， RDTSCP， RDRAND, INVPCID,
 * ENCLS, RDSEED, XSAVES, XRSTORS, VMX related instructions and all the other VM exits caused by
 * VM-entry failure due to invalid guest state, VM-entry failure due to MSR loading,
 * unexpected_vmexit_handler, Monitor trap flag, TPR below threshold, APIC access, Virtualized EOI,
 * Access to GDTR or IDTR, Access to LDTR or TR, EPT misconfiguration, VMX-preemption timer expired,
 * and APIC write.
 *
 * @param[inout] vcpu A  pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t unexpected_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Logging the following information with a log level of LOG_FATAL.
	 *  - exec_vmread(VMX_GUEST_RIP) */
	pr_fatal("Error: Unexpected VM exit condition from guest at 0x%016lx ", exec_vmread(VMX_GUEST_RIP));
	/** Logging the following information with a log level of LOG_FATAL.
	 *  - vcpu->arch.exit_reason */
	pr_fatal("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
	/** Logging the following information with a log level of LOG_FATAL.
	 *  - exec_vmread(VMX_EXIT_QUALIFICATION) */
	pr_err("Exit qualification: 0x%016lx ", exec_vmread(VMX_EXIT_QUALIFICATION));
	/** Call TRACE_2L() with the following parameters, in order to trace unexpected
	 *  VM-exit event for debug purpose.
	 *  - TRACE_VMEXIT_UNEXPECTED
	 *  - vcpu->arch.exit_reason
	 *  - 0UL */
	TRACE_2L(TRACE_VMEXIT_UNEXPECTED, vcpu->arch.exit_reason, 0UL);

	/** If the VM associated with the given \a vcpu is safety vm */
	if (is_safety_vm(vcpu->vm)) {
		/** Call panic with the following parameters, in order to print error information
		 *  and panic the corresponding pcpu.
		 *  - Error */
		panic("Error: Unexpected VM exit!");
	} else {
		/** Call fatal_error_shutdown_vm() with the following parameters, in order to
		 *  shutdown the corresponding vm.
		 *  - vcpu*/
		fatal_error_shutdown_vm(vcpu);
	}

	/** Return 0 */
	return 0;
}

/**
 * @brief This function is used to handle the VM exit caused by CPUID instructions.
 *
 * @param[inout] vcpu A  pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
int32_t cpuid_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint64_t.
	 *  - rax representing variable to store RAX as it is 64 bit, not initialized.
	 *  - rbx representing variable to store RBX as it is 64 bit, not initialized.
	 *  - rcx representing variable to store RCX as it is 64 bit, not initialized.
	 *  - rdx representing variable to store RDX as it is 64 bit, not initialized. */
	uint64_t rax, rbx, rcx, rdx;

	/** Set rax to vcpu_get_gpreg(vcpu, CPU_REG_RAX) */
	rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);
	/** Set rbx to vcpu_get_gpreg(vcpu, CPU_REG_RBX) */
	rbx = vcpu_get_gpreg(vcpu, CPU_REG_RBX);
	/** Set rcx to vcpu_get_gpreg(vcpu, CPU_REG_RCX) */
	rcx = vcpu_get_gpreg(vcpu, CPU_REG_RCX);
	/** Set rdx to vcpu_get_gpreg(vcpu, CPU_REG_RDX) */
	rdx = vcpu_get_gpreg(vcpu, CPU_REG_RDX);
	/** Call TRACE_2L() with the following parameters, in order to trace
	 *  VM-exit caused by CPUID Instruction for debug purpose.
	 *  - TRACE_VMEXIT_CPUID
	 *  - rax
	 *  - rcx */
	TRACE_2L(TRACE_VMEXIT_CPUID, rax, rcx);
	/** Call guest_cpuid() with the following parameters, in order to emulates the CPUID
	 *  instruction to store the EAX, EBX, ECX and EDX of the specified CPUID leaf or
	 *  sub-leaf of the vCPU to the integers pointed by rax, rbx, rcx and rdx, respectively.
	 *  - vcpu
	 *  - (uint32_t *)&rax
	 *  - (uint32_t *)&rbx
	 *  - (uint32_t *)&rcx
	 *  - (uint32_t *)&rdx */
	guest_cpuid(vcpu, (uint32_t *)&rax, (uint32_t *)&rbx, (uint32_t *)&rcx, (uint32_t *)&rdx);
	/** Call vcpu_set_gpreg() with the following parameters, in order to set the guest RAX to 'rax'.
	 *  - vcpu
	 *  - CPU_REG_RAX
	 *  - rax */
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, rax);
	/** Call vcpu_set_gpreg() with the following parameters, in order to set the guest RBX to 'rbx'.
	 *  - vcpu
	 *  - CPU_REG_RBX
	 *  - rax */
	vcpu_set_gpreg(vcpu, CPU_REG_RBX, rbx);
	/** Call vcpu_set_gpreg() with the following parameters, in order to set the guest RCX to 'rcx'.
	 *  - vcpu
	 *  - CPU_REG_RCX
	 *  - rax */
	vcpu_set_gpreg(vcpu, CPU_REG_RCX, rcx);
	/** Call vcpu_set_gpreg() with the following parameters, in order to set the guest RDX to 'rdx'.
	 *  - vcpu
	 *  - CPU_REG_RDX
	 *  - rax */
	vcpu_set_gpreg(vcpu, CPU_REG_RDX, rdx);

	/** Return 0 */
	return 0;
}

/**
 * @brief This function is handle vmexit caused by xsetbv instruction.
 * XSETBV instruction set's the XCR0 that is used to tell for which
 * components states can be saved on a context switch using xsave.
 * According to SDM vol3 25.1.1:
 * Invalid-opcode exception (UD) and faults based on privilege level (include
 * virtual-8086 mode privileged instructions are not recognized) have higher
 * priority than VM exit.
 * We don't need to handle those case here because we depends on VMX to handle
 * them.
 *
 * @param[inout] vcpu A  pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint64_t.
	 *  - val64 representing variable to store guest cr4 from VMCS */
	uint64_t val64;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing return value, initialized as 0. */
	int32_t ret = 0;

	/** Set val64 to GUEST_CR4 read from VMCS */
	val64 = exec_vmread(VMX_GUEST_CR4);
	/** If OSXSAVE bit of val64 is clear */
	if ((val64 & CR4_OSXSAVE) == 0UL) {
		/** Call vcpu_inject_gp() with the following parameters, in order to inject GP to the vcpu
		 *  with error code 0.
		 *  - vcpu
		 *  - 0 */
		vcpu_inject_gp(vcpu, 0U);
	} else {
			/** If ECX of the target vcpu is not 0 */
			if ((vcpu_get_gpreg(vcpu, CPU_REG_RCX) & 0xffffffffUL) != 0UL) {
				/** Call vcpu_inject_gp() with the following parameters, in order to inject GP to the
				 *  vcpu with error code 0.
				 *  - vcpu
				 *  - 0 */
				vcpu_inject_gp(vcpu, 0U);
			} else {
				/** Set val64 to (vcpu_get_gpreg(vcpu, CPU_REG_RAX) & 0xffffffffUL)|
				 *  (vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U) */
				val64 = (vcpu_get_gpreg(vcpu, CPU_REG_RAX) & 0xffffffffUL) |
					(vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U);

				/** If bit 0 of val64 is clear */
				if ((val64 & 0x01UL) == 0UL) {
					/** Call vcpu_inject_gp() with the following parameters, in order
					 *  to inject GP to the vcpu with error code 0.
					 *  - vcpu
					 *  - 0 */
					vcpu_inject_gp(vcpu, 0U);
				/** If any of the reserved bits(specified by XCR0_RESERVED_BITS) of val64 is not 0 */
				} else if ((val64 & XCR0_RESERVED_BITS) != 0UL) {
					/** Call vcpu_inject_gp() with the following parameters, in order
					 *  to inject GP to the vcpu with error code 0.
					 *  - vcpu
					 *  - 0 */
					vcpu_inject_gp(vcpu, 0U);
				} else {
					/*
					 * XCR0[2:1] (SSE state & AVX state) can't not be
					 * set to 10b as it is necessary to set both bits
					 * to use AVX instructions.
					 */
					/** If bits 2:1 of val64 is equal to XCR0_AVX */
					if ((val64 & (XCR0_SSE | XCR0_AVX)) == XCR0_AVX) {
						/** Call vcpu_inject_gp() with the following parameters, in order
						 * to inject GP to the vcpu with error code 0.
						 *  - vcpu
						 *  - 0 */
						vcpu_inject_gp(vcpu, 0U);
					} else {
						/*
						 * SDM Vol.1 13-4, XCR0[4:3] are associated with MPX state,
						 * Guest should not set these two bits without MPX support.
						 */
						/** If bits 4:3 of val64 is not 0 */
						if ((val64 & (XCR0_BNDREGS | XCR0_BNDCSR)) != 0UL) {
							/** Call vcpu_inject_gp() with the following parameters,
							 *  in order to inject GP to the vcpu with error code 0.
							 *  - vcpu
							 *  - 0 */
							vcpu_inject_gp(vcpu, 0U);
						} else {
							/** Call write_xcr() with the following parameters, in order to
							 *  write val64 into the XCR0.
							 *  - 0
							 *  - val64 */
							write_xcr(0, val64);
						}
					}
				}
			}
		}

	/** Return ret */
	return ret;
}

/**
 * @brief This function is used to handle vm exit caused by wbinvd instruction.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Call walk_ept_table() with the following parameters, in order to flush the cache derived from EPT.
	 *  - vcpu->vm
	 *  - ept_flush_leaf_page */
	walk_ept_table(vcpu->vm, ept_flush_leaf_page);

	/** Return 0 */
	return 0;
}

/**
 * @brief This function is used to handle undefined VM exits
 *
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_inject_ud() with the following parameters, in order to inject ud to the vcpu.
	 *  - vcpu */
	vcpu_inject_ud(vcpu);
	/** Return 0 */
	return 0;
}

/**
 * @brief This function is used to handle INIT signal caused VM exit.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t init_signal_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/*
	 * Intel SDM Volume 3, 25.2:
	 *   INIT signals. INIT signals cause VM exits. A logical processer performs none
	 *   of the operations normally associated with these events. Such exits do not modify
	 *   register state or clear pending events as they would outside of VMX operation (If
	 *   a logical processor is the wait-for-SIPI state, INIT signals are blocked. They do
	 *   not cause VM exits in this case).
	 *
	 * So, it is safe to ignore the signal and reture here.
	 */
	/** Call vcpu_retain_rip() with the following parameters, in order to retain the current guest RIP.
	 *  - vcpu */
	vcpu_retain_rip(vcpu);
	/** Return 0 */
	return 0;
}


/**
 * @brief This function is used to handle vm exit caused by task switch.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t taskswitch_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_inject_gp() with the following parameters, in order to inject gp to the vcpu with
	 *  error code from vmcs qualification
	 *  - vcpu
	 *  - vcpu->arch.exit_qualification & 0xFFUL */
	vcpu_inject_gp(vcpu, (uint32_t)(vcpu->arch.exit_qualification & 0xFFUL));
	/** Return 0 */
	return 0;
}

/**
 * @brief This function is used to handle vm exit caused by INVD instruction.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t invd_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_inject_gp() with the following parameters, in order to inject gp to the vcpu with error code 0.
	 *  - vcpu
	 *  - 0 */
	vcpu_inject_gp(vcpu, 0U);
	/** Return 0 */
	return 0;
}

/**
 * @brief This function is used to handle vm exit caused by DR accesses.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure whose VM exit needs to be handled.
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 *
 */
static int32_t movdr_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	/*
	 * In the current usecase, triple fault will happen if vcpu inject gp during
	 * mov_dr handler. Here just do nothing to work around and this issue will be
	 * resolved in the future.
	 */

	/** Return 0 */
	return 0;
}

/**
 * @}
 */
