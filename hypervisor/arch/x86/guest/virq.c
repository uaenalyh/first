/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <irq.h>
#include <lapic.h>
#include <mmu.h>
#include <vmx.h>
#include <vcpu.h>
#include <vmcs.h>
#include <vm.h>
#include <trace.h>
#include <logmsg.h>
#include <vm_reset.h>

/**
 * @defgroup vp-base_virq vp-base.virq
 * @ingroup vp-base
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define EXCEPTION_ERROR_CODE_VALID 8U

#define EXCEPTION_CLASS_BENIGN 1
#define EXCEPTION_CLASS_CONT   2
#define EXCEPTION_CLASS_PF     3

/* Exception types */
#define EXCEPTION_FAULT     0U
#define EXCEPTION_TRAP      1U
#define EXCEPTION_ABORT     2U
#define EXCEPTION_INTERRUPT 3U

static const uint32_t exception_type[32] = { [0] = VMX_INT_TYPE_HW_EXP,
	[1] = VMX_INT_TYPE_HW_EXP,
	[2] = VMX_INT_TYPE_HW_EXP,
	[3] = VMX_INT_TYPE_HW_EXP,
	[4] = VMX_INT_TYPE_HW_EXP,
	[5] = VMX_INT_TYPE_HW_EXP,
	[6] = VMX_INT_TYPE_HW_EXP,
	[7] = VMX_INT_TYPE_HW_EXP,
	[8] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[9] = VMX_INT_TYPE_HW_EXP,
	[10] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[11] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[12] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[13] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[14] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[15] = VMX_INT_TYPE_HW_EXP,
	[16] = VMX_INT_TYPE_HW_EXP,
	[17] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[18] = VMX_INT_TYPE_HW_EXP,
	[19] = VMX_INT_TYPE_HW_EXP,
	[20] = VMX_INT_TYPE_HW_EXP,
	[21] = VMX_INT_TYPE_HW_EXP,
	[22] = VMX_INT_TYPE_HW_EXP,
	[23] = VMX_INT_TYPE_HW_EXP,
	[24] = VMX_INT_TYPE_HW_EXP,
	[25] = VMX_INT_TYPE_HW_EXP,
	[26] = VMX_INT_TYPE_HW_EXP,
	[27] = VMX_INT_TYPE_HW_EXP,
	[28] = VMX_INT_TYPE_HW_EXP,
	[29] = VMX_INT_TYPE_HW_EXP,
	[30] = VMX_INT_TYPE_HW_EXP,
	[31] = VMX_INT_TYPE_HW_EXP };

static uint8_t get_exception_type(uint32_t vector)
{
	uint8_t type;

	/* Treat #DB as trap until decide to support Debug Registers */
	if ((vector > 31U) || (vector == IDT_NMI)) {
		type = EXCEPTION_INTERRUPT;
	} else if ((vector == IDT_DB) || (vector == IDT_BP) || (vector == IDT_OF)) {
		type = EXCEPTION_TRAP;
	} else if ((vector == IDT_DF) || (vector == IDT_MC)) {
		type = EXCEPTION_ABORT;
	} else {
		type = EXCEPTION_FAULT;
	}

	return type;
}

static bool is_guest_irq_enabled(struct acrn_vcpu *vcpu)
{
	uint64_t guest_rflags, guest_state;
	bool status = false;

	/* Read the RFLAGS of the guest */
	guest_rflags = vcpu_get_rflags(vcpu);
	/* Check the RFLAGS[IF] bit first */
	if ((guest_rflags & HV_ARCH_VCPU_RFLAGS_IF) != 0UL) {
		/* Interrupts are allowed */
		/* Check for temporarily disabled interrupts */
		guest_state = exec_vmread32(VMX_GUEST_INTERRUPTIBILITY_INFO);

		if ((guest_state & (HV_ARCH_VCPU_BLOCKED_BY_STI | HV_ARCH_VCPU_BLOCKED_BY_MOVSS)) == 0UL) {
			status = true;
		}
	}
	return status;
}

void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid)
{
	bitmap_set_lock(eventid, &vcpu->arch.pending_req);
	kick_vcpu(vcpu);
}

