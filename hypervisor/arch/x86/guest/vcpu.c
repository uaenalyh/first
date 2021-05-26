/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vcpu.h>
#include <vmsr.h>
#include <bits.h>
#include <vmx.h>
#include <logmsg.h>
#include <cpu_caps.h>
#include <per_cpu.h>
#include <init.h>
#include <vm.h>
#include <vmcs.h>
#include <mmu.h>
#include <virq.h>

/**
 * @defgroup vp-base_vcpu vp-base.vcpu
 * @ingroup vp-base
 * @brief Implementation notions of vCPU and related utility functions and external APIs.
 *
 * This module implements the vCPU infrastructures including vCPU registers operation, vCPU state
 * related operations and get other resource associated with the vCPU.
 *
 * Usage Remarks: vp-dm component and other vp-base modules use this module to set/get vCPU
 * registers, general purpose registers, MSR, rip, IA32_EFER and other vCPU contents, control vCPU
 * running states and so on.
 *
 * Dependency Justification: this module uses cpu, vmx, mmu, libs, schedule and other modules as basic infrastructures.
 *
 * External APIs:
 *  - init_vcpu_protect_mode_regs()      This function is used to initialize the registers before the
 *  target vCPU is launched in protected mode.
 *				   Depends on:
 *				    - memcpy_s()
 *				    - copy_to_gpa()
 *				    - set_vcpu_regs()
 *
 *  - is_long_mode() This function identifies if the vCPU is in IA-32e mode or other mode.
 *				   Depends on:
 *				    - vcpu_get_efer()
 *
 *  - is_pae()     This function identifies if the CR4.PAE bit of vCPU is set or clear.
 *				   Depends on:
 *				    - vcpu_get_cr4()
 *  - is_paging_enabled()    This function identifies if the CR0.PG bit of vCPU is set or clear.
 *				   Depends on:
 *				    - vcpu_get_cr0()
 *  - offline_vcpu()   This function is used to reset all fields in a vCPU instance, the vCPU state
 *					   is reset to VCPU_INIT.
 *				   Depends on:
 *				    - N/A
 *  - pause_vcpu()    This function is used to pause the dedicated vCPU.
 *				   Depends on:
 *				    - get_schedule_lock()
 *				    - remove_from_cpu_runqueue()
 *				    - make_reschedule_request()
 *				    - release_schedule_lock()
 *				    - asm_pause()
 *  - prepare_vcpu()   This function is to return result of creating vcpu and prepare the sched_obj
 *                     after created successfully.
 *				   Depends on:
 *					- build_stack_frame()
 *  - reset_vcpu()    This function is used to reset vCPU including vlapic, vCPU registers and all other context.
 *				   Depends on:
 *				    - memset()
 *					- vcpu_make_request()
 *  - run_vcpu()    This function is used to set RIP, RSP, CR0, CR4 and other related registers, launch vCPU,
 *					carry out an VM entry to the vCPU once and returns after an VM exit occurs.
 *				   Depends on:
 *				    - exec_vmwrite()
 *				    - exec_vmwrite64()
 *				    - exec_vmwrite16()
 *				    - flush_vpid_global()
 *				    - cpu_internal_buffers_clear()
 *  - set_vcpu_regs()  This function set the value of GDTR, IDTR, LDTR, TR, CS, IA32_EFER, RFLAGS, RIP, RSP, CR0,
 *                     CR3 and CR4 registers, and set mode of the vCPU.
 *				   Depends on:
 *				    - memcpy_s()
 *  - set_vcpu_startup_entry()   This function is used to set the startup related state of the target vcpu.
 *				   Depends on:
 *				    - N/A
 *  - vcpu_get_efer()    This function is to get the value of IA32_EFER register
 *				   Depends on:
 *				    - bitmap_test()
 *				    - bitmap_test_and_set_lock()
 *				    - exec_vmread64()
 *  - vcpu_get_gpreg()    This function is used to get the value of the guest general purpose registers.
 *				   Depends on:
 *				    - vcpu_get_cr0()
 *  - vcpu_get_guest_msr()    This function is used to get the guest msr from the vCPU structure.
 *				   Depends on:
 *				    - vmsr_get_guest_msr_index()
 *  - vcpu_get_rflags()    This function is used to get RFLAGS by vmread instruction.
 *				   Depends on:
 *				    -  exec_vmread64()
 *  - vcpu_get_rip()    This function is used to get guest rip.
 *				   Depends on:
 *				    - exec_vmread64()
 *  - vcpu_retain_rip()    This function is used to retain the guest RIP in the VMCS.
 *				   Depends on:
 *				    - N/A
 *  - vcpu_set_efer()    This function is used to set guest IA32_EFER of current vCPU.
 *				   Depends on:
 *				    - bitmap_set_lock()
 *  - vcpu_set_gpreg()    This function is used to set value of guest general purpose registers such as rax, rbx,
 *						  etc with specified value.
 *				   Depends on:
 *				    - N/A
 *  - vcpu_set_guest_msr()    This function is used to set specified value of msr to guest vCPU if the msr is valid.
 *				   Depends on:
 *				    - vmsr_get_guest_msr_index()
 *  - vcpu_set_rflags()    This function is used to a value to guest IA32_EFER of a vCPU.
 *				   Depends on:
 *				    - bitmap_set_lock()
 *  - vcpu_set_rip()    This function is used to set a value to guest RIP of a vCPU.
 *				   Depends on:
 *				    - bitmap_set_lock()
 *  - vcpu_vlapic()    This function is used to return the vLAPIC structure of a vCPU.
 *				   Depends on:
 *				    - bitmap_set_lock()
 *  - vcpumask2pcpumask()    The function converts a bitmap of vCPUs of a VM to a bitmap of corresponding
 *                           pCPUs they run on.
 *				   Depends on:
 *				    - bitmap_set_nolock()
 *
 * Internal functions:
 *  - vcpu_set_rip()    The function is used to set value of updated rsp to guest vCPU.
 *				   Depends on:
 *				    - bitmap_set_lock()
 *  - set_vcpu_mode()   The function is used to used to set the target vcpu to relevant mode according to CS attributes,
 *                      IA32_EFER and CR0 values.
 *				   Depends on:
 *				    - N/A
 *  - init_xsave()    This function is used to initialize the xsave components of the target vcpu.
 *				   Depends on:
 *				    - set_vcpu_regs()
 *  - reset_vcpu_regs()    This function is used to reset vcpu registers of the target vcpu.
 *				   Depends on:
 *				    - N/A
 *  - create_vcpu()    The function is used to create a vCPU.
 *				   Depends on:
 *				    - N/A
 *  - kick_vcpu()    This function is used to notify a vCPU of pending requests that it must handle immediately.
 *				   Depends on:
 *				    - kick_thread()
 *  - build_stack_frame()    The function is used to build the stack frame of the target vcpu .
 *				   Depends on:
 *				    - kick_thread()
 *  - save_xsave_area()    This function is used to save the physical state-component bitmaps and XSAVE-managed
 *                         user and supervisor state components to the given extended context.
 *				   Depends on:
 *				    - read_xcr()
 *				    - msr_read()
 *  - rstore_xsave_area()   The function is used to restore the physical state-component bitmaps and XSAVE-managed
 *                          user and supervisor state components from the given extended context.
 *				   Depends on:
 *				    - read_xcr()
 *				    - msr_write()
 *  - context_switch_out()    The function is used to save the extend context for target thread.
 *				   Depends on:
 *				    - read_xcr()
 *				    - msr_read()
 *  - context_switch_in()    The function is used to restore the extend context for target thread.
 *				   Depends on:
 *				    - read_xcr()
 *				    - msr_read()
 *  - launch_vcpu()    The function is used to set the vCPU state to VCPU_RUNNING and wake the vCPU thread.
 *				   Depends on:
 *				    - wake_thread()
 *  - pcpuid_from_vcpu()    The function is used to get the pcpuid of the target vCPU.
 *				   Depends on:
 *				    - sched_get_pcpuid()
 * @{
 */

/**
 * @file arch/x86/guest/vmx_asm.S
 *
 * @brief This file provides the VM entry sequence and entry point for VM exits
 *
 * This file implements a function, namely vmx_vmrun, that conducts an VM entry and returns after an VM exit. Taking a
 * pointer to a structure of type run_context and a signed integer being either VM_LAUNCH or VM_RESUME, this function
 * restores registers listed in the run_context with the given values, executes a vm_luanch or vm_resume instruction as
 * specified, saves guest states in the given run_context, and returns VM_FAIL if an VM entry failure occurs; otherwise
 * the function returns VM_SUCCESS.
 *
 * Refer to the control flow diagrams in section 11.4.8.4.17 (with vmx_run_start being the source node) and section
 * 11.4.8.5.1 in the Software Architecture Design Specification for detailed behavior of this function.
 */

/**
 * @file
 * @brief This file implements APIs that shall be provided by the vCPU module.
 */

/**
 * @brief The initial value of FPU Control Word field in the legacy region of an XSAVE area following STARTUP.
 */
#define XSAVE_STARTUP_FPU_CONTROL_WORD		0x40U
/**
 * @brief The initial value of FPU Tag Word field in the legacy region of an XSAVE area following STARTUP.
 */
#define XSAVE_STARTUP_FPU_TAG_WORD		0xFFU

