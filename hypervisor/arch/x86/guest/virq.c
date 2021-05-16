/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <virq.h>
#include <lapic.h>
#include <mmu.h>
#include <vmx.h>
#include <vcpu.h>
#include <vmcs.h>
#include <vm.h>
#include <trace.h>
#include <logmsg.h>
#include <vm_reset.h>
#include <console.h>

/**
 * @defgroup vp-base_virq vp-base.virq
 * @ingroup vp-base
 * @brief Implementation of all external APIs of guest IRQs.
 *
 * This module implements the exception related handlers of the guest vCPU.
 *
 * Usage:
 * - 'vp-base.hv_main' module depends on this module to handle the pending request
 *   and the VM exit caused by 'Exception or non-maskable interrupt (NMI)'.
 * - 'vp-base.vcr' and 'vp-base.hv_main' module depends on this module to inject GP
 *    and UD to the guest vCPU.
 * - 'vp-dm.io_req' module depends on this module to inject PF to the guest vCPU.
 * - 'vp-base.vcr' and 'hwmgmt.mmu' module depends on this module to notify the
 *    exception to the guest vCPU.
 * - 'vp-base.hv_main' and 'hwmgmt.irq' module depends on this module to inject the exception to
 *    the exception queue of the guest vCPU.
 * - 'vp-base.guest_mem' and 'vp-base.vcr' module depends on this module to notify the
 *    vCPU to flush its TLB.
 *
 * Dependency:
 * - This module depends on 'lib.bits' module to set or clear the bitmap of the
 *   target address.
 * - This module depends on 'vp-base.vcpu' module to notify the vcpu, retain the guest
 *   RIP, set the RFLAGS, set the CR2 and other vcpu related operations.
 * - This module depends on 'hwmgmt.vmx' module to read from or write to the VMCS fields.
 * - This module depends on 'vp-base.hv_main' module to initialize the VMCS fields.
 * - This module depends on 'vp-base.vm' module to judge if the VM is safety VM.
 * - This module depends on 'vp-base.vm_reset' module to shutdown the corresponding VM.
 * - This module depends on 'hwmgmt.mmu' module to invalidate cached EPT mappings.
 * - This module depends on 'hwmgmt.apic' module to initialize the LAPIC of the current physical CPU.
 * - This module depends on 'debug' module to set up timer for the console.
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the vp-base.virq module.
 *
 * This file implements all external functions, global variable, and macros that shall be provided by the
 * vp-base.virq module.
 *
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: get_exception_type and get_excep_class.
 *
 * Decomposed functions include: vcpu_inject_exception and vcpu_inject_lo_exception.
 */

/**
 * @brief Indicating that the error code needs to be delivered for the specified exception.
 */
#define EXCEPTION_ERROR_CODE_VALID 8U

#define EXCEPTION_CLASS_BENIGN 1  /**< Benign exception and interrupts */
#define EXCEPTION_CLASS_CONT   2  /**< Contributory exceptions */
#define EXCEPTION_CLASS_PF     3  /**< Page Faults */

/* Exception types */
#define EXCEPTION_FAULT     0U    /**< Exception type - Faults */
#define EXCEPTION_TRAP      1U    /**< Exception type - Traps */
#define EXCEPTION_ABORT     2U    /**< Exception type - Aborts */
#define EXCEPTION_INTERRUPT 3U    /**< Exception type - Interrupt */

/**
 * @brief The global array is used to store interrupt type and error code information.
 *
 * VMX_INT_TYPE_HW_EXP presents that interrupt type is hardware exception.
 * EXCEPTION_ERROR_CODE_VALID indicates that the error code needs to be
 * delivered for the specified exception.
 */
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

