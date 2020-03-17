/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vmx.h>
#include <irq.h>
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

/**
 * @addtogroup vp-base_hv_main
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/*
 * According to "SDM APPENDIX C VMX BASIC EXIT REASONS",
 * there are 65 Basic Exit Reasons.
 */
#define NR_VMX_EXIT_REASONS 65U

static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t unexpected_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t init_signal_vmexit_handler(__unused struct acrn_vcpu *vcpu);
static int32_t taskswitch_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t invd_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t movdr_vmexit_handler(struct acrn_vcpu *vcpu);

/* VM Dispatch table for Exit condition handling */
const struct vm_exit_dispatch dispatch_table[NR_VMX_EXIT_REASONS] = {
	[VMX_EXIT_REASON_EXCEPTION_OR_NMI] = {
		.handler = exception_vmexit_handler},
	[VMX_EXIT_REASON_EXTERNAL_INTERRUPT] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_TRIPLE_FAULT] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INIT_SIGNAL] = {
		.handler = init_signal_vmexit_handler},
	[VMX_EXIT_REASON_STARTUP_IPI] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_IO_SMI] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_OTHER_SMI] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INTERRUPT_WINDOW] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_NMI_WINDOW] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_TASK_SWITCH] = {
		.handler = taskswitch_vmexit_handler},
	[VMX_EXIT_REASON_CPUID] = {
		.handler = cpuid_vmexit_handler},
	[VMX_EXIT_REASON_GETSEC] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_HLT] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INVD] = {
		.handler = invd_vmexit_handler},
	[VMX_EXIT_REASON_INVLPG] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_RDPMC] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_RDTSC] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_RSM] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_VMCALL] = {
		.handler = undefined_vmexit_handler
		},
	[VMX_EXIT_REASON_VMCLEAR] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMLAUNCH] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRLD] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRST] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMREAD] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMRESUME] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMWRITE] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMXOFF] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMXON] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_CR_ACCESS] = {
		.handler = cr_access_vmexit_handler,
		.need_exit_qualification = 1U},
	[VMX_EXIT_REASON_DR_ACCESS] = {
		.handler = movdr_vmexit_handler},
	[VMX_EXIT_REASON_IO_INSTRUCTION] = {
		.handler = pio_instr_vmexit_handler,
		.need_exit_qualification = 1U},
	[VMX_EXIT_REASON_RDMSR] = {
		.handler = rdmsr_vmexit_handler},
	[VMX_EXIT_REASON_WRMSR] = {
		.handler = wrmsr_vmexit_handler},
	[VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_MWAIT] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR_TRAP] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_PAUSE] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_TPR_BELOW_THRESHOLD] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_APIC_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_VIRTUALIZED_EOI] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_GDTR_IDTR_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_LDTR_TR_ACCESS] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_EPT_VIOLATION] = {
		.handler = ept_violation_vmexit_handler,
		.need_exit_qualification = 1U},
	[VMX_EXIT_REASON_EPT_MISCONFIGURATION] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INVEPT] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_RDTSCP] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INVVPID] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_WBINVD] = {
		.handler = wbinvd_vmexit_handler},
	[VMX_EXIT_REASON_XSETBV] = {
		.handler = xsetbv_vmexit_handler},
	[VMX_EXIT_REASON_APIC_WRITE] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_RDRAND] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_INVPCID] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_VMFUNC] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_ENCLS] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_RDSEED] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_PAGE_MODIFICATION_LOG_FULL] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_XSAVES] = {
		.handler = unexpected_vmexit_handler},
	[VMX_EXIT_REASON_XRSTORS] = {
		.handler = unexpected_vmexit_handler}
};

int32_t vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct vm_exit_dispatch *dispatch = NULL;
	uint16_t basic_exit_reason;
	int32_t ret;

	if (get_pcpu_id() != pcpuid_from_vcpu(vcpu)) {
		pr_fatal("vcpu is not running on its pcpu!");
		ret = -EINVAL;
	} else {
		/* Obtain interrupt info */
		vcpu->arch.idt_vectoring_info = exec_vmread32(VMX_IDT_VEC_INFO_FIELD);
		/* Filter out HW exception & NMI */
		if ((vcpu->arch.idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
			uint32_t vector_info = vcpu->arch.idt_vectoring_info;
			uint32_t vector = vector_info & 0xffU;
			uint32_t type = (vector_info & VMX_INT_TYPE_MASK) >> 8U;
			uint32_t err_code = 0U;

			if (type == VMX_INT_TYPE_HW_EXP) {
				if ((vector_info & VMX_INT_INFO_ERR_CODE_VALID) != 0U) {
					err_code = exec_vmread32(VMX_IDT_VEC_ERROR_CODE);
				}
				vcpu_queue_exception(vcpu, vector, err_code);
				vcpu->arch.idt_vectoring_info = 0U;
			} else if (type == VMX_INT_TYPE_NMI) {
				vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
				vcpu->arch.idt_vectoring_info = 0U;
			} else {
				/* No action on EXT_INT or SW exception. */
			}
		}

		/* Calculate basic exit reason (low 16-bits) */
		basic_exit_reason = (uint16_t)(vcpu->arch.exit_reason & 0xFFFFU);

		/* Log details for exit */
		pr_dbg("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);

		/* Ensure exit reason is within dispatch table */
		if (basic_exit_reason >= ARRAY_SIZE(dispatch_table)) {
			pr_err("Invalid Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
			ret = -EINVAL;
		} else {
			/* Calculate dispatch table entry */
			dispatch = (struct vm_exit_dispatch *)(dispatch_table + basic_exit_reason);

			/* See if an exit qualification is necessary for this exit handler */
			if (dispatch->need_exit_qualification != 0U) {
				/* Get exit qualification */
				vcpu->arch.exit_qualification = exec_vmread(VMX_EXIT_QUALIFICATION);
			}

			/* exit dispatch handling */
			if (basic_exit_reason == VMX_EXIT_REASON_EXTERNAL_INTERRUPT) {

				ret = dispatch->handler(vcpu);
			} else {
				ret = dispatch->handler(vcpu);
			}
		}
	}

	return ret;
}

static int32_t unexpected_vmexit_handler(struct acrn_vcpu *vcpu)
{
	pr_fatal("Error: Unexpected VM exit condition from guest at 0x%016lx ", exec_vmread(VMX_GUEST_RIP));
	pr_fatal("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
	pr_err("Exit qualification: 0x%016lx ", exec_vmread(VMX_EXIT_QUALIFICATION));

	TRACE_2L(TRACE_VMEXIT_UNEXPECTED, vcpu->arch.exit_reason, 0UL);

	if(is_safety_vm(vcpu->vm)) {
		panic("Error: Unexpected VM exit!");
	}else {
		fatal_error_shutdown_vm(vcpu);
	}

	return 0;
}

int32_t cpuid_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t rax, rbx, rcx, rdx;

	rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);
	rbx = vcpu_get_gpreg(vcpu, CPU_REG_RBX);
	rcx = vcpu_get_gpreg(vcpu, CPU_REG_RCX);
	rdx = vcpu_get_gpreg(vcpu, CPU_REG_RDX);
	TRACE_2L(TRACE_VMEXIT_CPUID, rax, rcx);
	guest_cpuid(vcpu, (uint32_t *)&rax, (uint32_t *)&rbx, (uint32_t *)&rcx, (uint32_t *)&rdx);
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, rax);
	vcpu_set_gpreg(vcpu, CPU_REG_RBX, rbx);
	vcpu_set_gpreg(vcpu, CPU_REG_RCX, rcx);
	vcpu_set_gpreg(vcpu, CPU_REG_RDX, rdx);

	return 0;
}