/**
 * @brief The initial value of MXCSR field in the legacy region of an XSAVE area following STARTUP.
 */
#define XSAVE_STARTUP_MXCSR		0x1F80U

/**
 * @brief Structure to define stack frame
 *
 * The stack_frame is linked with the sequence of stack operation.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct stack_frame {
	uint64_t rdi;	/**< rdi register */
	uint64_t r15;	/**< r15 register */
	uint64_t r14;	/**< r14 register */
	uint64_t r13;	/**< r13 register */
	uint64_t r12;	/**< r12 register */
	uint64_t rbp;	/**< rbp register */
	uint64_t rbx;	/**< rbx register */
	uint64_t rflag;	/**< rflag register */
	uint64_t rip;	/**< rip register */
	uint64_t magic;	/**< the magic number which indicates the bottom of the stack */
};

/**
 * @brief This function is used to get the value of the target general purpose register.
 *
 * @param[in] vcpu A pointer which points to the target vcpu structure to get the guest general purpose registers.
 * @param[in] reg The index of the target general purpose register
 *
 * @return the value of the target gpreg
 *
 * @pre vcpu != NULL
 * @pre reg < NUM_GPRS
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vCPU is different among parallel invocation.
 */
uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing a pointer to the running context of the given vCPU, initialized
	 *	as &vcpu->arch.context.run_ctx.
	 */
	const struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** Return ctx->cpu_regs.longs[reg] which is the value of the target general-purpose register in ctx */
	return ctx->cpu_regs.longs[reg];
}

/**
 * @brief This function is used to set value of a specific guest general purpose register such as rax, rbx,
 *	etc with specified value.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the general purpose registers to.
 * @param[in] reg The index of the target general purpose register
 * @param[in] val The value which will be set to the target register
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre reg < NUM_GPRS
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vCPU is different among parallel invocation.
 */
void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing a pointer which points to vCPU running context, initialized
	 *  as &vcpu->arch.context.run_ctx.
	 */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** Set the target guest general purpose register to the input parameter val */
	ctx->cpu_regs.longs[reg] = val;
}

/**
 * @brief This function is used to get RIP of the target vCPU.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to get the guest RIP.
 *
 * @return RIP of the given vCPU
 *
 * @pre vcpu != NULL
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal
 *		to the vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vCPU is different among parallel invocation.
 */
uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing a pointer which points to vcpu running context, initialized
	 *	as &vcpu->arch.context.run_ctx.
	 */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** If calls to bitmap_test and bitmap_test_and_set_lock with CPU_REG_RIP and
	 *  &vcpu->reg_updated (for bitmap_test) or &vcpu->reg_cached (for the other)
	 *  being parameters both returns 0.
	 */
	if (!bitmap_test(CPU_REG_RIP, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_RIP, &vcpu->reg_cached)) {
		/** Call exec_vmread with VMX_GUEST_RIP being the parameter, in order to read
		 *  the value in the guest RIP from the current VMCS, and set ctx->rip to its return value.
		 */
		ctx->rip = exec_vmread(VMX_GUEST_RIP);
	}
	/** Return rip of the running context */
	return ctx->rip;
}

/**
 * @brief This function is used to set a value to guest RIP of a vCPU.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the guest RIP to.
 * @param[in] val The value which will be set to the guest RIP
 *
 * @return None
 *
 * @pre vcpu!= NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val)
{
	/** Set rip of the running context of the target vcpu to input parameter value */
	vcpu->arch.context.run_ctx.rip = val;
	/** Call bitmap_set_lock with the following parameters, in order to set the target bit CPU_REG_RIP to 1.
	 *  - CPU_REG_RIP: index to the bit in vcpu->reg_updated which stands for guest RIP
	 *  - &vcpu->reg_updated */
	bitmap_set_lock(CPU_REG_RIP, &vcpu->reg_updated);
}

/**
 * @brief This function is used to set value of updated RSP to guest vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the guest RSP to.
 * @param[in] val The value which will be set to the guest RSP
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing pointer which points to vcpu running context, initialized
	 *    as &vcpu->arch.context.run_ctx. */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** Set rsp of the vcpu running context to the inputparameter val */
	ctx->cpu_regs.regs.rsp = val;
	/** Call bitmap_set_lock with the following parameters, in order to set bit CPU_REG_RSP to 1.
	 *  - CPU_REG_RSP: index to the bit in vcpu->reg_updated which stands for guest RSP
	 *  - &vcpu->reg_updated: address of the integer where the bit is to be set */
	bitmap_set_lock(CPU_REG_RSP, &vcpu->reg_updated);
}

/**
 * @brief This function is used to set value of updated EFER to guest vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to get the guest IA32_EFER.
 *
 * @return IA32_EFER of the vCPU
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing pointer which points to vcpu running context, initialized
	 *	as &vcpu->arch.context.run_ctx. */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** If calls to bitmap_test and bitmap_test_and_set_lock with CPU_REG_EFER and
	 *  &vcpu->reg_updated (for bitmap_test) or &vcpu->reg_cached (for the other)
	 *  being parameters both returns 0.
	 */
	if (!bitmap_test(CPU_REG_EFER, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_EFER, &vcpu->reg_cached)) {
		/** Set IA32_EFER of the vcpu running context to exec_vmread64(VMX_GUEST_IA32_EFER_FULL) */
		ctx->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	}
	/** Return IA32_EFER of the vcpu running context */
	return ctx->ia32_efer;
}

/**
 * @brief This function is used to set guest IA32_EFER of current vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the guest IA32_EFER to.
 * @param[in] val The value which will be set to the guest IA32_EFER
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val)
{
	/** Set IA32_EFER of the running context of the target vcpu to input parameter val */
	vcpu->arch.context.run_ctx.ia32_efer = val;
	/** Call bitmap_set_lock() with the following parameters, in order to set bit CPU_REG_EFER to 1.
	 *  - CPU_REG_EFER: index to the bit in vcpu->reg_updated which stands for guest EFER
	 *  - &vcpu->reg_updated: address of the integer where the bit is to be set
	 */
	bitmap_set_lock(CPU_REG_EFER, &vcpu->reg_updated);
}

/**
 * @brief This function is used to get RFLAGS of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to get the guest RFLAGS.
 *
 * @return  vcpu->arch.context.run_ctx->rflags
 *
 * @pre vcpu != NULL
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal
 *		to the vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing a pointer which points to vcpu running context, initialized
	 *	as &vcpu->arch.context.run_ctx.
	 */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** If calls to bitmap_test and bitmap_test_and_set_lock with CPU_REG_RFLAGS and
	 *  &vcpu->reg_updated (for bitmap_test) or &vcpu->reg_cached (for the other)
	 *  being parameters both returns 0, and vcpu->launched is true.
	 */
	if (!bitmap_test(CPU_REG_RFLAGS, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_RFLAGS, &vcpu->reg_cached) && vcpu->launched) {
		/** Set rflags of the vcpu running context to exec_vmread(VMX_GUEST_RFLAGS) */
		ctx->rflags = exec_vmread(VMX_GUEST_RFLAGS);
	}
	/** Return rflags of the vcpu running context */
	return ctx->rflags;
}

/**
 * @brief This function is used to set rflags to the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the guest IA32_EFER to.
 * @param[in] val The value which will be set to the guest RFLAGS
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal
 *	to the vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_INIT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val)
{
	/** Set rflags if the vcpu running context to the input val of type unit64_t */
	vcpu->arch.context.run_ctx.rflags = val;
	/** Call bitmap_set_lock with the following parameters, in order to set bit CPU_REG_RFLAGS to 1.
	 *  - CPU_REG_RFLAGS: index to the bit in vcpu->reg_updated which stands for guest RFLAGS
	 *  - &vcpu->reg_updated: address of the integer where the bit is to be set
	 */
	bitmap_set_lock(CPU_REG_RFLAGS, &vcpu->reg_updated);
}

/**
 * @brief This function is used to get the guest msr from the vcpu structure.
 *
 * @param[in] vcpu A pointer to the vCPU whose MSR is to be read
 * @param[in] msr Address of the MSR to be read
 *
 * @return Value in the specified guest MSR of the given vCPU.
 *
 * @pre vcpu != NULL
 * @pre vmsr_get_guest_msr_index(msr) < NUM_GUEST_MSRS
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to
 *		the vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
uint64_t vcpu_get_guest_msr(const struct acrn_vcpu *vcpu, uint32_t msr)
{
	/** Declare the following local variables of type uint32_t.
	 *  - index representing sequence number of the target msr, initialized as vmsr_get_guest_msr_index(msr). */
	uint32_t index = vmsr_get_guest_msr_index(msr);
	/** Declare the following local variables of type uint64_t.
	 *  - val representing the msr result, initialized as OUL. */
	uint64_t val = 0UL;

	/** Set val to vcpu->arch.guest_msrs[index] */
	val = vcpu->arch.guest_msrs[index];

	/** Return the result of the target msr */
	return val;
}

/**
 * @brief This function is used to set specified value of msr to guest vcpu if the msr is valid.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the guest MSR to.
 * @param[in] msr The index of the guest MSRs.
 * @param[in] val The value which will be set to the guest MSR
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre vmsr_get_guest_msr_index(msr) < NUM_GUEST_MSRS
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to
 *		the vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 */