/**
 * @brief Get the exception/interrupt type information according to the vector number.
 *
 * The function is used to judge the exception type with the vector number and return the exception type.
 *
 * @param[in] vector A vector number
 *
 * @return Exception type
 *
 * @pre vector <= 255
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static uint8_t get_exception_type(uint32_t vector)
{
	/** Declare the following local variables of type 'uint8_t'.
	 *  - type representing the type of exceptions/interrupts, not initialized. */
	uint8_t type;

	/** If the \a vector presents NMI or user defined interrupt */
	if ((vector > 31U) || (vector == IDT_NMI)) {
		/** Set 'type' to 'EXCEPTION_INTERRUPT' */
		type = EXCEPTION_INTERRUPT;
	/** If the \a vector is DB, BP or OF */
	} else if ((vector == IDT_DB) || (vector == IDT_BP) || (vector == IDT_OF)) {
		/** Set 'type' to 'EXCEPTION_TRAP' */
		type = EXCEPTION_TRAP;
	/** If the \a vector is DF or MC */
	} else if ((vector == IDT_DF) || (vector == IDT_MC)) {
		/** Set 'type' to 'EXCEPTION_ABORT' */
		type = EXCEPTION_ABORT;
	} else {
		/** Set 'type' to 'EXCEPTION_FAULT' */
		type = EXCEPTION_FAULT;
	}

	/** Return 'type' */
	return type;
}

/**
 * @brief Submit a request to the given vCPU.
 *
 * The function is notify the event to the vCPU.
 *
 * @param[inout] vcpu A pointer to the vCPU structure where the request is submitted to.
 * @param[in] eventid A event id indicating the request to be submitted
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid)
{
	/** Call bitmap_set_lock with the following parameters, in order to set the target bit eventid to 1.
	 *  - eventid
	 *  - &vcpu->arch.pending_req
	 */
	bitmap_set_lock(eventid, &vcpu->arch.pending_req);
	/** Call kick_vcpu() with the following parameters, in order to notify the vCPU.
	 *  - vcpu
	 */
	kick_vcpu(vcpu);
}

/**
 * @brief Get the exception/interrupt class information according to the vector number.
 *
 * The function is used to judge the exception class with the vector number and return the exception type.
 *
 * @param[in] vector A vector number
 *
 * @return The exception class of the \a vector.
 *
 * @pre vector <= 255
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static int32_t get_excep_class(uint32_t vector)
{
	/** Declare the following local variables of type 'int32_t'.
	 *  - ret representing the class of exceptions/interrupts, not initialized. */
	int32_t ret;

	/** If the vector is DE, TS, NP, SS or GP */
	if ((vector == IDT_DE) || (vector == IDT_TS) || (vector == IDT_NP) || (vector == IDT_SS) ||
		(vector == IDT_GP)) {
		/** Set 'ret' to 'EXCEPTION_CLASS_CONT' */
		ret = EXCEPTION_CLASS_CONT;
	/** If the vector is PF or VE */
	} else if ((vector == IDT_PF) || (vector == IDT_VE)) {
		/** Set 'ret' to 'EXCEPTION_CLASS_PF' */
		ret = EXCEPTION_CLASS_PF;
	} else {
		/** Set 'ret' to 'EXCEPTION_CLASS_BENIGN' */
		ret = EXCEPTION_CLASS_BENIGN;
	}

	/** Return 'ret' */
	return ret;
}