/* SDM Vol3 -6.15, Table 6-4 - interrupt and exception classes */
static int32_t get_excep_class(uint32_t vector)
{
	int32_t ret;

	if ((vector == IDT_DE) || (vector == IDT_TS) || (vector == IDT_NP) || (vector == IDT_SS) ||
		(vector == IDT_GP)) {
		ret = EXCEPTION_CLASS_CONT;
	} else if ((vector == IDT_PF) || (vector == IDT_VE)) {
		ret = EXCEPTION_CLASS_PF;
	} else {
		ret = EXCEPTION_CLASS_BENIGN;
	}

	return ret;
}

int32_t vcpu_queue_exception(struct acrn_vcpu *vcpu, uint32_t vector_arg, uint32_t err_code_arg)
{
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	uint32_t vector = vector_arg;
	uint32_t err_code = err_code_arg;
	int32_t ret = 0;

	/* VECTOR_INVALID is also greater than 32 */
	if (vector >= 32U) {
		pr_err("invalid exception vector %d", vector);
		ret = -EINVAL;
	} else {

		uint32_t prev_vector = arch->exception_info.exception;
		int32_t new_class, prev_class;

		/* SDM vol3 - 6.15, Table 6-5 - conditions for generating a
		 * double fault */
		prev_class = get_excep_class(prev_vector);
		new_class = get_excep_class(vector);
		if ((prev_vector == IDT_DF) && (new_class != EXCEPTION_CLASS_BENIGN)) {
			/* triple fault happen - shutdwon mode */
			vcpu_make_request(vcpu, ACRN_REQUEST_TRP_FAULT);
		} else {
			if (((prev_class == EXCEPTION_CLASS_CONT) && (new_class == EXCEPTION_CLASS_CONT)) ||
				((prev_class == EXCEPTION_CLASS_PF) && (new_class != EXCEPTION_CLASS_BENIGN))) {
				/* generate double fault */
				vector = IDT_DF;
				err_code = 0U;
			} else {
				/* Trigger the given exception instead of override it with
				 * double/triple fault. */
			}

			arch->exception_info.exception = vector;

			if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
				arch->exception_info.error = err_code;
			} else {
				arch->exception_info.error = 0U;
			}
			vcpu_make_request(vcpu, ACRN_REQUEST_EXCP);
		}
	}

	return ret;
}

static void vcpu_inject_exception(struct acrn_vcpu *vcpu, uint32_t vector)
{
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_EXCP, &vcpu->arch.pending_req)) {

		if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
			exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, vcpu->arch.exception_info.error);
		}

		exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
			VMX_INT_INFO_VALID | (exception_type[vector] << 8U) | (vector & 0xFFU));

		vcpu->arch.exception_info.exception = VECTOR_INVALID;

		/* retain rip for exception injection */
		vcpu_retain_rip(vcpu);

		/* SDM 17.3.1.1 For any fault-class exception except a debug exception generated in response to an
		 * instruction breakpoint, the value pushed for RF is 1.
		 * #DB is treated as Trap in get_exception_type, so RF will not be set for instruction breakpoint.
		 */
		if (get_exception_type(vector) == EXCEPTION_FAULT) {
			vcpu_set_rflags(vcpu, vcpu_get_rflags(vcpu) | HV_ARCH_VCPU_RFLAGS_RF);
		}
	}
}

static bool vcpu_inject_lo_exception(struct acrn_vcpu *vcpu)
{
	uint32_t vector = vcpu->arch.exception_info.exception;
	bool injected = false;

	/* high priority exception already be injected */
	if (vector <= NR_MAX_VECTOR) {
		vcpu_inject_exception(vcpu, vector);
		injected = true;
	}

	return injected;
}

/* Inject general protection exception(#GP) to guest */
void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code)
{
	(void)vcpu_queue_exception(vcpu, IDT_GP, err_code);
}

/* Inject page fault exception(#PF) to guest */
void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code)
{
	vcpu_set_cr2(vcpu, addr);
	(void)vcpu_queue_exception(vcpu, IDT_PF, err_code);
}

/* Inject invalid opcode exception(#UD) to guest */
void vcpu_inject_ud(struct acrn_vcpu *vcpu)
{
	(void)vcpu_queue_exception(vcpu, IDT_UD, 0U);
}