void vcpu_set_guest_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val)
{
	/** Declare the following local variables of type uint32_t.
	 *  - index representing sequence number of the target msr, initialized as vmsr_get_guest_msr_index(msr). */
	uint32_t index = vmsr_get_guest_msr_index(msr);

	/** Set vcpu->arch.guest_msrs[index] to the input parameter val */
	vcpu->arch.guest_msrs[index] = val;
}

/**
 * @brief This function is to determine the vCPU mode of operation according to the guest cs
 * attributes, IA32_EFER and cr0, and save the result to the given vCPU data structure.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure to set the cpu_mode.
 * @param[in] cs_attr The value of guesr CS attributes
 * @param[in] ia32_efer The value of guest IA32_EFER
 * @param[in] cr0 The value of guest CR0
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation
 *
 */
static void set_vcpu_mode(struct acrn_vcpu *vcpu, uint32_t cs_attr, uint64_t ia32_efer, uint64_t cr0)
{
	/** If (IA32_EFER & MSR_IA32_EFER_LMA_BIT) != 0UL */
	if ((ia32_efer & MSR_IA32_EFER_LMA_BIT) != 0UL) {
		/** If CS access right presents 64-bit mode active */
		if ((cs_attr & 0x2000U) != 0U) {
			/** Set cpu mode of the target vcpu to CPU_MODE_64BIT */
			vcpu->arch.cpu_mode = CPU_MODE_64BIT;
		} else {
			/** Set cpu mode of the target vcpu to CPU_MODE_COMPATIBILITY */
			vcpu->arch.cpu_mode = CPU_MODE_COMPATIBILITY;
		}
	/** If CR0.PE is 1 */
	} else if ((cr0 & CR0_PE) != 0UL) {
		/** Set cpu mode of the target vcpu to CPU_MODE_PROTECTED */
		vcpu->arch.cpu_mode = CPU_MODE_PROTECTED;
	} else {
		/** Set cpu mode of the target vcpu to CPU_MODE_REAL */
		vcpu->arch.cpu_mode = CPU_MODE_REAL;
	}
}

/**
 * @brief This function is used to initialize the xsave components of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure
 *
 * @return N/A
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
 *
 */
static void init_xsave(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct ext_context *.
	 *  - ectx representing pointer which points to extend context of the running vcpu, initialized
	 *	as &vcpu->arch.context.ext_ctx. */
	struct ext_context *ectx = &(vcpu->arch.context.ext_ctx);

	/* Get user state components */
	/** Set xcr0 of the extend content to XCR0_INIT */
	ectx->xcr0 = XCR0_INIT;

	/* Get supervisor state components */
	/** Set xss of the extend content to XSS_INIT */
	ectx->xss = XSS_INIT;

	/** Bitwise OR XCOMP_BV field of the extend context by XSAVE_COMPACTED_FORMAT
	 *  xsaves only support compacted format, so set it in xcomp_bv[63],
	 *  keep the reset area in header area as zero.
	 *  With this config, the first time a vcpu is scheduled in, it will
	 *  initialize all the xsave componets.
	 */
	/** Set ectx->xs_area.xsave_hdr.hdr.xcomp_bv to ectx->xs_area.xsave_hdr.hdr.xcomp_bv |
	 *  XSAVE_COMPACTED_FORMAT | XSAVE_X87_BV | XCR0_SSE. */
	ectx->xs_area.xsave_hdr.hdr.xcomp_bv |= XSAVE_COMPACTED_FORMAT | XSAVE_X87_BV | XCR0_SSE;
	/** Set ectx->xs_area.xsave_hdr.hdr.xstate_bv to ectx->xs_area.xsave_hdr.hdr.xstate_bv |
	 *  (XSAVE_X87_BV | XCR0_SSE). */
	ectx->xs_area.xsave_hdr.hdr.xstate_bv |= (XSAVE_X87_BV | XCR0_SSE);

	/** Set ectx->xs_area.legacy_region.fcw to XSAVE_STARTUP_FPU_CONTROL_WORD. */
	ectx->xs_area.legacy_region.fcw = XSAVE_STARTUP_FPU_CONTROL_WORD;
	/** Set ectx->xs_area.legacy_region.ftw to XSAVE_STARTUP_FPU_TAG_WORD. */
	ectx->xs_area.legacy_region.ftw = XSAVE_STARTUP_FPU_TAG_WORD;
	/** Set ectx->xs_area.legacy_region.mxcsr to XSAVE_STARTUP_MXCSR. */
	ectx->xs_area.legacy_region.mxcsr = XSAVE_STARTUP_MXCSR;
}

/**
 * @brief This function set the value of GDTR, IDTR, LDTR, TR, CS, IA32_EFER, RFLAGS, RIP, RSP, CR0,
 *        CR3 and CR4 registers, and set mode of the vCPU.
 *
 * @param[inout] vcpu A pointer which points to a target vcpu structure whose contexts are set.
 * @param[in] vcpu_regs A pointer which points to a target acrn_vcpu_regs structure whose registers are set to
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre vmsr_get_guest_msr_index(msr) < NUM_GUEST_MSRS
 * @pre the host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the
 *	vmcs pointer of the current pcpu.
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs)
{
	/** Declare the following local variable of type struct ext_context *.
	 *  - ectx representing pointer which points to the extend context of vcpu */
	struct ext_context *ectx;
	/** Declare the following local variable of type struct run_context *.
	 *  - ctx representing pointer which points to the running context of vcpu */
	struct run_context *ctx;
	/** Declare the following local variables of type uint16_t *.
	 *  - sel representing a pointer to a segment selector of the target vcpu,
	 *  initialized as &vcpu_regs->cs_sel.
	 */
	uint16_t *sel = &(vcpu_regs->cs_sel);
	/** Declare the following local variables of type struct segment_sel *.
	 *  - seg representing a pointer which points to the segment register */
	struct segment_sel *seg;
	/** Declare the following local variables of type uint32_t.
	 *  - limit representing the hidden part limit.
	 *  - attr representing the hidden part attributes. */
	uint32_t limit, attr;

	/** Set ectx to &(vcpu->arch.context.ext_ctx) */
	ectx = &(vcpu->arch.context.ext_ctx);
	/** Set ctx to &(vcpu->arch.context.run_ctx) */
	ctx = &(vcpu->arch.context.run_ctx);

	/** Set ia32_kernel_gs_base of the extend context to 0x00000000U */
	ectx->ia32_kernel_gs_base = 0x00000000U;

	/** If vcpu is in protected mode */
	if ((vcpu_regs->cr0 & CR0_PE) != 0UL) {
		/** Set attr to PROTECTED_MODE_DATA_SEG_AR */
		attr = PROTECTED_MODE_DATA_SEG_AR;
		/** Set limit to PROTECTED_MODE_SEG_LIMIT */
		limit = PROTECTED_MODE_SEG_LIMIT;
	} else {
		/** Set attr to REAL_MODE_DATA_SEG_AR */
		attr = REAL_MODE_DATA_SEG_AR;
		/** Set attr to REAL_MODE_SEG_LIMIT */
		limit = REAL_MODE_SEG_LIMIT;
	}

	/** For each segment register ranging from cs to gs [with a step of length of whole segment register] */
	for (seg = &(ectx->cs); seg <= &(ectx->gs); seg++) {
		/** Set base address of the segment register to 0 */
		seg->base = 0UL;
		/** Set limit of the segment register to limit */
		seg->limit = limit;
		/** Set attributes of the segment register to attr */
		seg->attr = attr;
		/** Set selector of the segment register to *sel */
		seg->selector = *sel;
		/** Increment segment selector by 1 selector */
		sel++;
	}

	/** Set segment register attributes of the extend context to vcpu_regs->cs_ar */
	ectx->cs.attr = vcpu_regs->cs_ar;
	/** Set segment register base address of the extend context to vcpu_regs->cs_base */
	ectx->cs.base = vcpu_regs->cs_base;
	/** Set segment register limit of the extend context to vcpu_regs->cs_limit */
	ectx->cs.limit = vcpu_regs->cs_limit;

	/** Set GDTR base address of the extend context to vcpu_regs->gdt.base */
	ectx->gdtr.base = vcpu_regs->gdt.base;
	/** Set GDTR limit of the extend context to vcpu_regs->gdt.limit */
	ectx->gdtr.limit = vcpu_regs->gdt.limit;

	/** Set IDTR base address of the extend context to vcpu_regs->idt.base */
	ectx->idtr.base = vcpu_regs->idt.base;
	/** Set IDTR limit of the extend context to vcpu_regs->idt.limit */
	ectx->idtr.limit = vcpu_regs->idt.limit;

	/** Set IDTR segment selector of the extend context to vcpu_regs->ldt_sel */
	ectx->ldtr.selector = vcpu_regs->ldt_sel;
	/** Set TR segment selector of the extend context to vcpu_regs->tr_sel */
	ectx->tr.selector = vcpu_regs->tr_sel;

	/** Set IDTR base address of the extend context to 0 */
	ectx->ldtr.base = 0UL;
	/** Set TR base address of the extend context to 0 */
	ectx->tr.base = 0UL;
	/** Set LDTR limit of the extend context to ffffh */
	ectx->ldtr.limit = 0xFFFFU;
	/** Set TR limit of the extend context to ffffh */
	ectx->tr.limit = 0xFFFFU;
	/** Set LDTR attributes of the extend context to LDTR_AR */
	ectx->ldtr.attr = LDTR_AR;
	/** Set TR attributes of the extend context to TR_AR */
	ectx->tr.attr = TR_AR;
	/** Call memcpy_s() with the following parameters, in order to copy the cpu_regs of extend context
	 *  to the target vcpu.
	 *  - &(ctx->cpu_regs): address of cpu_regs in the extend context
	 *  - sizeof(struct acrn_gp_regs): size of the struct acrn_gp_regs
	 *  - &(vcpu_regs->gprs): address of the first byte of struct vcpu_regs->gprs
	 *  - sizeof(struct acrn_gp_regs): size of the struct acrn_gp_regs */
	(void)memcpy_s((void *)&(ctx->cpu_regs), sizeof(struct acrn_gp_regs), (void *)&(vcpu_regs->gprs),
		sizeof(struct acrn_gp_regs));
	/** Call vcpu_set_rip() with the following parameters, in order to set the rip to the target vcpu.
	 *  - vcpu: the target vcpu
	 *  - vcpu_regs->rip: the rip to set */
	vcpu_set_rip(vcpu, vcpu_regs->rip);
	/** Call vcpu_set_efer() with the following parameters, in order to set the IA32_EFER to the target vcpu.
	 *  - vcpu: the target vcpu
	 *  - vcpu_regs->ia32_efer: the IA32_EFER to set */
	vcpu_set_efer(vcpu, vcpu_regs->ia32_efer);
	/** Call vcpu_set_rsp() with the following parameters, in order to set the rsp to the target vcpu.
	 *  - vcpu: the target vcpu
	 *  - vcpu_regs->gprs.rsp: the rsp to set */
	vcpu_set_rsp(vcpu, vcpu_regs->gprs.rsp);

	/** Call vcpu_set_rflags() with the following parameters, in order to
	 *  set the rflags to 02h, which is the initial value of EFLAGS register.
	 *  - vcpu: the target vcpu
	 *  - 0x02UL: the default value of RFLAGS register following startup or INIT
	 */
	vcpu_set_rflags(vcpu, 0x02UL);

	/** Set ctx->cr0 to vcpu_regs->cr0. */
	ctx->cr0 = vcpu_regs->cr0;
	/** Set ectx->cr3 to vcpu_regs->cr3. */
	ectx->cr3 = vcpu_regs->cr3;
	/** Set ctx->cr4 to vcpu_regs->cr4. */
	ctx->cr4 = vcpu_regs->cr4;

	/** Call set_vcpu_mode() with the following parameters, in order to set the relevant vcpu mode.
	 *  - vcpu: the target vcpu
	 *  - vcpu_regs->cs_ar: attributes of the cs register
	 *  - vcpu_regs->ia32_efer: IA32_EFER of the vcpu
	 *  - vcpu_regs->cr0: CR0 of the vcpu */
	set_vcpu_mode(vcpu, vcpu_regs->cs_ar, vcpu_regs->ia32_efer, vcpu_regs->cr0);
}