/**
 * @brief Queue the exception into the target vCPU.
 *
 * The function is used to inject exception into vCPU, generally it requests to injection directly. But
 * this function checks if the VCPU is in a exception handling procedure, if yes, then
 * request to inject a proper second exception instead.
 *
 * @param[inout] vcpu A pointer points to the vcpu structure where the exception to be injected.
 * @param[in] vector_arg The vector number of the exception.
 * @param[in] err_code_arg The error code of the exception.
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vector_arg == IDT_PF ||  vector_arg == IDT_GP || vector_arg == IDT_UD || vector_arg == IDT_NMI
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void vcpu_queue_exception(struct acrn_vcpu *vcpu, uint32_t vector_arg, uint32_t err_code_arg)
{
	/** Declare the following local variables of type struct acrn_vcpu_arch *.
	 *  - arch representing a pointer points to the structure acrn_vcpu_arch associated with the vCPU,
	 *  initialized as &vcpu->arch.
	 */
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	/** Declare the following local variables of type 'uint32_t'.
	 *  - vector representing vector number of the exception,
	 *  initialized with vector_arg.
	 */
	uint32_t vector = vector_arg;
	/** Declare the following local variables of type 'uint32_t'.
	 *  - err_code representing error code of the exception,
	 *  initialized with err_code_arg.
	 */
	uint32_t err_code = err_code_arg;
	/** Declare the following local variables of type 'uint32_t'.
	 *  - prev_vector representing the previous vector information,
	 *  initialized with arch->exception_info.exception.
	 */
	uint32_t prev_vector = arch->exception_info.exception;
	/** Declare the following local variables of type 'int32_t'.
	 *  - new_class representing class of current exception, not initialized
	 *  - prev_class representing class of previous exception, not initialized
	 */
	int32_t new_class, prev_class;

	/** Set 'prev_class' to 'get_excep_class(prev_vector)' */
	prev_class = get_excep_class(prev_vector);
	/** Set 'new_class' to 'get_excep_class(vector)' */
	new_class = get_excep_class(vector);
	/** If prev_vector is IDT_DF and new_class does not equal to EXCEPTION_CLASS_BENIGN */
	if ((prev_vector == IDT_DF) && (new_class != EXCEPTION_CLASS_BENIGN)) {
		/** Call vcpu_make_request() with the following parameters, in order to notify the vCPU
		 *  that triple fault happens.
		 *  - vcpu
		 *  - ACRN_REQUEST_TRP_FAULT
		 */
		vcpu_make_request(vcpu, ACRN_REQUEST_TRP_FAULT);
	} else {
		/** If any one of the following conditions are satisfied:
		 * 1. both prev_class and new_class are equal to EXCEPTION_CLASS_CONT
		 * 2. prev_class is equals to EXCEPTION_CLASS_PF while new_class is not EXCEPTION_CLASS_BENIGN
		 */
		if (((prev_class == EXCEPTION_CLASS_CONT) && (new_class == EXCEPTION_CLASS_CONT)) ||
			((prev_class == EXCEPTION_CLASS_PF) && (new_class != EXCEPTION_CLASS_BENIGN))) {
			/** Set 'vector' to 'IDT_DF' */
			vector = IDT_DF;
			/** Set 'err_code' to 0 */
			err_code = 0U;
		}

		/** Set 'arch->exception_info.exception' to 'vector' */
		arch->exception_info.exception = vector;

		/** If exception_type[vector] bitwise AND EXCEPTION_ERROR_CODE_VALID is not equals to 0 */
		if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
			/** Set 'arch->exception_info.error' to 'err_code' */
			arch->exception_info.error = err_code;
		} else {
			/** Set 'arch->exception_info.error' to '0' */
			arch->exception_info.error = 0U;
		}
		/** Call vcpu_make_request() with the following parameters, in order to submit exception injection
		 *  to the vCPU.
		 *  - vcpu
		 *  - ACRN_REQUEST_EXCP
		 */
		vcpu_make_request(vcpu, ACRN_REQUEST_EXCP);
	}
}

