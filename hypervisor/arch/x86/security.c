/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <msr.h>
#include <cpufeatures.h>
#include <cpu.h>
#include <per_cpu.h>
#include <cpu_caps.h>
#include <security.h>
#include <logmsg.h>

/**
 * @defgroup hwmgmt_security hwmgmt.security
 * @ingroup hwmgmt
 * @brief Implementation of security related utilities functions and API detecting the security capabilities.
 *
 * This module detects security mitigation system software interface for known processor vulnerabilities,
 * such as Spectre, L1TF, MDS, MCE on underlying platform, wraps utility functions for processor
 * vulnerabilities mitigation, or initializes the runtime stack protector in hypervisor.
 *
 * Usage Remarks: Init module uses this module to verify security system software interfaces and setup stack
 * protector context, and vCPU module uses this module to clear CPU internal buffers.
 *
 * Dependency Justification: this module uses hwmgmt.cpu module to access MSR registers, and also uses
 * hwmgmt.cpu_caps to fetch hardware capabilities.
 *
 * External APIs:
 *  - check_cpu_security_cap()     This function checks the security capability for physical platform.
 *				   Depends on:
 *				    - pcpu_has_cap()
 *				    - msr_read()
 *
 *  - cpu_internal_buffers_clear() This function clears the CPU internal buffers.
 *				   Depends on:
 *				    - verw_buffer_overwriting()
 *
 *  - set_fs_base()                This function initializes the stack protector context.
 *				   Depends on:
 *				    - get_random_value()
 *				    - msr_write()
 *
 *  - cpu_l1d_flush()              This function flushes L1 data cache if it is required on VM entry.
 *				   Depends on:
 *				    - msr_write()
 *
 * Internal functions that are wrappers of inline assembly required by the coding guideline:
 *  - verw_buffer_overwriting()    Wrapper of inline assembly to overwrite CPU internal buffers
 *				   with VERW instruction.
 *  - get_random_value()           Wrapper of inline assembly to get a random number with rdrandom instruction.
 *
 * @{
 */

/**
 * @file
 * @brief this file implements APIs that shall be provided by the hwmgmt.security module.
 */

/**
* @brief Global Boolean variable indicating if the L1D flush is required(false)
* or not(true) when VM entry.
*
* According to the Intel SDM, 2.1 Vol.4:
* SKIP_L1DFL_VMENTRY (bit3 of IA32_ARCH_CAPABILITIES): A value of 1 indicates the hypervisor
* need not flush the L1D cache on VM entry.
*
* This global variable is true only if SKIP_L1DFL_VMENTRY bit is set.
**/
static bool skip_l1dfl_vmentry;

/**
* @brief Global Boolean variable indicating if CPU internal buffer flush is required(true)
* or not(false) when VM entry.
*
* This global variable is true only if CPUID.(EAX=7H, ECX=0):EDX[MD_CLEAR=10] is set
* and 'skip_l1dfl_vmentry' is true.
**/
static bool cpu_md_clear;

/**
 * @brief Check the security system software interfaces for underlying platform
 *
 * Check if system software interfaces are supported by physical platfrom for
 * known CPU vulnerabilities mitigation.
 *
 * @return A Boolean value indicating whether software interfaces are sufficient to mitigate
 * known CPU vulnerabilities
 *
 * @pre n/a
 *
 * @post n/a
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark n/a
 *
 * @reentrancy unspecified
 *
 * @threadsafety unspecified
 */