/**
 * @brief This structure realmode_init_vregs is used to store initial state of vCPU registers for real mode.
 */
static struct acrn_vcpu_regs realmode_init_vregs = {
	.gdt = {
		/**
		 * @brief The initial value for limit of guest GDTR in real mode is FFFFh.
		 */
		.limit = 0xFFFFU,
		/**
		 * @brief The initial value for base address of guest GDTR in real mode is 0.
		 */
		.base = 0UL,
	},
	.idt = {
		/**
		 * @brief The initial value for limit of guest IDTR in real mode is FFFFh.
		 */
		.limit = 0xFFFFU,
		/**
		 * @brief The initial value for base address of guest IDTR in real mode is 0.
		 */
		.base = 0UL,
	},
	/**
	 * @brief The initial value for attributes of guest CS in real mode is REAL_MODE_CODE_SEG_AR.
	 */
	.cs_ar = REAL_MODE_CODE_SEG_AR,
	/**
	 * @brief The initial value for selector of guest CS in real mode is REAL_MODE_BSP_INIT_CODE_SEL.
	 */
	.cs_sel = REAL_MODE_BSP_INIT_CODE_SEL,
	/**
	 * @brief The initial value for base address of guest CS in real mode is FFFF0000h.
	 */
	.cs_base = 0xFFFF0000UL,
	/**
	 * @brief The initial value for limit of guest CS in real mode is FFFFh.
	 */
	.cs_limit = 0xFFFFU,
	/**
	 * @brief The initial value for guest RIP in real mode is FFF0h.
	 */
	.rip = 0xFFF0UL,
	/**
	 * @brief The initial value for guest CR0 in real mode is CR0 to CR0_ET | CR0_NE | CR0_CD | CR0_NW.
	 */
	.cr0 = CR0_ET | CR0_NE | CR0_CD | CR0_NW,
	/**
	 * @brief The initial value for guest CR3 in real mode is 0.
	 */
	.cr3 = 0UL,
	/**
	 * @brief The initial value for attributes of guest CR4 in real mode is 0.
	 */
	.cr4 = 0UL,
	.gprs = {
		/**
		 * @brief The initial value for RDX in real mode is 00080600h.
		 */
		.rdx = 0x00080600UL,
	}
};

/**
 * @brief This array presents the host virtual start address of internal array.
 *
 * Its size is 32 bytes, and stores 4 consecutive 8-byte values with zero
 * extension (0H,0H,00CF9B000000FFFFH,00CF93000000FFFFH) in little endian order;
 *
 */
static uint64_t init_vgdt[] = {
	0x0UL, 0x0UL, 0x00CF9B000000FFFFUL,
	0x00CF93000000FFFFUL,
};

/**
 * @brief This structure protect_mode_init_vregs is used to store initial state of vCPU registers
 *        for protected mode.
 */
static struct acrn_vcpu_regs protect_mode_init_vregs = {  /* initial values of vCPU registers for protected mode */
	/**
	 * @brief The initial value for attributes of guest CS in protected mode is PROTECTED_MODE_CODE_SEG_AR.
	 */
	.cs_ar = PROTECTED_MODE_CODE_SEG_AR,
	/**
	 * @brief The initial value for limit of guest CS in protected mode is PROTECTED_MODE_SEG_LIMIT.
	 */
	.cs_limit = PROTECTED_MODE_SEG_LIMIT,
	/**
	 * @brief The initial value for selector of guest CS in protected mode is 10h.
	 */
	.cs_sel = 0x10U,
	/**
	 * @brief The initial value for guest CR0 in protected mode is CR0_ET | CR0_NE | CR0_PE | CR0_CD | CR0_NW.
	 */
	.cr0 = CR0_ET | CR0_NE | CR0_PE | CR0_NW | CR0_CD,
	/**
	 * @brief The initial value for selector of guest DS in protected mode is 18h.
	 */
	.ds_sel = 0x18U,
	/**
	 * @brief The initial value for selector of guest SS in protected mode is 18h.
	 */
	.ss_sel = 0x18U,
	/**
	 * @brief The initial value for selector of guest ES in protected mode is 18h.
	 */
	.es_sel = 0x18U
};

/**
 * @brief This function is used to reset vcpu registers of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure whose registers are reset.
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
 * @threadsafety When \a vcpu is different among parallel invocation.
 *
 */
void reset_vcpu_regs(struct acrn_vcpu *vcpu)
{
	/** Call set_vcpu_regs() with the following parameters, in order to set the
	 *  vcpu registers to the relevant states.
	 *  - vcpu: the target vcpu
	 *  - &realmode_init_vregs: the address of the initial register state to set */
	set_vcpu_regs(vcpu, &realmode_init_vregs);
}

/**
 * @brief This function is used to initialize the registers of protected mode of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure whose registers are initialized.
 * @param[in] vgdt_base_gpa The value of the guest GDTR base address.
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
 * @threadsafety When \a vcpu is different among parallel invocation.
 *
 */
void init_vcpu_protect_mode_regs(struct acrn_vcpu *vcpu, uint64_t vgdt_base_gpa)
{
	/** Declare the following local variables of type struct acrn_vcpu_regs.
	 *  - vcpu_regs representing the struct which stores vcpu registers information */
	struct acrn_vcpu_regs vcpu_regs;

	/** Call memcpy_s() with the following parameters, in order to copy the protect mode init register states
	 *  to the target vcpu.
	 *  - &vcpu_regs: address of the vcpu_regs struct
	 *  - sizeof(struct acrn_vcpu_regs): size of the struct acrn_vcpu_regs
	 *  - &protect_mode_init_vregs: address of the first byte of struct protect_mode_init_vregs
	 *  - sizeof(struct acrn_vcpu_regs): size of the struct acrn_vcpu_regs */
	(void)memcpy_s((void *)&vcpu_regs, sizeof(struct acrn_vcpu_regs), (void *)&protect_mode_init_vregs,
		sizeof(struct acrn_vcpu_regs));

	/** Set GDTR base address of the vcpu to vgdt_base_gpa */
	vcpu_regs.gdt.base = vgdt_base_gpa;
	/** Set GDTR limit of the vcpu to (sizeof(init_vgdt) - 1) */
	vcpu_regs.gdt.limit = sizeof(init_vgdt) - 1U;
	/** Set IDTR limit of the vcpu to 0XffffU */
	vcpu_regs.idt.limit = 0xffffU;
	/** Call copy_to_gpa() with the following parameters, in order to copy data in the host virtual memory region
	 *  [&init_vgdt, &init_vgdt + sizeof(init_vgdt)) into the guest physical memory region [&init_vgdt, &init_vgdt
	 *  + sizeof(init_vgdt)).
	 *  - vcpu->vm: pointer that points to VM data structure
	 *  - &init_vgdt: start virtual linear address of HV memory region which data is stored in
	 *  - vgdt_base_gpa: start guest physical address of guest memory region which data will be copied into
	 *  - sizeof(init_vgdt): size of memory which date will be copied */
	copy_to_gpa(vcpu->vm, &init_vgdt, vgdt_base_gpa, sizeof(init_vgdt));

	/** Call set_vcpu_regs() with the following parameters, in order to set the registers of the target vcpu.
	 *  - vcpu: target vcpu to set
	 *  - &vcpu_regs: address of the vcpu_reg structure */
	set_vcpu_regs(vcpu, &vcpu_regs);
}