int32_t interrupt_window_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t value32;

	TRACE_2L(TRACE_VMEXIT_INTERRUPT_WINDOW, 0UL, 0UL);

	/* Disable interrupt-window exiting first.
	 * acrn_handle_pending_request will continue handle for this vcpu
	 */
	vcpu->arch.irq_window_enabled = false;
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	value32 &= ~(VMX_PROCBASED_CTLS_IRQ_WIN);
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);

	vcpu_retain_rip(vcpu);
	return 0;
}

int32_t external_interrupt_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t intr_info;
	struct intr_excp_ctx ctx;
	int32_t ret;

	intr_info = exec_vmread32(VMX_EXIT_INT_INFO);
	if (((intr_info & VMX_INT_INFO_VALID) == 0U) ||
		(((intr_info & VMX_INT_TYPE_MASK) >> 8U) != VMX_INT_TYPE_EXT_INT)) {
		pr_err("Invalid VM exit interrupt info:%x", intr_info);
		vcpu_retain_rip(vcpu);
		ret = -EINVAL;
	} else {
		ctx.vector = intr_info & 0xFFU;
		ctx.rip = vcpu_get_rip(vcpu);
		ctx.rflags = vcpu_get_rflags(vcpu);
		ctx.cs = exec_vmread32(VMX_GUEST_CS_SEL);

		dispatch_interrupt(&ctx);
		vcpu_retain_rip(vcpu);

		TRACE_2L(TRACE_VMEXIT_EXTERNAL_INTERRUPT, ctx.vector, 0UL);
		ret = 0;
	}

	return ret;
}

int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu)
{
	bool injected = false;
	int32_t ret = 0;
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	uint64_t *pending_req_bits = &arch->pending_req;

	/* make sure ACRN_REQUEST_INIT_VMCS handler as the first one */
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_INIT_VMCS, pending_req_bits)) {
		init_vmcs(vcpu);
		console_setup_timer();
	}

	if (bitmap_test_and_clear_lock(ACRN_REQUEST_TRP_FAULT, pending_req_bits)) {
		pr_fatal("Triple fault happen -> shutdown!");
		ret = -EFAULT;
	} else {
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_LAPIC_RESET, pending_req_bits)) {
			init_lapic(get_pcpu_id());
		}

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EPT_FLUSH, pending_req_bits)) {
			invept(vcpu->vm->arch_vm.nworld_eptp);
		}

		/* inject NMI before maskable hardware interrupt */
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_NMI, pending_req_bits)) {
			/* Inject NMI vector = 2 */
			exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
				VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8U) | IDT_NMI);
			injected = true;
			if (bitmap_test(ACRN_REQUEST_EXCP, pending_req_bits)) {
				vcpu_retain_rip(vcpu);
			}
		} else {
			/* handling pending vector injection:
			 * there are many reason inject failed, we need re-inject again
			 * here should take care
			 * - SW exception (not maskable by IF)
			 * - external interrupt, if IF clear, will keep in IDT_VEC_INFO_FIELD
			 *   at next vm exit?
			 */
			if ((arch->idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
				exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, arch->idt_vectoring_info);
				arch->idt_vectoring_info = 0U;
				injected = true;
			}
		}

		if (!injected) {
			/* if there is no eligible vector before this point */
			/* SDM Vol3 table 6-2, inject lowpri exception */
			(void)vcpu_inject_lo_exception(vcpu);
		}
	}

	return ret;
}

/*
 * @pre vcpu != NULL
 */
int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t intinfo;
	uint32_t exception_vector = VECTOR_INVALID;

	pr_dbg(" Handling guest exception");

	/* Obtain VM-Exit information field pg 2912 */
	intinfo = exec_vmread32(VMX_EXIT_INT_INFO);
	if ((intinfo & VMX_INT_INFO_VALID) != 0U) {
		exception_vector = intinfo & 0xFFU;
	}

	/* Handle all other exceptions */
	vcpu_retain_rip(vcpu);

	switch(exception_vector) {
	case IDT_DB:
		/* inject #GP(0) for #DB */
		vcpu_inject_gp(vcpu, 0U);
		break;
	default:
		if(is_safety_vm(vcpu->vm)) {
			panic("Unexpected Exception from guest, vector: 0x%x!", exception_vector);
		} else {
			fatal_error_shutdown_vm(vcpu);
		}
	}

	return 0;
}

/**
 * @}
 */