bool check_cpu_security_cap(void)
{
	/** Declare the following local variables of type bool.
	 *  - ret representing CPU ucode is qualified(true) or not(false), initialized as true.
	 */
	bool ret = true;
	/** Declare the following local variables of type bool.
	 *  - Set mds_no to true only if IA32_ARCH_CAP_MDS_NO bit in MSR_IA32_ARCH_CAPABILITIES is set,
	 *   otherwise set it to false.
	 */
	bool mds_no = false;
	/** Declare the following local variables of type bool.
	 *  - Set ssb_no to true only if IA32_ARCH_CAP_SSB_NO bit in MSR_IA32_ARCH_CAPABILITIES is set,
	 *   otherwise set it to false.
	 */
	bool ssb_no = false;
	/** Declare the following local variables of type uint64_t.
	 *  - x86_arch_capabilities representing bitmap value of MSR_IA32_ARCH_CAPABILITIES,
	 * initialized as 0.
	 */
	uint64_t x86_arch_capabilities = 0UL;

	/** If IA32_ARCH_CAPABILITIES MSR is supported by physical platform.*/
	if (pcpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		/** Set x86_arch_capabilities to the value of MSR_IA32_ARCH_CAPABILITIES */
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		/** Set skip_l1dfl_vmentry to false if flushing L1D is required(false)
		 *  or true if flush L1D is not required on VM entry.
		 */
		skip_l1dfl_vmentry = ((x86_arch_capabilities & IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY) != 0UL);

		/** Set mds_no to true if IA32_ARCH_CAP_MDS_NO bit in x86_arch_capabilities is set. */
		mds_no = ((x86_arch_capabilities & IA32_ARCH_CAP_MDS_NO) != 0UL);

		/** Set ssb_no to true if IA32_ARCH_CAP_SSB_NO bit in x86_arch_capabilities is set. */
		ssb_no = ((x86_arch_capabilities & IA32_ARCH_CAP_SSB_NO) != 0UL);
	}

	/** If L1D flush on VM entry is required, but L1D_FLUSH command is not
	 *  supported by physical platform.
	 */
	if ((!pcpu_has_cap(X86_FEATURE_L1D_FLUSH)) && (!skip_l1dfl_vmentry)) {
		/** Set the qualification of CPU ucode to false */
		ret = false;
	}

	/** If Processor is susceptble to SSB, but SSBD is not supported by physical platform. */
	if ((!pcpu_has_cap(X86_FEATURE_SSBD)) && (!ssb_no)) {
		/** Set the qualification of CPU ucode to false */
		ret = false;
	}

	/** If IBRS and IBPB are not supported by physical platform. */
	if (!pcpu_has_cap(X86_FEATURE_IBRS_IBPB)) {
		/** Set the qualification of CPU ucode to false */
		ret = false;
	}

	/** If mds_no is false*/
	if (!mds_no) {
		/** If MD_CLEAR is supported by physical platform. */
		if (pcpu_has_cap(X86_FEATURE_MDS_CLEAR)) {
			/** MSR_IA32_ARCH_CAPABILITIES is not supported by physical platform or
			 *  IA32_ARCH_CAP_MDS_NO bit in MSR_IA32_ARCH_CAPABILITIES is not set,
			 *  and MD_CLEAR is supported by physical platfrom.
			 *  In this case, set cpu_md_clear to true to indicate CPU internal buffers
			 *  flush shall be required on VM entry.
			 */
			cpu_md_clear = true;
			/** If the hypervisor need flush the L1D on VM entry */
			if (!skip_l1dfl_vmentry) {
				/** L1D cache flush will also overwrite CPU internal buffers,
				 *  additional MDS buffers clear operation is not required.
				 *  In this case, set cpu_md_clear to false to indicate CPU internal
				 *  buffers flush is not required on VM entry.
				 */
				cpu_md_clear = false;
			}
		} else {
			/** Processor is affected by MDS but no mitigation software
			 *  interface is enumerated, CPU microcode need to be udpated.
			 *  Set the qualification of CPU ucode to false.
			 */
			ret = false;
		}
	}

	/** Return the qualification of CPU ucode */
	return ret;
}

 /**
  * @brief Flush L1 data cache if it is required on VM entry.
  *
  * This function flushes L1 data cache if such flush is required on VM entry(current processor is
  * potentially affected by L1TF CPU vulnerability),and flushing L1 data cache also clears CPU
  * internal buffers if this processor is affected by MDS vulnerability.
  *
  *
  * @return None
  *
  * @pre None
  *
  * @post None
  *
  * @mode HV_OPERATIONAL
  *
  * @reentrancy unspecified
  *
  * @threadsafety yes
  */
void cpu_l1d_flush(void)
{
	/** If the hypervisor need flush the L1D on VM entry */
	if (!skip_l1dfl_vmentry) {
		/** If L1D_FLUSH is supported by physical platform. */
		if (pcpu_has_cap(X86_FEATURE_L1D_FLUSH)) {
			/**
			 * Call msr_write() with the following parameters,
			 * in order to flush L1D cache.
			 *  - MSR_IA32_FLUSH_CMD
			 *  - IA32_L1D_FLUSH
			 */
			msr_write(MSR_IA32_FLUSH_CMD, IA32_L1D_FLUSH);
		}
	}

}

 /**
 * @brief Overwrite CPU internal buffers with VERW instruction
 *
 * On processors that enumerate MD_CLEAR:CPUID.(EAX=7H,ECX=0):EDX[MD_CLEAR=10],
 * the VERW instruction or L1D_FLUSH command should be used to cause the
 * processor to overwrite buffer values that are affected by MDS
 * (Microarchitectural Data Sampling) vulnerabilities.
 *
 * The VERW instruction and L1D_FLUSH command will overwrite below buffer values:
 *  - Store buffer value for the current logical processor on processors affected
 *    by MSBDS (Microarchitectural Store Buffer Data Sampling).
 *  - Fill buffer for all logical processors on the physical core for processors
 *    affected by MFBDS (Microarchitectural Fill Buffer Data Sampling).
 *  - Load port for all logical processors on the physical core for processors
 *    affected by MLPDS(Microarchitectural Load Port Data Sampling).
 *
 * If processor is affected by L1TF vulnerability and the mitigation is enabled,
 * L1D_FLUSH will overwrite internal buffers on processors affected by MDS, no
 * additional buffer overwriting is required before VM entry. For other cases,
 * VERW instruction is used to overwrite buffer values for processors affected
 * by MDS.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety yes
 */