/**
 * @brief This function is used to to set the address of the first instruction \a vcpu will
 *        execute in real-address mode once it is launched.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure
 * @param[in] entry The guest physical address where the AP will start fetching instructions.
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 *
 */
void set_vcpu_startup_entry(struct acrn_vcpu *vcpu, uint64_t entry)
{
	/** Declare the following local variables of type struct ext_context *.
	 *  - ectx representing extend context struct. */
	struct ext_context *ectx;

	/** Set extend context struct to &(vcpu->arch.context.ext_ctx) */
	ectx = &(vcpu->arch.context.ext_ctx);
	/** Set cs selector of the extend context to (uint16_t)((entry >> 4U) & 0xFFFFU) */
	ectx->cs.selector = (uint16_t)((entry >> 4U) & 0xFFFFU);
	/** Set cs base address of the extend context to (uint64_t)ectx->cs.selector << 4U */
	ectx->cs.base = (uint64_t)ectx->cs.selector << 4U;

	/** Call vcpu_set_rip() with the following parameters, in order to set the rip to 0.
	 *  - vcpu: the target vcpu
	 *  - 0UL: the rip value to set */
	vcpu_set_rip(vcpu, 0UL);
}

/**
 * @brief This function is used to create related states of the target vcpu.
 *
 * @param[in] vm A pointer which points to the target vm structure which is bound with the guest vCPU
 * @param[in] pcpu_id The id of the pcpu
 * @param[out] rtn_vcpu_handle A pointer which points to the pointer points to vcpu structure.
 *
 * @return N/A
 *
 * @pre vm != NULL && rtn_vcpu_handle != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 *
 */
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing the pointer to the next fresh vCPU data
	 *         structure of \a vm. */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variables of type uint16_t.
	 *  - vcpu_id representing the id of the vcpu. */
	uint16_t vcpu_id;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the return value of the function. */
	int32_t ret;

	/** Logging the following information with a log level of LOG_INFO.
	 *  - "Creating VCPU working on PCPU"
	 *  - pcpu_id: the target pcpu id*/
	pr_info("Creating VCPU working on PCPU%hu", pcpu_id);

	/** Set id of the vcpu to vm->hw.created_vcpus */
	vcpu_id = vm->hw.created_vcpus;
	/** If the id of the vcpu is less than the MAX_VCPUS_PER_VM*/
	if (vcpu_id < MAX_VCPUS_PER_VM) {
		/* Allocate memory for VCPU */
		/** Set vcpu to &(vm->hw.vcpu_array[vcpu_id]) */
		vcpu = &(vm->hw.vcpu_array[vcpu_id]);
		/** Call memset() with the following parameters, in order to set
		 *  sizeof(struct acrn_vcpu) bytes starting from \a vcpu to 0.
		 *  - vcpu: The address of the memory block to fill
		 *  - 0: The value to be set to each byte of the specified memory block.
		 *  - sizeof(struct acrn_vcpu): The number of bytes to be set */
		(void)memset((void *)vcpu, 0U, sizeof(struct acrn_vcpu));

		/* Initialize CPU ID for this VCPU */
		/** Set id of the target vcpu to vcpu_id */
		vcpu->vcpu_id = vcpu_id;
		/** Set ever_run_vcpu of the per_cpu_data[(pcpu_id)] to vcpu */
		per_cpu(ever_run_vcpu, pcpu_id) = vcpu;

		/** Set vm of the vcpu to the input parameter vm */
		vcpu->vm = vm;

		/** Logging the following information with a log level of LOG_INFO.
		 *  - vcpu->vm->vm_id: created vm id information
		 *  - vcpu->vcpu_id: created vcpu id information
		 *  - "PRIMARY" when is_vcpu_bsp(vcpu) is true, else "SECONDARY" : Role information*/
		pr_info("Create VM%d-VCPU%d, Role: %s", vcpu->vm->vm_id, vcpu->vcpu_id,
			is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

		/** Set vpid of the vcpu->arch to 1U + (vm->vm_id * MAX_VCPUS_PER_VM) + vcpu->vcpu_id */
		vcpu->arch.vpid = 1U + (vm->vm_id * MAX_VCPUS_PER_VM) + vcpu->vcpu_id;

		/** Set exception information of the vcpu to VECTOR_INVALID */
		vcpu->arch.exception_info.exception = VECTOR_INVALID;

		/** Set vcpu->arch.vcpu_powerup to false; */
		vcpu->arch.vcpu_powerup = false;

		/* Create per vcpu vlapic */
		/** Call vlapic_create() with the following parameters, in order to initialize vlapic related states.
		 *  - vcpu: the target vcpu
		 */
		vlapic_create(vcpu);

		/* Populate the return handle */
		/** Set *rtn_vcpu_handle to vcpu */
		*rtn_vcpu_handle = vcpu;

		/** Set launched state of vcpu to false */
		vcpu->launched = false;
		/** Set running state of vcpu to false */
		vcpu->running = false;
		/** Set nr_sipi of the vcpu->arch to 0 */
		vcpu->arch.nr_sipi = 0U;
		/** Set state of the vcpu to VCPU_INIT */
		vcpu->state = VCPU_INIT;

		/** Call init_xsave() with the following parameters, in order to initialize
		 *  the xsave area of the target vcpu.
		 *  - vcpu : the vcpu to set */
		init_xsave(vcpu);
		/** Call reset_vcpu_regs() with the following parameters, in order to
		 *  reset registers of the target vcpu.
		 *  - vcpu : the vcpu to reset */
		reset_vcpu_regs(vcpu);
		/** Call memset() with the following parameters, in order to set
		 *  sizeof(struct io_request) bytes starting from &vcpu->req to 0.
		 *  - &vcpu->req: The address of the memory block to fill
		 *  - 0: The value to be set to each byte of the specified memory block.
		 *  - sizeof(struct io_request): The number of bytes to be set */
		(void)memset((void *)&vcpu->req, 0U, sizeof(struct io_request));
		/** Increment created_vcpus number of the vm->hw by 1 */
		vm->hw.created_vcpus++;
		/** Set return value to 0 */
		ret = 0;
	} else {
		/** Logging the following information with a log level of LOG_ERROR.
		 *  -  __func__: current running function name
		 *  - vcpu id is invalid!: error information */
		pr_err("%s, vcpu id is invalid!\n", __func__);
		/** Set return value to -EINVAL */
		ret = -EINVAL;
	}

	/** Return ret */
	return ret;
}

/**
 * @brief This function is used to run the vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be run
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 *
 */