/**
 * @brief Inject the exception to the target vCPU.
 *
 * The function is used to write the exception information to the VMCS and retain the guest RIP,
 * so that the exception will be injected to the target vCPU at next VM-Entry.
 *
 * @param[inout] vcpu A pointer points to a vcpu structure which representing the vCPU
 * where the exception to be injected.
 *
 * @return true (1) if exception is injected otherwise false (0)
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static bool vcpu_inject_exception(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type 'uint32_t'.
	 *  - vector representing exception vector to be injected,
	 *  initialized as vcpu->arch.exception_info.exception.
	 */
	uint32_t vector = vcpu->arch.exception_info.exception;

	/** Declare the following local variables of type 'bool'.
	 *  - injected representing exception vector is already injected,
	 *  initialized as false.
	 */
	bool injected = false;

	/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_EXCP, &vcpu->arch.pending_req)
	 * is true */
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_EXCP, &vcpu->arch.pending_req)) {

		/** If exception_type[vector] bitwise AND EXCEPTION_ERROR_CODE_VALID is not 0 */
		if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
			/** Call exec_vmwrite32() with the following parameters, in order to
			 *  write vcpu->arch.exception_info.error to the VMCS field VMX_ENTRY_EXCEPTION_ERROR_CODE.
			 *  - VMX_ENTRY_EXCEPTION_ERROR_CODE
			 *  - vcpu->arch.exception_info.error
			 */
			exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, vcpu->arch.exception_info.error);
		}

		/** Call exec_vmwrite32() with the following parameters, in order to
		 *  write VMX_INT_INFO_VALID | (exception_type[vector] << 8U) | (vector & 0xFFU)
		 *  to the VMCS field VMX_ENTRY_INT_INFO_FIELD.
		 *  - VMX_ENTRY_INT_INFO_FIELD
		 *  - VMX_INT_INFO_VALID | (exception_type[vector] << 8U) | (vector & 0xFFU))
		 */
		exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
			VMX_INT_INFO_VALID | (exception_type[vector] << 8U) | (vector & 0xFFU));

		/** Set 'vcpu->arch.exception_info.exception' to 'VECTOR_INVALID' */
		vcpu->arch.exception_info.exception = VECTOR_INVALID;

		/* If return value of get_exception_type(vector) is EXCEPTION_FAULT */
		if (get_exception_type(vector) == EXCEPTION_FAULT) {
			/** Call vcpu_retain_rip() with the following parameters, in order to retain the guest RIP.
			 *  - vcpu
			 */
			vcpu_retain_rip(vcpu);
		}

		/** If return value of get_exception_type() with vector being parameter is EXCEPTION_FAULT */
		if (get_exception_type(vector) == EXCEPTION_FAULT) {
			/** Call vcpu_set_rflags() with the following parameters, in order to set
			 *  RF bit of the guest RFLAGS.
			 *  - vcpu
			 *  - vcpu_get_rflags(vcpu) | HV_ARCH_VCPU_RFLAGS_RF
			 */
			vcpu_set_rflags(vcpu, vcpu_get_rflags(vcpu) | HV_ARCH_VCPU_RFLAGS_RF);
		}
		/** Set injected to true. */
		injected = true;
	}
		/** Return injected. */
		return injected;
}

/**
 * @brief Inject the exception GP to the target vCPU.
 *
 * The function is used to inject GP to the target vCPU with error code.
 *
 * @param[inout] vcpu A pointer points to a vcpu structure which representing the vCPU
 * where the exception to be injected.
 * @param[in] err_code The error code of the exception.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code)
{
	/** Call vcpu_queue_exception() with the following parameters, in order to inject GP to the target vCPU.
	 *  - vcpu
	 *  - IDT_GP
	 *  - err_code
	 */
	vcpu_queue_exception(vcpu, IDT_GP, err_code);
}

/**
 * @brief Inject the exception PF to the target vCPU.
 *
 * The function is used to inject PF to the target vCPU.
 *
 * @param[inout] vcpu A pointer points to a vcpu structure which representing the vCPU
 * where the exception to be injected.
 * @param[in] addr The linear address that caused a page fault
 * @param[in] err_code The error code of the exception
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code)
{
	/** Call vcpu_set_cr2() with the following parameters, in order to set addr
	 *  to the guest CR2 of the vCPU.
	 *  - vcpu
	 *  - addr
	 */
	vcpu_set_cr2(vcpu, addr);
	/** Call vcpu_queue_exception() with the following parameters, in order to inject PF to the exception queue.
	 *  - vcpu
	 *  - IDT_PF
	 *  - err_code
	 */
	vcpu_queue_exception(vcpu, IDT_PF, err_code);
}