/*
 * XSETBV instruction set's the XCR0 that is used to tell for which
 * components states can be saved on a context switch using xsave.
 *
 * According to SDM vol3 25.1.1:
 * Invalid-opcode exception (UD) and faults based on privilege level (include
 * virtual-8086 mode previleged instructions are not recognized) have higher
 * priority than VM exit.
 *
 * We don't need to handle those case here because we depends on VMX to handle
 * them.
 */
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t idx;
	uint64_t val64;
	int32_t ret = 0;

	val64 = exec_vmread(VMX_GUEST_CR4);
	if ((val64 & CR4_OSXSAVE) == 0UL) {
		vcpu_inject_gp(vcpu, 0U);
	} else {
		idx = vcpu->arch.cur_context;
		if (idx >= NR_WORLD) {
			ret = -1;
		} else {
			/* to access XCR0,'ecx' should be 0 */
			if ((vcpu_get_gpreg(vcpu, CPU_REG_RCX) & 0xffffffffUL) != 0UL) {
				vcpu_inject_gp(vcpu, 0U);
			} else {
				val64 = (vcpu_get_gpreg(vcpu, CPU_REG_RAX) & 0xffffffffUL) |
					(vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U);

				/* bit 0(x87 state) of XCR0 can't be cleared */
				if ((val64 & 0x01UL) == 0UL) {
					vcpu_inject_gp(vcpu, 0U);
				} else if ((val64 & XCR0_RESERVED_BITS) != 0UL) {
					vcpu_inject_gp(vcpu, 0U);
				} else {
					/*
					 * XCR0[2:1] (SSE state & AVX state) can't not be
					 * set to 10b as it is necessary to set both bits
					 * to use AVX instructions.
					 */
					if ((val64 & (XCR0_SSE | XCR0_AVX)) == XCR0_AVX) {
						vcpu_inject_gp(vcpu, 0U);
					} else {
						/*
						 * SDM Vol.1 13-4, XCR0[4:3] are associated with MPX state,
						 * Guest should not set these two bits without MPX support.
						 */
						if ((val64 & (XCR0_BNDREGS | XCR0_BNDCSR)) != 0UL) {
							vcpu_inject_gp(vcpu, 0U);
						} else {
							write_xcr(0, val64);
						}
					}
				}
			}
		}
	}

	return ret;
}

static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu)
{
	walk_ept_table(vcpu->vm, ept_flush_leaf_page);

	return 0;
}

/* vmexit handler for just injecting a #UD exception
 *
 * ACRN doesn't support nested virtualization, the following VMExit will inject #UD
 * VMCLEAR/VMLAUNCH/VMPTRST/VMREAD/VMRESUME/VMWRITE/VMXOFF/VMXON.
 * ACRN doesn't enable VMFUNC, VMFUNC treated as undefined.
 */
static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu_inject_ud(vcpu);
	return 0;
}

/*
 * This handler is only triggered by INIT signal when poweroff from inside of RTVM
 */
static int32_t init_signal_vmexit_handler(__unused struct acrn_vcpu *vcpu)
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
	vcpu_retain_rip(vcpu);
	return 0;
}

/**
 * @}
 */
static int32_t taskswitch_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu_inject_gp(vcpu, (uint32_t)(vcpu->arch.exit_qualification & 0xFFUL));
	return 0;
}


static int32_t invd_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu_inject_gp(vcpu, 0U);
	return 0;
}

static int32_t movdr_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	/*
	 * In the current usecase, triple fault will happen if vcpu inject gp during
	 * mov_dr handler. Here just do nothing to work around and this issue will be
	 * resolved in the future.
	 */
//	vcpu_inject_gp(vcpu, 0U);
	return 0;
}