int32_t run_vcpu(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - instlen representing the instruction length
	 *  - cs_attr representing the attributes of cs */
	uint32_t instlen, cs_attr;
	/** Declare the following local variables of type uint64_t.
	 *  - rip representing rip of the vcpu.
	 *  - ia32_efer representing IA32_EFER of the vcpu.
	 *  - cr0 representing cr0 of the vcpu. */
	uint64_t rip, ia32_efer, cr0;
	/** Declare the following local variables of type struct run_context *.
	 *  - ctx representing pointer which points to the running context, initialized
	 *  as &(vcpu->arch.context.run_ctx).
	 */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;
	/** Declare the following local variables of type int32_t.
	 *  - status representing vmx running status, initialized as zero. */
	int32_t status = 0;

	/** If calls to bitmap_test_and_set_lock with CPU_REG_RIP and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_RIP, &vcpu->reg_updated)) {
		/** Call exec_vmwrite() with the following parameters, in order to write rip to vmcs field.
		 *  - VMX_GUEST_RIP: address of GUEST RIP in VMCS field
		 *  - ctx->rip:  rip in the context*/
		exec_vmwrite(VMX_GUEST_RIP, ctx->rip);
	}
	/** If calls to bitmap_test_and_set_lock with CPU_REG_RSP and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_RSP, &vcpu->reg_updated)) {
		/** Call exec_vmwrite() with the following parameters, in order to write rsp to vmcs field.
		 *  - VMX_GUEST_RSP: address of GUEST RSP in VMCS field
		 *  - ctx->cpu_regs.regs.rsp:  rsp in the context*/
		exec_vmwrite(VMX_GUEST_RSP, ctx->cpu_regs.regs.rsp);
	}
	/** If calls to bitmap_test_and_set_lock with CPU_REG_EFER and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_EFER, &vcpu->reg_updated)) {
		/** Call exec_vmwrite() with the following parameters, in order to write rsp to vmcs field .
		 *  - VMX_GUEST_IA32_EFER_FULL: address of GUEST EFER in VMCS field
		 *  - ctx->ia32_efer:  IA32_EFER in the context*/
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, ctx->ia32_efer);
	}
	/** If calls to bitmap_test_and_set_lock with CPU_REG_RFLAGS and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_RFLAGS, &vcpu->reg_updated)) {
		/** Call exec_vmwrite() with the following parameters, in order to write rsp to vmcs field .
		 *  - VMX_GUEST_RFLAGS: address of GUEST RFLAGS in VMCS field
		 *  - ctx->rflags:  rflags in the context*/
		exec_vmwrite(VMX_GUEST_RFLAGS, ctx->rflags);
	}

	/** If calls to bitmap_test_and_set_lock with CPU_REG_CR0 and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_CR0, &vcpu->reg_updated)) {
		/** Call vcpu_set_cr0() with the following parameters, in order to set cr0 to the vcpu.
		 *  - vcpu: the target vcpu to set
		 *  - ctx->cr0:  cr0 to set
		 *  - false */
		vcpu_set_cr0(vcpu, ctx->cr0, false);
	}

	/** If calls to bitmap_test_and_set_lock with CPU_REG_CR4 and &vcpu->reg_updated
	 *  being parameters returns true.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_CR4, &vcpu->reg_updated)) {
		/** Call vcpu_set_cr4() with the following parameters, in order to set cr4 to the vcpu.
		 *  - vcpu: the target vcpu to set
		 *  - ctx->cr4:  cr4 to set
		 *  - false */
		vcpu_set_cr4(vcpu, ctx->cr4, false);
	}

	/** Set vcpu->arch.vcpu_powerup to true; */
	vcpu->arch.vcpu_powerup = true;

	/** If this VCPU is not already launched */
	if (!vcpu->launched) {
		/** Logging the following information with a log level of LOG_INFO.
		 *  - vcpu->vm->vm_id: vm id
		 *  - vcpu->vcpu_id: vcpu id */
		pr_info("VM %d Starting VCPU %hu", vcpu->vm->vm_id, vcpu->vcpu_id);

		/** Call exec_vmwrite16 with the following parameters, in order to set vpid to vmcs field.
		 *  - VMX_VPID: address of VPID in VMCS field
		 *  - vcpu->arch.vpid:  vpid in the context
		 */
		exec_vmwrite16(VMX_VPID, vcpu->arch.vpid);

		/** Call flush_vpid_global(), in order to invalidate all mappings (linear mappings,
		 *  guest-physical mappings and combined mappings) in the TLBs and paging-structure
		 *  caches that are tagged with all VPIDs.
		 */
		flush_vpid_global();

		/** Set launched status of the vcpu to true */
		vcpu->launched = true;

		/** Call cpu_l1d_flush(), in order to Flush L1 data cache.
		 */
		cpu_l1d_flush();

		/** Call cpu_internal_buffers_clear() in order to clear CPU internal buffers.
		 */
		cpu_internal_buffers_clear();

		/** Call vmx_vmrun with ctx and VM_LAUNCH being the parameters, in order to
		 *  save running context of the vcpu, execute a VM launch, and then set
		 *  status to its return value.
		 */
		status = vmx_vmrun(ctx, VM_LAUNCH);

		/** If the VMX running status is 0 */
		if (status == 0) {
			/** If the return value of is_vcpu_bsp() with vcpu as parameter is true*/
			if (is_vcpu_bsp(vcpu)) {
				/** Logging the following information with a log level of LOG_INFO.
				 *  - vcpu->vm->vm_id: vm id
				 *  - vcpu->vcpu_id: vcpu id */
				pr_info("VM %d VCPU %hu successfully launched", vcpu->vm->vm_id, vcpu->vcpu_id);
			}
		}
	} else {
		/* This VCPU was already launched, check if the last guest
		 * instruction needs to be repeated and resume VCPU accordingly
		 */
		/** Set instruction length to vcpu->arch.inst_len */
		instlen = vcpu->arch.inst_len;
		/** Call vcpu_get_rip with vcpu being the parameter, in order to
		 *  get current guest RIP, and set rip to its return value.
		 */
		rip = vcpu_get_rip(vcpu);
		/** Call exec_vmwrite() with the following parameters, in order to write rip
		 *  value to VMCS field.
		 *  - VMX_GUEST_RIP: address of GUEST RIP in VMCS field
		 *  - ((rip + (uint64_t)instlen) & 0xFFFFFFFFFFFFFFFFUL)):  guest address
		 *  of the next instruction to be executed.
		 */
		exec_vmwrite(VMX_GUEST_RIP, ((rip + (uint64_t)instlen) & 0xFFFFFFFFFFFFFFFFUL));

		/** Call cpu_l1d_flush(), in order to Flush L1 data cache.
		 */
		cpu_l1d_flush();

		/* Mitigation for MDS vulnerability, overwrite CPU internal buffers */
		/** Call cpu_internal_buffers_clear(), in order to
		 *  clear CPU internal buffers.
		 */
		cpu_internal_buffers_clear();

		/** Call vmx_vmrun with ctx and VM_RESUME being the parameters, in order to
		 *  save running context of the vcpu, execute a VM resume, and then set
		 *  status to its return value.
		 */
		status = vmx_vmrun(ctx, VM_RESUME);
	}

	/** Set cached flag of the vcpu to zero */
	vcpu->reg_cached = 0UL;

	/** Set cs register attributes to exec_vmread32(VMX_GUEST_CS_ATTR) */
	cs_attr = exec_vmread32(VMX_GUEST_CS_ATTR);
	/** Set ia32_efer to vcpu_get_efer(vcpu) */
	ia32_efer = vcpu_get_efer(vcpu);
	/** Set cr0 to vcpu_get_cr0(vcpu) */
	cr0 = vcpu_get_cr0(vcpu);
	/** Call set_vcpu_mode() with the following parameters, in order to set the relevant vcpu mode.
	 *  - vcpu: the target vcpu
	 *  - cs_attr: attributes of the cs register
	 *  - ia32_efer: IA32_EFER of the vcpu
	 *  - cr0: CR0 of the vcpu
	 */
	set_vcpu_mode(vcpu, cs_attr, ia32_efer, cr0);

	/** Set inst_len of vcpu->arch to exec_vmread32(VMX_EXIT_INSTR_LEN) */
	vcpu->arch.inst_len = exec_vmread32(VMX_EXIT_INSTR_LEN);

	/** Set rsp of ctx->cpu_regs.regs to exec_vmread32(VMX_GUEST_RSP) */
	ctx->cpu_regs.regs.rsp = exec_vmread(VMX_GUEST_RSP);

	/** Set exit_reason of vcpu->arch to exec_vmread32(VMX_EXIT_REASON) */
	vcpu->arch.exit_reason = exec_vmread32(VMX_EXIT_REASON);

	/** If status is not 0 */
	if (status != 0) {
		/** If vm entry failed */
		if ((vcpu->arch.exit_reason & VMX_VMENTRY_FAIL) != 0U) {
			/** Logging the following information with a log level of LOG_FATAL.
			 *  - vcpu->arch.exit_reason: vm entry fail reason */
			pr_fatal("vmentry fail reason=%lx", vcpu->arch.exit_reason);
		} else {
			/** Logging the following information with a log level of LOG_FATAL.
			 *  - exec_vmread32(VMX_INSTR_ERROR): vmexit fail err instruction */
			pr_fatal("vmexit fail err_inst=%x", exec_vmread32(VMX_INSTR_ERROR));
		}


		/** Assert that status is not equal to 0. */
		ASSERT(status == 0, "vm fail");
	}

	/** Return status */
	return status;
}

/**
 * @brief This function is used to offline the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be offline
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation
 *
 */
void offline_vcpu(struct acrn_vcpu *vcpu)
{
	/** Set per_cpu_data[pcpuid_from_vcpu(vcpu)].ever_run_vcpu to NULL */
	per_cpu(ever_run_vcpu, pcpuid_from_vcpu(vcpu)) = NULL;
	/** Set state of the vcpu to VCPU_OFFLINE */
	vcpu->state = VCPU_OFFLINE;
}

/**
 * @brief This function is used to notify a vCPU of pending requests that it must handle immediately.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be notified
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
void kick_vcpu(const struct acrn_vcpu *vcpu)
{
	/** Call kick_thread with the following parameters, in order to kick the thread of the vcpu.
	 *  - &(vcpu->thread_obj): thread of the vcpu */
	kick_thread(&vcpu->thread_obj);
}

/**
 * @brief This function is used to build the stack frame of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be run
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre (&vcpu->stack[CONFIG_STACK_SIZE] & (CPU_STACK_ALIGN - 1UL)) == 0
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation
 *
 */