/**
 * @brief Inject the exception UD to the target vCPU.
 *
 * The function is used to inject UD to the target vCPU.
 *
 * @param[inout] vcpu A pointer points to a vcpu structure which representing the vCPU
 * where the exception to be injected.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void vcpu_inject_ud(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_queue_exception() with the following parameters, in order to inject UD to the exception queue.
	 *  - vcpu
	 *  - IDT_UD
	 *  - 0
	 */
	vcpu_queue_exception(vcpu, IDT_UD, 0U);
}

/**
 * @brief Handle the pending request.
 *
 * The function is used to handle the pending request which is not handled yet.
 *
 * @param[inout] vcpu A pointer points to a vcpu structure which representing the vCPU
 * whose request will be handled
 *
 * @return The status when handling pending request.
 * @retval -EFAULT when triple fault happens during handling pending request.
 * @retval 0 Otherwise.
 *
 * @pre vcpu != NULL
 * @pre pcpuid_from_vcpu(vcpu) == get_pcpu_id()
 * @pre vcpu->arch.exception_info.exception == IDT_PF ||  vcpu->arch.exception_info.exception
 * == IDT_GP || vcpu->arch.exception_info.exception == IDT_UD || vcpu->arch.exception_info.exception
 * == IDT_NMI || vcpu->arch.exception_info.exception == IDT_DF
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type 'bool'.
	 *  - injected representing if the exception is already injected, initialized as false.
	 */
	bool injected = false;
	/** Declare the following local variables of type 'int32_t'.
	 *  - ret representing the status when handing pending request, initialized as 0.
	 */
	int32_t ret = 0;
	/** Declare the following local variables of type 'struct acrn_vcpu_arch *'.
	 *  - arch representing a pointer points to the structure acrn_vcpu_arch associated with the vCPU,
	 *  initialized as &vcpu->arch.
	 */
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pending_req_bits representing a pointer points to the bitmap containing the
	 *  pending request information, initialized as &arch->pending_req.
	 */
	uint64_t *pending_req_bits = &arch->pending_req;

	/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_INIT_VMCS, pending_req_bits) is true */
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_INIT_VMCS, pending_req_bits)) {
		/** Call init_VMCS() with the following parameters, in order to initialize
		 *  the VMCS fields of the target vCPU.
		 *  - vcpu
		 */
		init_vmcs(vcpu);
		/** Call console_setup_timer(), in order to set up timer for the console.
		 */
		console_setup_timer();
	}

	/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_TRP_FAULT, pending_req_bits) is true */
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_TRP_FAULT, pending_req_bits)) {
		/** Logging the following information with a log level of LOG_FATAL.
		 *  - Triple fault happen -> shutdown! */
		pr_fatal("Triple fault happen -> shutdown!");
		/** Set 'ret' to '-EFAULT' */
		ret = -EFAULT;
	} else {
		/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_LAPIC_RESET, pending_req_bits) is true */
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_LAPIC_RESET, pending_req_bits)) {
			/** Call init_lapic() with the following parameters, in order
			 *  to initialize the LAPIC of the current physical CPU.
			 *  - get_pcpu_id()
			 */
			init_lapic(get_pcpu_id());
		}

		/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_EPT_FLUSH, pending_req_bits)
		 *  is true */
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EPT_FLUSH, pending_req_bits)) {
			/** Call invept() with the following parameters, in order
			 *  to invalidate cached EPT mappings.
			 *  - vcpu->vm->arch_vm.nworld_eptp
			 */
			invept(vcpu->vm->arch_vm.nworld_eptp);
		}
		/** Call vcpu_inject_exception() with vcpu being parameters, in order
		 *  to inject exception to the target vCPU, and then set
		 *  injected to its return value.
		 */
		injected = vcpu_inject_exception(vcpu);

		/** If injected is false */
		if (!injected) {
			/** If return value of bitmap_test_and_clear_lock(ACRN_REQUEST_NMI, pending_req_bits) is true */
			if (bitmap_test_and_clear_lock(ACRN_REQUEST_NMI, pending_req_bits)) {
				/** Call exec_vmwrite32() with the following parameters, in order to
				 *  write VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8U) | IDT_NMI
				 *  to the VMCS field VMX_ENTRY_INT_INFO_FIELD.
				 *  - VMX_ENTRY_INT_INFO_FIELD
				 *  - VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8U) | IDT_NMI
				 */
				exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
					VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8U) | IDT_NMI);
			} else {
				/* handling pending vector injection:
				* there are many reason inject failed, we need re-inject again
				* here should take care
				* - SW exception (not maskable by IF)
				* - external interrupt, if IF clear, will keep in IDT_VEC_INFO_FIELD
				*   at next vm exit?
				*/
				/** If bit 31(valid interrupt) of arch->idt_vectoring_info is not clear. */
				if ((arch->idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
					/** Call exec_vmwrite() with the following parameters, in order
					 *  to write arch->idt_vectoring_info to the VMCS field:
					 * 'VM-entry interruption-information'.
					 *  - VMX_ENTRY_INT_INFO_FIELD
					 *  - arch->idt_vectoring_info
					 */
					exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, arch->idt_vectoring_info);
					/** Set 'arch->idt_vectoring_info' to '0' */
					arch->idt_vectoring_info = 0U;
				}
			}
		}


	}
	/** Return ret */
	return ret;
}