static inline void verw_buffer_overwriting(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - ds representing a writable segment descriptor, initialized as HOST_GDT_RING0_DATA_SEL */
	uint16_t ds = HOST_GDT_RING0_DATA_SEL;

	/** Execute verw in order to overwrite CPU internal buffers.
	 *  - Input operands: ds, the segment selector, this segment shall be writable
	 *  - Output operands: none
	 *  - Clobbers: cc
	 */
	asm volatile("verw %[ds]" : : [ ds ] "m"(ds) : "cc");
}

/**
 * @brief Clear CPU internal buffers.
 *
 *  This function clears CPU internal buffers if it is required, by calling
 *  verw_buffer_overwriting().
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
void cpu_internal_buffers_clear(void)
{
	/** If CPU internal buffers clearing is required */
	if (cpu_md_clear) {
		/** Call verw_buffer_overwriting() with the following parameters,
		 * in order to overwrite CPU internal buffers.
		 *  - none */
		verw_buffer_overwriting();
	}
}

#ifdef STACK_PROTECTOR
/**
 * @brief Get a random value
 *
 * Get a random number by executing rdrandom instruction.
 *
 * @return a 64bit random number
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 * @threadsafety unspecified
 */
static uint64_t get_random_value(void)
{
	/** Declare the following local variables of type uint64_t.
	 *  - random representing number to be returned, initialized as 0.
	 */
	uint64_t random = 0UL;

	/** Execute rdrand in order to read a random number.
	 *  - Input operands: none
	 *  - Output operands: random
	 *  - Clobbers rax
	 */
	asm volatile("1: rdrand %%rax\n"
		     "jnc 1b\n"
		     "mov %%rax, %0\n"
		     : "=r"(random)
		     :
		     : "%rax");
	/** Return the target random number */
	return random;
}

/**
 * @brief Initialize the per-CPU stack canary structure for the current physical CPU
 *
 * Initialize the per-CPU stack canary structure for the current physical CPU by
 * assigning a random value to the canary. Also set the IA32_FS_BASE MSR of the current
 * physical CPU to the base address of the per-CPU stack canary structure for the
 * current physical CPU.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_INIT
 *
 * @reentrancy unspecified
 * @threadsafety unspecified
 */
void set_fs_base(void)
{
	/** Declare the following local variables of type struct stack_canary *.
	 *  - psc representing pointer to instance of struct stack_canary,
	 *  initialized as address of get_cpu_var(stk_canary).
	 */
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	/** Set canary value in stack_canary to the value returned by get_random_value */
	psc->canary = get_random_value();
	/** Call msr_write() with the following parameters, in order to configure MSR_IA32_FS_BASE.
	 *  - psc
	 */
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif

#ifdef CONFIG_MCE_ON_PSC_WORKAROUND_DISABLED
bool is_ept_force_4k_ipage(void)
{
	return false;
}
#else
bool is_ept_force_4k_ipage(void)
{
	bool force_4k_ipage = true;
	const struct cpuinfo_x86 *info = get_pcpu_info();
	uint64_t x86_arch_capabilities;

	if (info->displayfamily == 0x6U) {
		switch (info->displaymodel) {
		case 0x26U:
		case 0x27U:
		case 0x35U:
		case 0x36U:
		case 0x37U:
		case 0x86U:
		case 0x1CU:
		case 0x4AU:
		case 0x4CU:
		case 0x4DU:
		case 0x5AU:
		case 0x5CU:
		case 0x5DU:
		case 0x5FU:
		case 0x6EU:
		case 0x7AU:
			/* Atom processor is not affected by the issue
			 * "Machine Check Error on Page Size Change"
			 */
			force_4k_ipage = false;
			break;
		default:
			force_4k_ipage = true;
			break;
		}
	}

	if (pcpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		if ((x86_arch_capabilities & IA32_ARCH_CAP_IF_PSCHANGE_MC_NO) != 0UL) {
			force_4k_ipage = false;
		}
	}

	return force_4k_ipage;
}
#endif

/**
 * @}
 */