static uint64_t build_stack_frame(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint64_t.
	 *  - stacktop representing the top address of the stack, initialized as
	 *  (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE]
	 */
	uint64_t stacktop = (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE];
	/** Declare the following local variables of type struct stack_frame *.
	 *  - frame representing the stack frame. */
	struct stack_frame *frame;
	/** Declare the following local variables of type uint64_t *.
	 *  - ret representing pointer that points to the return value. */
	uint64_t *ret;

	/** Set frame to (struct stack_frame *)stacktop */
	frame = (struct stack_frame *)stacktop;
	/** Decrement frame by 1 */
	frame -= 1;

	/** Set magic number of the frame stack to SP_BOTTOM_MAGIC */
	frame->magic = SP_BOTTOM_MAGIC;
	/** Set rip of the frame to (uint64_t)vcpu->thread_obj.thread_entry. */
	frame->rip = (uint64_t)vcpu->thread_obj.thread_entry; /*return address*/
	/** Set rflag of the frame to zero. */
	frame->rflag = 0UL;
	/** Set rbx of the frame to zero. */
	frame->rbx = 0UL;
	/** Set rbp of the frame to zero. */
	frame->rbp = 0UL;
	/** Set r12 of the frame to zero. */
	frame->r12 = 0UL;
	/** Set r13 of the frame to zero. */
	frame->r13 = 0UL;
	/** Set r14 of the frame to zero. */
	frame->r14 = 0UL;
	/** Set r15 of the frame to zero. */
	frame->r15 = 0UL;
	/** Set rdi of the frame to address of vcpu->thread_obj. */
	frame->rdi = (uint64_t)&vcpu->thread_obj;

	/** Set ret to address of frame->rdi. */
	ret = &frame->rdi;

	/** Return (uint64_t)ret */
	return (uint64_t)ret;
}

/**
 * @brief This function is used to reset the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be reset
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation
 *
 */
void reset_vcpu(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct acrn_vlapic *.
	 *  - vlapic representing pointer that points to vlapic of the vcpu. */
	struct acrn_vlapic *vlapic;

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - vcpu->vcpu_id: vcpu id to reset */
	pr_dbg("vcpu%hu reset", vcpu->vcpu_id);

	/** Set state of the vcpu to VCPU_INIT */
	vcpu->state = VCPU_INIT;

	/** Set launched state of vcpu to false */
	vcpu->launched = false;
	/** Set running state of vcpu to false */
	vcpu->running = false;
	/** Set nr_sipi state of vcpu->arch to false */
	vcpu->arch.nr_sipi = 0U;

	/** Set exception information of vcpu->arch to VECTOR_INVALID */
	vcpu->arch.exception_info.exception = VECTOR_INVALID;

	/** Call memset() with the following parameters, in order to set
	 *  sizeof(struct run_context) bytes starting from &vcpu->arch.context to 0.
	 *  - (void *)&vcpu->arch.contexts[i]: The address of the memory block to fill
	 *  - 0: The value to be set to each byte of the specified memory block.
	 *  - sizeof(struct run_context): The number of bytes to be set */
	(void)memset((void *)(&vcpu->arch.context), 0U, sizeof(struct run_context));

	/** Call vcpu_vlapic() with vcpu as parameter in order to get address of the
	 *  vlapic structure associated with the vcpu and set vlapic to its return value. */
	vlapic = vcpu_vlapic(vcpu);
	/** Call vlapic_reset() with the following parameters, in order to reset the target vlapic.
	 *  - vlapic: the target vlapic to reset */
	vlapic_reset(vlapic);

	/** Call vcpu_make_request with following parameters in order to
	 *  initialize the LAPIC of the physical CPU corresponding to the vCPU.
	 *  - vcpu
	 *  - ACRN_REQUEST_LAPIC_RESET
	 */
	vcpu_make_request(vcpu, ACRN_REQUEST_LAPIC_RESET);

	/** Call reset_vcpu_regs() with the following parameters, in order to reset the vcpu registers.
	 *  -vcpu: the target vcpu to reset */
	reset_vcpu_regs(vcpu);
}


/**
 * @brief This function is used to pause the target vcpu.
 *
 * Change a vCPU state to VCPU_ZOMBIE, and make a reschedule request for it.
 *
 * @param[inout] vcpu A pointer which points to the target vcpu structure which will be paused
 * @param[in] new_state The new state that \a vcpu shall be put in
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation
 *
 */
void pause_vcpu(struct acrn_vcpu *vcpu)
{
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - vcpu->vcpu_id: id of the vcpu
	 */
	pr_dbg("vcpu%hu paused", vcpu->vcpu_id);

	/** If vcpu->state is VCPU_RUNNING or VCPU_INIT */
	if ((vcpu->state == VCPU_RUNNING) || (vcpu->state == VCPU_INIT)) {
		/** Set prev_state of the vcpu to vcpu->state */
		vcpu->prev_state = vcpu->state;
		/** Set vcpu->state to VCPU_ZOMBIE */
		vcpu->state = VCPU_ZOMBIE;

		/** If vcpu->prev_state is VCPU_RUNNING */
		if (vcpu->prev_state == VCPU_RUNNING) {
			/** Call sleep_thread() with the following parameters, in order to sleep the target thread.
			 *  - &vcpu->thread_obj: address of the target thread. */
			sleep_thread(&vcpu->thread_obj);

			/** If return value of pcpuid_from_vcpu(vcpu) is different with return value of get_cpu_id(). */
			if (pcpuid_from_vcpu(vcpu) != get_pcpu_id()) {
				/** Until vcpu->running is false */
				while (vcpu->running) {
					/** Call asm_pause(), in order to pause current physical CPU. */
					asm_pause();
				}
			}
		}
	}
}

/**
 * @brief Save XSAVE-managed user and supervisor state components
 *
 * This function saves all XSAVE-managed state components (including user and supervisor) that are enabled in the
 * current XCR0 and IA32_XSS.
 *
 * @param[out] xs_area A pointer which points to the target XSAVE area
 *
 * @return N/A
 *
 * @pre xs_area != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a xs_area is different among parallel invocation
 *
 */
static void asm_xsaves(struct xsave_area *xs_area)
{
	/** Execute xsaves in order to save extend state components to the xsave area located at target memory address.
	 *  - Input operands: *xs_area, RDX holds UINT32_MAX, RAX holds UINT32_MAX.
	 *  - Output operands: N/A
	 *  - Clobbers: memory */
	asm volatile("xsaves %0" : : "m"(*xs_area), "d"(UINT32_MAX), "a"(UINT32_MAX) : "memory");
}

/**
 * @brief This function is used to save the physical state-component bitmaps and XSAVE-managed
 * user and supervisor state components to the given extended context.
 *
 * @param[inout] ectx A pointer which points to the target ext_context structure
 *
 * @return N/A
 *
 * @pre ectx != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a ectx is different among parallel invocation
 *
 */
void save_xsave_area(struct ext_context *ectx)
{
	/** Call read_xcr() with 0 being parameter, in order to read host CR0,
	 *  and set ectx->xcr0 to its return value */
	ectx->xcr0 = read_xcr(0);
	/** Call write_xcr() with the following parameters, in order to
	 *  write (ectx->xcr0 | XCR0_SSE | XCR0_AVX) into the XCR0.
	 *  - 0
	 *  - (ectx->xcr0 | XCR0_SSE | XCR0_AVX) */
	write_xcr(0, (ectx->xcr0 | XCR0_SSE | XCR0_AVX));
	/** Call msr_read() with MSR_IA32_XSS being parameter, in order to read host
	 *  MSR with index of MSR_IA32_XSS and set ectx->xss to its return value */
	ectx->xss = msr_read(MSR_IA32_XSS);
	/** Call asm_xsaves() with &ectx->xs_area being the parameter, in order to save XSAVE managed state components
	 *  to ectx->xs_area. */
	asm_xsaves(&ectx->xs_area);
}

/**
 * @brief Restore XSAVE-managed user and supervisor state components
 *
 * This function restores all XSAVE-managed state components (including user and supervisor) that are enabled in the
 * current XCR0 and IA32_XSS.
 *
 * @param[in] xs_area A pointer which points to the target XSAVE area
 *
 * @return N/A
 *
 * @pre xs_area != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
static void asm_xrstors(const struct xsave_area *xs_area)
{
	/** Execute xrstors in order to restore of processor state components from the XSAVE area located at the
	 * memory address.
	 *  - Input operands: *xs_area, RDX holds UINT32_MAX, RAX holds UINT32_MAX.
	 *  - Output operands: N/A
	 *  - Clobbers memory */
	asm volatile("xrstors %0" : : "m"(*xs_area), "d"(UINT32_MAX), "a"(UINT32_MAX) : "memory");
}

/**
 * @brief This function is used to restore the physical state-component bitmaps and XSAVE-managed
 * user and supervisor state components from the given extended context.
 *
 * @param[inout] ectx A pointer which points to the target ext_context structure
 *
 * @return N/A
 *
 * @pre ectx != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a ectx is different among parallel invocation
 *
 */
void rstore_xsave_area(const struct ext_context *ectx)
{
	/** Call write_xcr with the following parameters, in order write xcr0 of extend context to target xcr0.
	 *  - 0: number of xcr
	 *  - (ectx->xcr0 | XCR0_SSE | XCR0_AVX): value to write to the xcr0 */
	write_xcr(0, (ectx->xcr0 | XCR0_SSE | XCR0_AVX));
	/** Call msr_write() with the following parameters, in order to write xss of extend context to MSR_IA32_XSS.
	 *  - MSR_IA32_XSS: target MSR to write
	 *  - ectx->xss: value to write to the msr */
	msr_write(MSR_IA32_XSS, ectx->xss);
	/** Call asm_xrstors() with &ectx->xs_area being the parameter, in order to restore XSAVE managed state
	 *  components from ectx->xs_area. */
	asm_xrstors(&ectx->xs_area);
	/** Call write_xcr with the following parameters, in order write xcr0 of extend context to target xcr0.
	 *  - 0: number of xcr
	 *  - ectx->xcr0: value to write to the xcr0 */
	write_xcr(0, ectx->xcr0);
}