/**
 * @brief Handle the exception when the VM Exit reason is 'Exception or non-maskable interrupt (NMI)'.
 *
 * The function is used to handle the guest exception which cause VM-exits.
 *
 * @param[inout] vcpu A pointer points to the vCPU which triggers the VM exit with the reason
 * 'exception or non-maskable interrupt (NMI)'
 *
 * @return The error code indicating whether an error is detected during handling the VM exit.
 * @retval 0 When VM Exit handling is succeed.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type 'uint32_t'.
	 *  - intinfo representing interrupt information, not initialized.
	 */
	uint32_t intinfo;
	/** Declare the following local variables of type 'uint32_t'.
	 *  - exception_vector representing vector of the exception,
	 *  initialized as VECTOR_INVALID.
	 */
	uint32_t exception_vector = VECTOR_INVALID;

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - Handling guest exception */
	pr_dbg(" Handling guest exception");

	/** Call exec_vmread32() with VMX_EXIT_INT_INFO being parameter to get
	 *  current VM-Exit interrupt information and set intinfo to its return value.
	 */
	intinfo = exec_vmread32(VMX_EXIT_INT_INFO);
	/** If bit 31 of intinfo is not 0 */
	if ((intinfo & VMX_INT_INFO_VALID) != 0U) {
		/** Set 'exception_vector' to 'intinfo & FFH' */
		exception_vector = intinfo & 0xFFU;
	}

	/** Call vcpu_retain_rip() with the following parameters, in order to retain the guest RIP.
	 *  - vcpu
	 */
	vcpu_retain_rip(vcpu);

	/** Depending on 'exception_vector' */
	switch (exception_vector) {
	/** exception_vector is IDT_DB. */
	case IDT_DB:
		/** Call vcpu_inject_gp() with the following parameters, in order to
		 *  inject GP with error code 0 to the guest vCPU.
		 *  - vcpu
		 *  - 0U
		 */
		vcpu_inject_gp(vcpu, 0U);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** If return value of is_safety_vm(vcpu->vm) is true */
		if (is_safety_vm(vcpu->vm)) {
			/** Call panic with the following parameters, in order to print error information
			 *  and panic the corresponding physical CPU.
			 *  - exception_vector
			 */
			panic("Unexpected Exception from guest, vector: 0x%x!", exception_vector);
		} else {
			/** Call fatal_error_shutdown_vm() with the following parameters, in order to
			 *  shutdown the corresponding VM.
			 *  - vcpu
			 */
			fatal_error_shutdown_vm(vcpu);
		}
	}
	/** Return 0 */
	return 0;
}

/**
 * @}
 */