/**
 * @brief The function is used to save the extend context for target thread.
 *
 * @param[inout] prev A pointer which points to the target thread_object structure
 *
 * @return N/A
 *
 * @pre prev != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a prev is different among parallel invocation
 *
 */
static void context_switch_out(struct thread_object *prev)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing the pointer which points to vcpu, initialized as
	 *  list_entry(prev, struct acrn_vcpu, thread_obj).
	 */
	struct acrn_vcpu *vcpu = list_entry(prev, struct acrn_vcpu, thread_obj);
	/** Declare the following local variables of type struct ext_context *.
	 *  - ectx representing the pointer that points to the extend context, initialized
	 *  as &(vcpu->arch.context.ext_ctx).
	 */
	struct ext_context *ectx = &(vcpu->arch.context.ext_ctx);

	/** Set ia32_star of the extend context to msr_read(MSR_IA32_STAR) */
	ectx->ia32_star = msr_read(MSR_IA32_STAR);
	/** Set ia32_lstar of the extend context to msr_read(MSR_IA32_LSTAR) */
	ectx->ia32_lstar = msr_read(MSR_IA32_LSTAR);
	/** Set ia32_fmask of the extend context to msr_read(MSR_IA32_FMASK) */
	ectx->ia32_fmask = msr_read(MSR_IA32_FMASK);
	/** Set ia32_kernel_gs_base of the extend context to msr_read(MSR_IA32_KERNEL_GS_BASE) */
	ectx->ia32_kernel_gs_base = msr_read(MSR_IA32_KERNEL_GS_BASE);

	/** Call save_xsave_area() with the following parameters, in order to save the xsave component.
	 *  - ectx: extend context of the vcpu */
	save_xsave_area(ectx);

	/** Set vcpu->running to false */
	vcpu->running = false;
}

/**
 * @brief The function is used to restore the extend context for target thread.
 *
 * @param[inout] next A pointer which points to the target thread_object structure
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a next is different among parallel invocation
 *
 */
static void context_switch_in(struct thread_object *next)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing pointer which point to the vcpu, initialized as
	 *  list_entry(next, struct acrn_vcpu, thread_obj).
	 */
	struct acrn_vcpu *vcpu = list_entry(next, struct acrn_vcpu, thread_obj);
	/** Declare the following local variables of type struct ext_context *.
	 *  - ectx representing pointer which points to the extend context of vcpu, initialized as
	 *  &(vcpu->arch.context.ext_ctx)
	 */
	struct ext_context *ectx = &(vcpu->arch.context.ext_ctx);

	/** Call load_vmcs() with the following parameters, in order to load the VMCS of the vcpu.
	 *  - vcpu: the target vcpu to load vmcs */
	load_vmcs(vcpu);

	/** Call msr_write() with the following parameters, in order to set ectx->ia32_star to target msr.
	 *  - MSR_IA32_STAR: the index of target MSR
	 *  - ectx->ia32_star: the value to set */
	msr_write(MSR_IA32_STAR, ectx->ia32_star);
	/** Call msr_write() with the following parameters, in order to set ectx->ia32_lstar to target msr.
	 *  - MSR_IA32_LSTAR: the index of target MSR
	 *  - ectx->ia32_lstar: the value to set */
	msr_write(MSR_IA32_LSTAR, ectx->ia32_lstar);
	/** Call msr_write() with the following parameters, in order to set ectx->ia32_fmask to target msr.
	 *  - MSR_IA32_FMASK: the index of target MSR
	 *  - ectx->ia32_fmask: the value to set */
	msr_write(MSR_IA32_FMASK, ectx->ia32_fmask);
	/** Call msr_write() with the following parameters, in order to set ectx->ia32_lstar to target msr.
	 *  - MSR_IA32_KERNEL_GS_BASE: the index of target MSR
	 *  - ectx->ia32_kernel_gs_base: the value to set */
	msr_write(MSR_IA32_KERNEL_GS_BASE, ectx->ia32_kernel_gs_base);

	/** Call rstore_xsave_area() with the following parameters, in order to restore state components
	 *  from the XSAVE area.
	 *  - ectx: the xsave area to restore  */
	rstore_xsave_area(ectx);

	/** Set running state of the vcpu to true */
	vcpu->running = true;
}

/**
 * @brief This function is used to launch the target vcpu.
 * Adds a vCPU into the run queue and make a reschedule request for it. It sets the vCPU state to VCPU_RUNNING.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure representing the vCPU which will be launched
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
void launch_vcpu(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing id of the pcpu, initialized as pcpuid_from_vcpu(vcpu). */
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	/** Set state of the vcpu to VCPU_RUNNING */
	vcpu->state = VCPU_RUNNING;
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - vcpu->vcpu_id: the current vcpu id
	 *  - pcpu_id: the current pcpu id */
	pr_dbg("vcpu%hu scheduled on pcpu%hu", vcpu->vcpu_id, pcpu_id);

	/** Call wake_thread with the following parameters, in order to wake up the target thread.
	 *  - &vcpu->thread_obj: address of the vcpu thread */
	wake_thread(&vcpu->thread_obj);
}

/**
 * @brief This function is used to create a vcpu for the vm and mapped to the pcpu.
 *
 * @param[inout] vm A pointer which points to a vm structure representing the VM whose vCPU shall be created
 * @param[in] pcpu_id The ID of the physical processor that the vCPU shall be executed on
 *
 * @return An error code indicating if the vCPU creation succeeds or not.
 *
 * @retval 0 on success
 * @retval -EINVAL if the vCPU ID is invalid
 *
 * @pre vm != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when vcpu is different among parallel invocation.
 *
 */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id)
{
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the return value. */
	int32_t ret;
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing pointer which points to the vcpu, initialized as NULL. */
	struct acrn_vcpu *vcpu = NULL;

	/** Set ret to return value of create_vcpu(pcpu_id, vm, &vcpu) */
	ret = create_vcpu(pcpu_id, vm, &vcpu);
	/** If ret equals to 0 */
	if (ret == 0) {
		/** Set sched_ctrl of the vcpu->thread_obj to &per_cpu(sched_ctl, pcpu_id) */
		vcpu->thread_obj.sched_ctl = &per_cpu(sched_ctl, pcpu_id);
		/** Set thread_entry of the vcpu->thread_obj to vcpu_thread */
		vcpu->thread_obj.thread_entry = vcpu_thread;
		/** Set pcpu_id of the vcpu->thread_obj to pcpu_id */
		vcpu->thread_obj.pcpu_id = pcpu_id;
		/** Set host_sp of the vcpu->thread_obj to return value of build_stack_frame()
		 *  with vcpu being parameter */
		vcpu->thread_obj.host_sp = build_stack_frame(vcpu);
		/** Set switch_out of the vcpu->thread_obj to context_switch_out */
		vcpu->thread_obj.switch_out = context_switch_out;
		/** Set switch_in of the vcpu->thread_obj to context_switch_in */
		vcpu->thread_obj.switch_in = context_switch_in;
		/** Call init_thread_data() with the following parameters, in order to initial the target thread.
		 *  - &vcpu->thread_obj: address of the target thread */
		init_thread_data(&vcpu->thread_obj);
	}

	/** Return ret */
	return ret;
}

/**
 * @brief This function is used to get pcpu id of the target vcpu.
 *
 * @param[inout] vcpu A pointer which points to a vcpu structure representing the vCPU which will be launched
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu)
{
	/** Return sched_get_pcpuid(&vcpu->thread_obj) */
	return sched_get_pcpuid(&vcpu->thread_obj);
}

/**
 * @brief The function converts a bitmap of vCPUs of a VM to a bitmap of corresponding pCPUs they run on.
 *
 * @param[in] vm A pointer to a VM structure representing the VM acting as the context of the conversion
 * @param[in] vdmask A bitmap of vCPU IDs to be converted
 *
 * @return N/A
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 *
 */
uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask)
{
	/** Declare the following local variables of type uint16_t.
	 *  - vcpu_id representing id of the vcpu */
	uint16_t vcpu_id;
	/** Declare the following local variables of type uint64_t.
	 *  - dmask representing destination cpu mask, initialized as 0 */
	uint64_t dmask = 0UL;
	/** Declare the following local variables of type struct vcpu *.
	 *  - vcpu representing pointer which points to the vcpu */
	struct acrn_vcpu *vcpu;

	/** For each vcpu_id ranging from 0 to (vm->hw.created_vcpus - 1) [with a step of 1] */
	for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
		/** If bit[vcpu_id] of vdmask is 1H */
		if ((vdmask & (1UL << vcpu_id)) != 0UL) {
			/** Set vcpu to vcpu_from_vid(vm, vcpu_id) */
			vcpu = vcpu_from_vid(vm, vcpu_id);
			/** Call bitmap_set_nolock with the following parameters, in order to set bit
			 *  pcpuid_from_vcpu(vcpu) to 1.
			 *  - pcpuid_from_vcpu(vcpu): index of the bit to be set
			 *  - &dmask: address if the integer where the bit is to be set */
			bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &dmask);
		}
	}

	/** Return destination cpu mask */
	return dmask;
}

/**
 * @}
 */
