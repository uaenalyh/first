/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <pgtable.h>
#include <msr.h>
#include <cpuid.h>
#include <vcpu.h>
#include <vm.h>
#include <vmx.h>
#include <guest_pm.h>
#include <ucode.h>
#include <trace.h>
#include <logmsg.h>

/**
 * @defgroup vp-base_vmsr vp-base.vmsr
 * @ingroup vp-base
 * @brief Implementation of all external APIs to virtualize RDMSR and WRMSR instructions.
 *
 * This module implements the virtualization of RDMSR and WRMSR instructions executed from guest software.
 * In ACRN hypervisor, 'use MSR bitmaps' control bit in Primary Processor-Based VM-Execution Controls is 1 and
 * MSR bitmaps are used to control execution of the RDMSR and WRMSR instructions from guest software.
 * An execution of RDMSR or WRMSR from guest software causes a VM exit if the value of RCX is in neither of the
 * ranges covered by the MSR bitmaps or if the appropriate bit in the MSR bitmaps (corresponding to the instruction
 * and the RCX value) is 1.
 * Hypervisor would set up the MSR bitmaps for each vCPU and handle the VM exit caused by RDMSR or WRMSR instruction
 * executed from guest software.
 *
 * Usage:
 * - 'vp-base.hv_main' module depends on this module to set up the MSR bitmaps for the specified vCPU and
 * handle the VM exit caused by RDMSR or WRMSR instruction executed from guest software.
 * - 'vp-base.vcpu' module depends on this module to get the corresponding index for the specified MSR.
 *
 * Dependency:
 * - This module depends on 'debug' module to log some information and trace some events in debug phase.
 * - This module depends on 'hwmgmt.time' module to get the current TSC value.
 * - This module depends on 'hwmgmt.vmx' module to access some fields in VMCS, such as Address of MSR bitmaps,
 * Guest IA32_PAT, and TSC offset.
 * - This module depends on 'vp-base.vcpu' module to access guest states, such as MSR, RAX, RCX, RDX,
 * the vLAPIC of the specified vCPU, and the physical CPU ID associated with the specified vCPU.
 * - This module depends on 'vp-base.vcpuid' module to access guest CPUID.80000001H.
 * - This module depends on 'vp-base.vcr' module to access the CR0 associated with the specified vCPU.
 * - This module depends on 'vp-base.virq' module to flush TLB entries and paging structure cache entries applicable to
 * the specified vCPU.
 * - This module depends on 'vp-base.vlapic' module to access some guest MSRs related to virtual LAPIC,
 * such as IA32_TSC_DEADLINE, IA32_APIC_BASE, and x2APIC MSRs.
 * - This module depends on 'vp-base.vm' module to check if the specified VM is a safety VM or not.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the vp-base.vmsr module.
 *
 * This file implements all external functions that shall be provided by the vp-base.vmsr module and
 * it defines all macros and global variables that are used in this file.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: is_x2apic_msr, enable_msr_interception, is_pat_mem_type_invalid, is_mc_ctl2_msr,
 * is_mc_ctl_msr, is_mc_status_msr, and set_tsc_msr_interception.
 *
 * Decomposed functions include: intercept_x2apic_msrs, init_msr_area, write_pat_msr, set_guest_tsc,
 * set_guest_tsc_adjust, set_guest_ia32_misc_enable, write_efer_msr, and update_msr_bitmap_x2apic_passthru.
 *
 */

/**
 * @brief A flag which indicates that RDMSR and WRMSR instructions executed from guest would not cause VM exit.
 *
 * This flag is not associated with all MSRs. Each MSR has its own setting to indicate its behavior.
 */
#define INTERCEPT_DISABLE    (0U)
/**
 * @brief A flag which indicates that RDMSR instruction executed from guest would cause VM exit.
 *
 * This flag is not associated with all MSRs. Each MSR has its own setting to indicate its behavior.
 */
#define INTERCEPT_READ       (1U << 0U)
/**
 * @brief A flag which indicates that WRMSR instruction executed from guest would cause VM exit.
 *
 * This flag is not associated with all MSRs. Each MSR has its own setting to indicate its behavior.
 */
#define INTERCEPT_WRITE      (1U << 1U)
/**
 * @brief A flag which indicates that RDMSR and WRMSR instructions executed from guest would cause VM exit.
 *
 * This flag is not associated with all MSRs. Each MSR has its own setting to indicate its behavior.
 */
#define INTERCEPT_READ_WRITE (INTERCEPT_READ | INTERCEPT_WRITE)

/**
 * @brief Bit indicator for STIBP (Single Thread Indirect Branch Predictors) Bit in MSR IA32_SPEC_CTRL.
 */
#define MSR_IA32_SPEC_CTRL_STIBP	(1UL << 1U)

/**
 * @brief The contents of guest MSR IA32_MCG_CAP for the safety VM. This value is defined in SRS.
 */
#define MCG_CAP_FOR_SAFETY_VM		0x040AUL

/**
 * @brief Bit mask of non-reserved bits in MSR IA32_MISC_ENABLE.
 *
 * Only following bits are not reserved: 'Limit CPUID Maxval' Bit (Bit 22) and 'XD Bit Disable' Bit (Bit 34).
 */
#define MSR_IA32_MISC_ENABLE_MASK	(MSR_IA32_MISC_ENABLE_LIMIT_CPUID | MSR_IA32_MISC_ENABLE_XD_DISABLE)

/**
 * @brief Bit mask of non-reserved bits in MSR IA32_EFER.
 *
 * Only following bits are not reserved: 'SYSCALL Enable' Bit (Bit 0), 'IA-32e Mode Enable' Bit (Bit 8),
 * 'IA-32e Mode Active' Bit (Bit 10), and 'Execute Disable Bit Enable' Bit (Bit 11).
 */
#define MSR_IA32_EFER_MASK		0xD01UL

/**
 * @brief The start address of low MSRs.
 *
 * Low MSRs contain the MSR whose address is in the range 00000000H to 00001FFFH.
 */
#define LOW_MSR_START			0U
/**
 * @brief The end address of low MSRs.
 *
 * Low MSRs contain the MSR whose address is in the range 00000000H to 00001FFFH.
 */
#define LOW_MSR_END			0x1FFFU
/**
 * @brief The start address of high MSRs.
 *
 * High MSRs contain the MSR whose address is in the range C0000000H to C0001FFFH.
 */
#define HIGH_MSR_START			0xc0000000U
/**
 * @brief The end address of high MSRs.
 *
 * High MSRs contain the MSR whose address is in the range C0000000H to C0001FFFH.
 */
#define HIGH_MSR_END			0xc0001FFFU

/**
 * @brief A placeholder to reserve entries in array 'emulated_guest_msrs' for scope extension in the future.
 */
#define MSR_RSVD			0xFFFFFFFFU

/**
 * @brief Number of reporting banks for machine check. This value is defined in SRS.
 */
#define NUM_MC_BANKS			10U

/**
 * @brief An array which contains the emulated MSRs.
 *
 * The contents of these emulated MSRs can be accessed via vcpu_get_guest_msr and vcpu_set_guest_msr provided by
 * vp-base.vcpu module.
 */
static const uint32_t emulated_guest_msrs[NUM_GUEST_MSRS] = {
	/*
	 * MSRs that trusty may touch and need isolation between secure and normal world
	 * This may include MSR_IA32_STAR, MSR_IA32_LSTAR, MSR_IA32_FMASK,
	 * MSR_IA32_KERNEL_GS_BASE, MSR_IA32_SYSENTER_ESP, MSR_IA32_SYSENTER_CS, MSR_IA32_SYSENTER_EIP
	 *
	 * Number of entries: NUM_WORLD_MSRS
	 */
	MSR_IA32_PAT,
	MSR_IA32_TSC_ADJUST,

	/*
	 * MSRs don't need isolation between worlds
	 * Number of entries: NUM_COMMON_MSRS
	 */
	MSR_IA32_TSC_DEADLINE,
	MSR_RSVD,			/* MSR_IA32_BIOS_UPDT_TRIG, */
	MSR_IA32_BIOS_SIGN_ID,
	MSR_IA32_TIME_STAMP_COUNTER,
	MSR_RSVD,			/* MSR_IA32_APIC_BASE, */
	MSR_RSVD,			/* MSR_IA32_PERF_CTL, */
	MSR_IA32_FEATURE_CONTROL,

	MSR_IA32_MCG_CAP,
	MSR_RSVD,			/* MSR_IA32_MCG_STATUS, */
	MSR_IA32_MISC_ENABLE,

	/* Don't support SGX Launch Control yet, read only */
	MSR_RSVD,			/* MSR_IA32_SGXLEPUBKEYHASH0, */
	MSR_RSVD,			/* MSR_IA32_SGXLEPUBKEYHASH1, */
	MSR_RSVD,			/* MSR_IA32_SGXLEPUBKEYHASH2, */
	MSR_RSVD,			/* MSR_IA32_SGXLEPUBKEYHASH3, */
	/* Read only */
	MSR_RSVD,			/* MSR_IA32_SGX_SVN_STATUS, */
};

/**
 * @brief Number of MSRs that are not intercepted.
 *
 * If a MSR is not intercepted, it means that RDMSR and WRMSR instructions executed from guest associated with this MSR
 * would not cause VM exit.
 */
#define NUM_UNINTERCEPTED_MSRS 20U
/**
 * @brief An array which contains the MSRs that are not intercepted.
 *
 * If a MSR is not intercepted, it means that RDMSR and WRMSR instructions executed from guest associated with this MSR
 * would not cause VM exit.
 */
static const uint32_t unintercepted_msrs[NUM_UNINTERCEPTED_MSRS] = {
	MSR_IA32_P5_MC_ADDR,
	MSR_IA32_P5_MC_TYPE,
	MSR_IA32_PLATFORM_ID,
	MSR_SMI_COUNT,
	MSR_IA32_PRED_CMD,
	MSR_PLATFORM_INFO,
	MSR_IA32_FLUSH_CMD,
	MSR_FEATURE_CONFIG,
	MSR_IA32_SYSENTER_CS,
	MSR_IA32_SYSENTER_ESP,
	MSR_IA32_SYSENTER_EIP,
	MSR_IA32_MCG_STATUS,
	MSR_IA32_STAR,
	MSR_IA32_LSTAR,
	MSR_IA32_CSTAR,
	MSR_IA32_FMASK,
	MSR_IA32_FS_BASE,
	MSR_IA32_GS_BASE,
	MSR_IA32_KERNEL_GS_BASE,
	MSR_IA32_TSC_AUX,
};

#define NUM_X2APIC_MSRS 44U /**< Number of the x2APIC MSRs. */
/**
 * @brief An array which contains the x2APIC MSRs.
 */
static const uint32_t x2apic_msrs[NUM_X2APIC_MSRS] = {
	MSR_IA32_EXT_XAPICID,
	MSR_IA32_EXT_APIC_VERSION,
	MSR_IA32_EXT_APIC_TPR,
	MSR_IA32_EXT_APIC_PPR,
	MSR_IA32_EXT_APIC_EOI,
	MSR_IA32_EXT_APIC_LDR,
	MSR_IA32_EXT_APIC_SIVR,
	MSR_IA32_EXT_APIC_ISR0,
	MSR_IA32_EXT_APIC_ISR1,
	MSR_IA32_EXT_APIC_ISR2,
	MSR_IA32_EXT_APIC_ISR3,
	MSR_IA32_EXT_APIC_ISR4,
	MSR_IA32_EXT_APIC_ISR5,
	MSR_IA32_EXT_APIC_ISR6,
	MSR_IA32_EXT_APIC_ISR7,
	MSR_IA32_EXT_APIC_TMR0,
	MSR_IA32_EXT_APIC_TMR1,
	MSR_IA32_EXT_APIC_TMR2,
	MSR_IA32_EXT_APIC_TMR3,
	MSR_IA32_EXT_APIC_TMR4,
	MSR_IA32_EXT_APIC_TMR5,
	MSR_IA32_EXT_APIC_TMR6,
	MSR_IA32_EXT_APIC_TMR7,
	MSR_IA32_EXT_APIC_IRR0,
	MSR_IA32_EXT_APIC_IRR1,
	MSR_IA32_EXT_APIC_IRR2,
	MSR_IA32_EXT_APIC_IRR3,
	MSR_IA32_EXT_APIC_IRR4,
	MSR_IA32_EXT_APIC_IRR5,
	MSR_IA32_EXT_APIC_IRR6,
	MSR_IA32_EXT_APIC_IRR7,
	MSR_IA32_EXT_APIC_ESR,
	MSR_IA32_EXT_APIC_LVT_CMCI,
	MSR_IA32_EXT_APIC_ICR,
	MSR_IA32_EXT_APIC_LVT_TIMER,
	MSR_IA32_EXT_APIC_LVT_THERMAL,
	MSR_IA32_EXT_APIC_LVT_PMI,
	MSR_IA32_EXT_APIC_LVT_LINT0,
	MSR_IA32_EXT_APIC_LVT_LINT1,
	MSR_IA32_EXT_APIC_LVT_ERROR,
	MSR_IA32_EXT_APIC_INIT_COUNT,
	MSR_IA32_EXT_APIC_CUR_COUNT,
	MSR_IA32_EXT_APIC_DIV_CONF,
	MSR_IA32_EXT_APIC_SELF_IPI,
};

/**
 * @brief Check whether the specified MSR belongs to x2APIC MSRs or not.
 *
 * Check whether the specified MSR belongs to x2APIC MSRs or not.
 *
 * It is supposed to be called by 'rdmsr_vmexit_handler' and 'wrmsr_vmexit_handler'.
 *
 * @param[in] msr The specified MSR to be checked.
 *
 * @return A Boolean value indicating whether the specified MSR belongs to x2APIC MSRs or not.
 *
 * @retval true The specified MSR belongs to x2APIC MSRs.
 * @retval false The specified MSR does not belong to x2APIC MSRs.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static bool is_x2apic_msr(uint32_t msr)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as array index, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type bool.
	 *  - ret representing whether the specified MSR belongs to x2APIC MSRs or not, initialized as false. */
	bool ret = false;

	/** For each 'i' ranging from 0H to 'NUM_X2APIC_MSRS - 1' [with a step of 1] */
	for (i = 0U; i < NUM_X2APIC_MSRS; i++) {
		/** If \a msr is equal to the ith element in array 'x2apic_msrs' */
		if (msr == x2apic_msrs[i]) {
			/** Set 'ret' to true, meaning that the specified \a msr belongs to x2APIC MSRs. */
			ret = true;
			/** Terminate the loop */
			break;
		}
	}

	/** Return 'ret' */
	return ret;
}

/* emulated_guest_msrs[] shares same indexes with array vcpu->arch->guest_msrs[] */
/**
 * @brief Return the corresponding index for the specified MSR.
 *
 * Return the corresponding index for the specified MSR. This index is associated with array 'emulated_guest_msrs'.
 *
 * It is supposed to be called by 'vcpu_get_guest_msr' and 'vcpu_set_guest_msr' from 'vp-base.vcpu' module.
 *
 * @param[in] msr The specified MSR whose corresponding index is requested.
 *
 * @return The corresponding index for the specified MSR.
 *
 * @retval less-than-NUM_GUEST_MSRS The specified MSR is the retval-th element in array 'emulated_guest_msrs'.
 * @retval NUM_GUEST_MSRS The specified MSR is not an element in array 'emulated_guest_msrs'.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
uint32_t vmsr_get_guest_msr_index(uint32_t msr)
{
	/** Declare the following local variables of type uint32_t.
	 *  - index representing the corresponding index for the specified \a msr, not initialized. */
	uint32_t index;

	/** For each 'index' ranging from 0H to 'NUM_GUEST_MSRS - 1' [with a step of 1] */
	for (index = 0U; index < NUM_GUEST_MSRS; index++) {
		/** If \a msr is equal to the index-th element in array 'emulated_guest_msrs' */
		if (emulated_guest_msrs[index] == msr) {
			/** Terminate the loop */
			break;
		}
	}

	/** If 'index' is equal to NUM_GUEST_MSRS */
	if (index == NUM_GUEST_MSRS) {
		/** Logging the following information with a log level of 3.
		 *  - __func__
		 *  - msr
		 */
		pr_err("%s, MSR %x is not defined in array emulated_guest_msrs[]", __func__, msr);
	}

	/** Return 'index' */
	return index;
}

/**
 * @brief Update the specified MSR bitmap according to the specified MSR and the specified mode.
 *
 * For the specified MSR, the corresponding bits in the specified MSR bitmap would be updated according to the
 * specified mode.
 * When \a mode is INTERCEPT_DISABLE, corresponding bit in MSR read bitmap and MSR write bitmap are to be updated to
 * both 0.
 * When \a mode is INTERCEPT_READ, corresponding bit in MSR read bitmap is to be updated to 1 and
 * corresponding bit in MSR write bitmap is to be updated to 0.
 * When \a mode is INTERCEPT_WRITE, corresponding bit in MSR read bitmap is to be updated to 0 and
 * corresponding bit in MSR write bitmap is to be updated to 1.
 * When \a mode is INTERCEPT_READ_WRITE, corresponding bit in MSR read bitmap and MSR write bitmap are to be updated to
 * both 1.
 *
 * In order to simplify statements in the control flow, the specified MSR bitmap is stated as 4096 8-bit fields,
 * including four contiguous parts. These four parts are: the read bitmap for low MSRs, the read bitmap for high MSRs,
 * the write bitmap for low MSRs, and the write bitmap for high MSRs. Each part takes 1024 8-bit fields and contains
 * the settings for 8192 MSRs (one bit for each MSR).
 * For each MSR, there are two 8-bit fields containing its corresponding bits. One locates in the read bitmap and
 * the other one locates in the write bitmap.
 *
 * It is supposed to be called when hypervisor attempts to update MSR bitmaps for any vCPU.
 *
 * @param[out] bitmap A pointer which points to the MSR bitmap that is to be updated.
 * @param[in] msr_arg The specified MSR whose corresponding bits in the MSR bitmap need to be updated.
 * @param[in] mode The specified mode which indicates whether RDMSR and WRMSR instructions executed from guest
 *                 associated with the specified MSR would cause VM exit or not.
 *
 * @return None
 *
 * @pre bitmap != NULL
 * @pre (mode & (~INTERCEPT_READ_WRITE)) == 0
 * @pre ((msr_arg >= LOW_MSR_START) && (msr_arg <= LOW_MSR_END)) ||
 *      ((msr_arg >= HIGH_MSR_START) && (msr_arg <= HIGH_MSR_END))
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety When \a bitmap is different among parallel invocation.
 */
static void enable_msr_interception(uint8_t *bitmap, uint32_t msr_arg, uint32_t mode)
{
	/** Declare the following local variables of type uint32_t.
	 *  - read_offset representing the offset of the read bitmap in the MSR bitmap, initialized as 0H. */
	uint32_t read_offset = 0U;
	/** Declare the following local variables of type uint32_t.
	 *  - write_offset representing the offset of the write bitmap in the MSR bitmap, initialized as 2048. */
	uint32_t write_offset = 2048U;
	/** Declare the following local variables of type uint32_t.
	 *  - msr representing the specified MSR whose corresponding bits in the MSR bitmap need to be updated,
	 *  initialized as 'msr_arg'. */
	uint32_t msr = msr_arg;
	/** Declare the following local variables of type uint8_t.
	 *  - msr_bit representing the bit mask of the specified bit which is to be updated, not initialized. */
	uint8_t msr_bit;
	/** Declare the following local variables of type uint32_t.
	 *  - msr_index representing the index of the 8-bit field which is to be updated, not initialized. */
	uint32_t msr_index;

	/** If the specified MSR is a high MSR, meaning that its address is in the range C0000000H to C0001FFFH */
	if ((msr & HIGH_MSR_START) != 0U) {
		/** Increment 'read_offset' by 1024, the new value is the offset of the read bitmap in the MSR bitmap
		 *  for high MSRs */
		read_offset = read_offset + 1024U;
		/** Increment 'write_offset' by 1024, the new value is the offset of the write bitmap in the MSR bitmap
		 *  for high MSRs */
		write_offset = write_offset + 1024U;
	}

	/** Clear high 19-bits of 'msr'.
	 *  High 19-bits of 'msr' can be used to distinguish low MSRs and high MSRs. (This information has been
	 *  detected with previous 'if' branch.)
	 *  Low 13-bits of 'msr' can be used to further detect the location of the corresponding bits. */
	msr &= 0x1FFFU;
	/** Set 'msr_bit' to the value of left shifting 1H with [low 3-bits of 'msr'] bits, which is
	 *  the bit mask of the corresponding bit in the 8-bit field for \a msr_arg */
	msr_bit = 1U << (msr & 0x7U);
	/** Set 'msr_index' to 1/8 of 'msr' (round down), which indicates the index of the 8-bit field where the
	 *  the setting for \a msr_arg locates */
	msr_index = msr >> 3U;

	/** If the specified \a mode indicates that RDMSR instruction executed from guest would cause VM exit. */
	if ((mode & INTERCEPT_READ) == INTERCEPT_READ) {
		/** Set the corresponding bit for the specified MSR in the MSR read bitmap to 1,
		 *  this bit is located in the 'read_offset + msr_index'-th 8-bit field in \a bitmap and
		 *  the modification is done with the help of the bit mask 'msr_bit'.
		 */
		bitmap[read_offset + msr_index] |= msr_bit;
	} else {
		/** Set the corresponding bit for the specified MSR in the MSR read bitmap to 0,
		 *  this bit is located in the 'read_offset + msr_index'-th 8-bit field in \a bitmap and
		 *  the modification is done with the help of the bit mask 'msr_bit'.
		 */
		bitmap[read_offset + msr_index] &= ~msr_bit;
	}

	/** If the specified \a mode indicates that WRMSR instruction executed from guest would cause VM exit. */
	if ((mode & INTERCEPT_WRITE) == INTERCEPT_WRITE) {
		/** Set the corresponding bit for the specified MSR in the MSR write bitmap to 1,
		 *  this bit is located in the 'write_offset + msr_index'-th 8-bit field in \a bitmap and
		 *  the modification is done with the help of the bit mask 'msr_bit'.
		 */
		bitmap[write_offset + msr_index] |= msr_bit;
	} else {
		/** Set the corresponding bit for the specified MSR in the MSR write bitmap to 0,
		 *  this bit is located in the 'write_offset + msr_index'-th 8-bit field in \a bitmap and
		 *  the modification is done with the help of the bit mask 'msr_bit'.
		 */
		bitmap[write_offset + msr_index] &= ~msr_bit;
	}
}

/**
 * @brief Update the specified MSR bitmap with regard to x2APIC MSRs.
 *
 * For all x2APIC MSRs, the corresponding bits in the specified MSR bitmap would be updated
 * according to the specified mode.
 * Refer to the function 'enable_msr_interception' for mode explanations.
 *
 * It is supposed to be called when hypervisor attempts to update MSR bitmaps with regard to x2APIC MSRs for any vCPU.
 *
 * @param[out] msr_bitmap A pointer which points to the MSR bitmap that is to be updated.
 * @param[in] mode The specified mode which indicates whether RDMSR and WRMSR instructions executed from guest
 *                 associated with the specified MSR would cause VM exit or not.
 *
 * @return None
 *
 * @pre msr_bitmap != NULL
 * @pre (mode & (~INTERCEPT_READ_WRITE)) == 0
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety When \a msr_bitmap is different among parallel invocation.
 */
static void intercept_x2apic_msrs(uint8_t *msr_bitmap, uint32_t mode)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as array index, not initialized. */
	uint32_t i;

	/** For each 'i' ranging from 0H to 'NUM_X2APIC_MSRS - 1' [with a step of 1] */
	for (i = 0U; i < NUM_X2APIC_MSRS; i++) {
		/** Call enable_msr_interception with the following parameters, in order to update \a msr_bitmap
		 *  according to the specified MSR 'x2apic_msrs[i]' and the specified \a mode.
		 *  - msr_bitmap
		 *  - x2apic_msrs[i]
		 *  - mode
		 */
		enable_msr_interception(msr_bitmap, x2apic_msrs[i], mode);
	}
}

/**
 * @brief Initialize the VMX-transition MSR areas for the specified vCPU.
 *
 * Initialize the VMX-transition MSR areas for the specified vCPU.
 *
 * It is supposed to be called only by 'init_msr_emulation'.
 *
 * @param[inout] vcpu A pointer which points to a structure representing the vCPU whose MSR areas are to be initialized.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_msr_area(struct acrn_vcpu *vcpu)
{
	/** Set vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_index to MSR_IA32_TSC_AUX, which is the MSR index
	 *  in the MSR Entry to be loaded on VM entry */
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	/** Set vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value to vcpu->vcpu_id, which is the virtual CPU ID
	 *  associated with \a vcpu and the MSR data in the MSR Entry to be loaded on VM entry */
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value = vcpu->vcpu_id;
	/** Set vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_index to MSR_IA32_TSC_AUX, which is the MSR index
	 *  in the MSR Entry to be loaded on VM exit */
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	/** Set vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value to the return value of 'pcpuid_from_vcpu(vcpu)',
	 *  which is the physical CPU ID associated with \a vcpu and the MSR data in the MSR Entry to be loaded
	 *  on VM exit */
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value = pcpuid_from_vcpu(vcpu);
}

static void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu);

/**
 * @brief Initialize the MSR bitmap and the VMX-transition MSR areas for the specified vCPU.
 *
 * For the specified vCPU, this interface initializes and sets up the MSR bitmap in VM execution control fields.
 * It also initializes the VMX-transition MSR areas for the specified vCPU.
 *
 * It is supposed to be called only by 'init_exec_ctrl' from 'vp-base.hv_main' module.
 *
 * @param[inout] vcpu A pointer which points to a structure representing the vCPU whose MSR bitmap and MSR areas
 *                    are to be initialized.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type 'uint8_t *'.
	 *  - msr_bitmap representing a pointer which points to the MSR bitmap associated with the specified \a vcpu,
	 *  initialized as vcpu->arch.msr_bitmap. */
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	/** Declare the following local variables of type uint32_t.
	 *  - msr representing a MSR address, not initialized.
	 *  - i representing the loop counter as array index, not initialized.
	 */
	uint32_t msr, i;
	/** Declare the following local variables of type uint64_t.
	 *  - value64 representing the host physical address of the MSR bitmap associated with the specified \a vcpu,
	 *  not initialized. */
	uint64_t value64;

	/* Trap all MSRs by default */
	/** For each 'msr' ranging from LOW_MSR_START to LOW_MSR_END [with a step of 1] */
	for (msr = LOW_MSR_START; msr <= LOW_MSR_END; msr++) {
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'msr' and the specified mode INTERCEPT_READ_WRITE.
		 *  - msr_bitmap
		 *  - msr
		 *  - INTERCEPT_READ_WRITE
		 */
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
	}

	/** For each 'msr' ranging from HIGH_MSR_START to HIGH_MSR_END [with a step of 1] */
	for (msr = HIGH_MSR_START; msr <= HIGH_MSR_END; msr++) {
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'msr' and the specified mode INTERCEPT_READ_WRITE.
		 *  - msr_bitmap
		 *  - msr
		 *  - INTERCEPT_READ_WRITE
		 */
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
	}

	/* unintercepted_msrs read_write 0_0 */
	/** For each 'i' ranging from 0H to 'NUM_UNINTERCEPTED_MSRS - 1' [with a step of 1] */
	for (i = 0U; i < NUM_UNINTERCEPTED_MSRS; i++) {
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'unintercepted_msrs[i]' and the specified mode INTERCEPT_DISABLE.
		 *  - msr_bitmap
		 *  - unintercepted_msrs[i]
		 *  - INTERCEPT_DISABLE
		 */
		enable_msr_interception(msr_bitmap, unintercepted_msrs[i], INTERCEPT_DISABLE);
	}

	/* only intercept wrmsr for MSR_IA32_TIME_STAMP_COUNTER, MSR_IA32_EFER */
	/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
	 *  according to the specified MSR IA32_TIME_STAMP_COUNTER and the specified mode INTERCEPT_WRITE.
	 *  - msr_bitmap
	 *  - MSR_IA32_TIME_STAMP_COUNTER
	 *  - INTERCEPT_WRITE
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_TIME_STAMP_COUNTER, INTERCEPT_WRITE);
	/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
	 *  according to the specified MSR IA32_EFER and the specified mode INTERCEPT_WRITE.
	 *  - msr_bitmap
	 *  - MSR_IA32_EFER
	 *  - INTERCEPT_WRITE
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_EFER, INTERCEPT_WRITE);

	/* handle cases different between safety VM and non-safety VM */
	/* Machine Check */
	/** If 'vcpu->vm' is a safety VM */
	if (is_safety_vm(vcpu->vm)) {
		/** For each 'msr' ranging from MSR_IA32_MC0_CTL2 to 'MSR_IA32_MC4_CTL2 - 1' [with a step of 1] */
		for (msr = MSR_IA32_MC0_CTL2; msr < MSR_IA32_MC4_CTL2; msr++) {
			/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
			 *  according to the specified MSR 'msr' and the specified mode INTERCEPT_DISABLE.
			 *  - msr_bitmap
			 *  - msr
			 *  - INTERCEPT_DISABLE
			 */
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}

		/** For each 'msr' ranging from MSR_IA32_MC0_CTL to 'MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS - 4'
		 *  [with a step of 4] */
		for (msr = MSR_IA32_MC0_CTL; msr < (MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS); msr = msr + 4U) {
			/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
			 *  according to the specified MSR 'msr' and the specified mode INTERCEPT_DISABLE.
			 *  - msr_bitmap
			 *  - msr
			 *  - INTERCEPT_DISABLE
			 */
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}

		/** For each 'msr' ranging from MSR_IA32_MC0_STATUS to 'MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS - 4'
		 *  [with a step of 4] */
		for (msr = MSR_IA32_MC0_STATUS; msr < (MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS); msr = msr + 4U) {
			/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
			 *  according to the specified MSR 'msr' and the specified mode INTERCEPT_DISABLE.
			 *  - msr_bitmap
			 *  - msr
			 *  - INTERCEPT_DISABLE
			 */
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}
	}

	/** Call update_msr_bitmap_x2apic_passthru with the following parameters, in order to update the MSR bitmap
	 *  associated with the specified \a vcpu to support x2APIC and LAPIC passthrough.
	 *  - vcpu */
	update_msr_bitmap_x2apic_passthru(vcpu);

	/* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
	/** Set 'value64' to the return value of 'hva2hpa(vcpu->arch.msr_bitmap)', which is the host physical address
	 *  of the MSR bitmap associated with the specified \a vcpu */
	value64 = hva2hpa(vcpu->arch.msr_bitmap);
	/** Call exec_vmwrite64 with the following parameters, in order to write 'value64' into
	 *  the 64-Bit control field 'Address of MSR bitmaps (full)' in current VMCS.
	 *  - VMX_MSR_BITMAP_FULL
	 *  - value64
	 */
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	/** Logging the following information with a log level of 6.
	 *  - value64
	 */
	pr_dbg("VMX_MSR_BITMAP: 0x%016lx ", value64);

	/* Initialize the MSR save/store area */
	/** Call init_msr_area with the following parameters, in order to initialize the VMX-transition MSR areas for
	 *  the specified \a vcpu.
	 *  - vcpu */
	init_msr_area(vcpu);
}

/**
 * @brief Bit mask of reserved bits for each 8-bit field in MSR IA32_PAT.
 *
 * For each 8-bit field, bits 7:3 are reserved.
 */
#define PAT_FIELD_RSV_BITS (0xF8UL)

/**
 * @brief Check whether the specified value indicates a valid memory type or not.
 *
 * Check whether the specified value indicates a valid memory type or not. The value is associated with the
 * MSR IA32_PAT.
 *
 * According to Chapter 11.12.2, Vol. 3, SDM, the MSR IA32_PAT contains eight page attribute fields.
 * For each 8-bit field, bits 7:3 are reserved and must be set to all 0s.
 * For each 8-bit field, bits 2:0 are used to specify a memory type, and the value set to these three bits must not be
 * 2H or 3H.
 *
 * It is supposed to be called only by 'write_pat_msr'.
 *
 * @param[in] x The specified value to be checked.
 *
 * @return A Boolean value implying whether the specified value indicates a valid memory type or not.
 *
 * @retval true The specified value does not indicate a valid memory type.
 * @retval false The specified value indicates a valid memory type.
 *
 * @pre (x & (~FFH)) == 0
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_pat_mem_type_invalid(uint64_t x)
{
	/** Return 'true' if any one of the following conditions is satisfied.
	 *  - Bits 7:3 of 'x' is not 0H.
	 *  - Bits 2:0 of 'x' is 2H.
	 *  - Bits 2:0 of 'x' is 3H.
	 *  Otherwise, return 'false'. */
	return (((x & PAT_FIELD_RSV_BITS) != 0UL) || ((x & 0x6UL) == 0x2UL));
}

/**
 * @brief Emulate the write operation into the MSR IA32_PAT for guest.
 *
 * Emulate the write operation into the MSR IA32_PAT for guest.
 *
 * If the specified value is valid, the MSR IA32_PAT associated with the specified vCPU would be updated,
 * and the 64-Bit guest state field IA32_PAT in current VMCS would be updated as well if the
 * Cache Disable Bit (Bit 30) of the CR0 associated with the specified vCPU is 0H.
 *
 * If the specified value is invalid, '-EINVAL' would be returned to indicate that the attempted operation is invalid.
 *
 * It is supposed to be called only by 'wrmsr_vmexit_handler'.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write the specified value into its MSR IA32_PAT.
 * @param[in] value The specified value that the vCPU attempts to write into guest MSR IA32_PAT.
 *
 * @return A status code indicating whether the specified value is valid or not to be written into guest MSR IA32_PAT.
 *
 * @retval 0 The specified value is valid to be written into guest MSR IA32_PAT.
 * @retval -EINVAL The specified value is not valid to be written into guest MSR IA32_PAT.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t write_pat_msr(struct acrn_vcpu *vcpu, uint64_t value)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type uint64_t.
	 *  - field representing a 8-bit field in \a value, not initialized. */
	uint64_t field;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the validity of \a value, initialized as 0. */
	int32_t ret = 0;

	/** For each 'i' ranging from 0H to 7H [with a step of 1] */
	for (i = 0U; i < 8U; i++) {
		/** Set 'field' to the (i+1)-th 8-bit field in \a value */
		field = (value >> (i * 8U)) & 0xffUL;
		/** If 'field' indicates an invalid memory type */
		if (is_pat_mem_type_invalid(field)) {
			/** Logging the following information with a log level of 3.
			 *  - value
			 */
			pr_err("invalid guest IA32_PAT: 0x%016lx", value);
			/** Set 'ret' to -EINVAL, which indicates that \a value is not valid to be written into guest
			 *  MSR IA32_PAT */
			ret = -EINVAL;
			/** Terminate the loop */
			break;
		}
	}

	/** If 'ret' is equal to 0, which indicates that \a value is valid to be written into guest MSR IA32_PAT */
	if (ret == 0) {
		/** Call vcpu_set_guest_msr with the following parameters, in order to write \a value into
		 *  the MSR IA32_PAT associated with \a vcpu.
		 *  - vcpu
		 *  - MSR_IA32_PAT
		 *  - value
		 */
		vcpu_set_guest_msr(vcpu, MSR_IA32_PAT, value);

		/*
		 * If context->cr0.CD is set, we defer any further requests to write
		 * guest's IA32_PAT, until the time when guest's CR0.CD is being cleared
		 */
		/** If the Cache Disable Bit (Bit 30) of the CR0 associated with \a vcpu is 0H */
		if ((vcpu_get_cr0(vcpu) & CR0_CD) == 0UL) {
			/** Call exec_vmwrite64 with the following parameters, in order to write \a value into
			 *  the 64-Bit guest state field 'Guest IA32_PAT (full)' in current VMCS.
			 *  - VMX_GUEST_IA32_PAT_FULL
			 *  - value
			 */
			exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value);
		}
	}

	/** Return 'ret' */
	return ret;
}

/**
 * @brief Check whether the specified MSR is a valid IA32_MCi_CTL2 MSR.
 *
 * Check whether the specified MSR is a valid IA32_MCi_CTL2 MSR.
 * If it is valid, it belongs to IA32_MCi_CTL2 MSRs and it is implemented on the physical platform.
 *
 * It is supposed to be called by 'rdmsr_vmexit_handler' and 'wrmsr_vmexit_handler'.
 *
 * @param[in] msr The specified MSR to be checked.
 *
 * @return A Boolean value indicating whether the specified MSR is a valid IA32_MCi_CTL2 MSR.
 *
 * @retval true The specified MSR is a valid IA32_MCi_CTL2 MSR.
 * @retval false The specified MSR is not a valid IA32_MCi_CTL2 MSR.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_mc_ctl2_msr(uint32_t msr)
{
	/** Return 'true' if \a msr is in the range MSR_IA32_MC0_CTL2 to 'MSR_IA32_MC0_CTL2 + NUM_MC_BANKS - 1'.
	 *  Otherwise, return 'false'. */
	return ((msr >= MSR_IA32_MC0_CTL2) && (msr < (MSR_IA32_MC0_CTL2 + NUM_MC_BANKS)));
}

/**
 * @brief Check whether the specified MSR is a valid IA32_MCi_CTL MSR.
 *
 * Check whether the specified MSR is a valid IA32_MCi_CTL MSR.
 * If it is valid, it belongs to IA32_MCi_CTL MSRs and it is implemented on the physical platform.
 *
 * It is supposed to be called by 'rdmsr_vmexit_handler' and 'wrmsr_vmexit_handler'.
 *
 * @param[in] msr The specified MSR to be checked.
 *
 * @return A Boolean value indicating whether the specified MSR is a valid IA32_MCi_CTL MSR.
 *
 * @retval true The specified MSR is a valid IA32_MCi_CTL MSR.
 * @retval false The specified MSR is not a valid IA32_MCi_CTL MSR.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_mc_ctl_msr(uint32_t msr)
{
	/** Return 'true' if \a msr is in the range MSR_IA32_MC0_CTL to 'MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS - 4'
	 *  and the remainder after division of \a msr by 4 is equal to 0H.
	 *  Otherwise, return 'false'. */
	return ((msr >= MSR_IA32_MC0_CTL) && (msr < (MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS)) && ((msr % 4U) == 0U));
}

/**
 * @brief Check whether the specified MSR is a valid IA32_MCi_STATUS MSR.
 *
 * Check whether the specified MSR is a valid IA32_MCi_STATUS MSR.
 * If it is valid, it belongs to IA32_MCi_STATUS MSRs and it is implemented on the physical platform.
 *
 * It is supposed to be called by 'rdmsr_vmexit_handler' and 'wrmsr_vmexit_handler'.
 *
 * @param[in] msr The specified MSR to be checked.
 *
 * @return A Boolean value indicating whether the specified MSR is a valid IA32_MCi_STATUS MSR.
 *
 * @retval true The specified MSR is a valid IA32_MCi_STATUS MSR.
 * @retval false The specified MSR is not a valid IA32_MCi_STATUS MSR.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_mc_status_msr(uint32_t msr)
{
	/** Return 'true' if \a msr is in the range MSR_IA32_MC0_STATUS to 'MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS - 4'
	 *  and the remainder after division of \a msr by 4 is equal to 1H.
	 *  Otherwise, return 'false'. */
	return ((msr >= MSR_IA32_MC0_STATUS) && (msr < (MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS))
			&& ((msr % 4U) == 1U));
}

/**
 * @brief Handle the VM exit caused by a RDMSR instruction executing from guest software.
 *
 * Handle the VM exit caused by a RDMSR instruction executing from guest software and return a status code indicating
 * whether further handling operations are required from the caller.
 * If the status code is 0, it indicates that no further operation is required.
 * If the status code is negative, it indicates that the caller needs to inject #GP(0) to guest software.
 *
 * It is supposed to be used as a callback in 'vmexit_handler' from 'vp-base.hv_main' module.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to execute a RDMSR instruction.
 *
 * @return A status code indicating whether further handling operations are required.
 *
 * @retval 0 The RDMSR instruction executing from guest software would not cause an exception being generated.
 * @retval negative The RDMSR instruction executing from guest software would cause an exception being generated.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 * @remark This API shall be called after init_msr_emulation has been called on \a vcpu once on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
int32_t rdmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type int32_t.
	 *  - err representing a status code, initialized as 0. */
	int32_t err = 0;
	/** Declare the following local variables of type uint32_t.
	 *  - msr representing the MSR to be read from, not initialized. */
	uint32_t msr;
	/** Declare the following local variables of type uint64_t.
	 *  - v representing the contents of the MSR to be read from, initialized as 0H. */
	uint64_t v = 0UL;

	/* Read the msr value */
	/** Set 'msr' to the return value of '(uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX)',
	 *  which is the contents of ECX register associated with \a vcpu and it specifies the MSR to be read from. */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/** Depending on 'msr' */
	switch (msr) {
	/** 'msr' is MSR_IA32_TSC_DEADLINE */
	case MSR_IA32_TSC_DEADLINE: {
		/** Set 'v' to the return value of 'vlapic_get_tsc_deadline_msr(vcpu_vlapic(vcpu))',
		 *  which specifies the contents of the MSR IA32_TSC_DEADLINE associated with \a vcpu */
		v = vlapic_get_tsc_deadline_msr(vcpu_vlapic(vcpu));
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_TSC_ADJUST */
	case MSR_IA32_TSC_ADJUST: {
		/** Set 'v' to the return value of 'vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST)',
		 *  which specifies the contents of the MSR IA32_TSC_ADJUST associated with \a vcpu */
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_BIOS_SIGN_ID */
	case MSR_IA32_BIOS_SIGN_ID: {
		/** Set 'v' to the return value of 'get_microcode_version()',
		 *  which specifies the contents of the guest MSR IA32_BIOS_SIGN_ID */
		v = get_microcode_version();
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_PAT */
	case MSR_IA32_PAT: {
		/*
		 * note: if run_ctx->cr0.CD is set, the actual value in guest's
		 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
		 * the saved value guest_msrs[MSR_IA32_PAT]
		 */
		/** Set 'v' to the return value of 'vcpu_get_guest_msr(vcpu, MSR_IA32_PAT)',
		 *  which specifies the contents of the MSR IA32_PAT associated with \a vcpu */
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_PAT);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_APIC_BASE */
	case MSR_IA32_APIC_BASE: {
		/** Set 'v' to the return value of 'vlapic_get_apicbase(vcpu_vlapic(vcpu))',
		 *  which specifies the contents of the MSR IA32_APIC_BASE associated with \a vcpu */
		v = vlapic_get_apicbase(vcpu_vlapic(vcpu));
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_FEATURE_CONTROL */
	case MSR_IA32_FEATURE_CONTROL: {
		/** Set 'v' to MSR_IA32_FEATURE_CONTROL_LOCK */
		v = MSR_IA32_FEATURE_CONTROL_LOCK;
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_MISC_ENABLE */
	case MSR_IA32_MISC_ENABLE: {
		/** Set 'v' to the return value of 'vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE)',
		 *  which specifies the contents of the MSR IA32_MISC_ENABLE associated with \a vcpu */
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_SPEC_CTRL */
	case MSR_IA32_SPEC_CTRL: {
		/** Set 'v' to the contents of the native guest MSR IA32_SPEC_CTRL with STIBP bit being cleared */
		v = msr_read(MSR_IA32_SPEC_CTRL) & (~MSR_IA32_SPEC_CTRL_STIBP);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_MONITOR_FILTER_SIZE */
	case MSR_IA32_MONITOR_FILTER_SIZE: {
		/** Set 'v' to 0H */
		v = 0UL;
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_MCG_CAP */
	case MSR_IA32_MCG_CAP: {
		/** If 'vcpu->vm' is a safety VM */
		if (is_safety_vm(vcpu->vm)) {
			/** Set 'v' to MCG_CAP_FOR_SAFETY_VM */
			v = MCG_CAP_FOR_SAFETY_VM;
		} else {
			/** Set 'v' to 0H */
			v = 0UL;
		}
		/** End of case */
		break;
	}

	/** Otherwise */
	default: {
		/** If 'msr' is a valid IA32_MCi_CTL2 MSR, or IA32_MCi_CTL MSR, or IA32_MCi_STATUS MSR */
		if (is_mc_ctl2_msr(msr) || is_mc_ctl_msr(msr) || is_mc_status_msr(msr)) {
			/* handle Machine Check related MSRs */
			/** If 'vcpu->vm' is a safety VM */
			if (is_safety_vm(vcpu->vm)) {
				/** If 'msr' is in the range MSR_IA32_MC4_CTL2 to MSR_IA32_MC9_CTL2 */
				if ((msr >= MSR_IA32_MC4_CTL2) && (msr <= MSR_IA32_MC9_CTL2)) {
					/** Set 'v' to 0H */
					v = 0UL;
				}
				/* Otherwise, it's not trapped. */
			} else {
				/** Set 'err' to -EACCES, which indicates that an exception needs to be injected
				 *  to guest software by the caller */
				err = -EACCES;
			}
		/** If 'msr' belongs to x2APIC MSRs */
		} else if (is_x2apic_msr(msr)) {
			/** Set 'err' to the return value of 'vlapic_x2apic_read(vcpu, msr, &v)',
			 *  which indicates whether an exception needs to be injected to guest software.
			 *  If no exception is expected, the contents of the specified 'msr' associated with \a vcpu
			 *  would be specified as 'v'.
			 */
			err = vlapic_x2apic_read(vcpu, msr, &v);
		} else {
			/** Logging the following information with a log level of 4.
			 *  - __func__
			 *  - vcpu->vm->vm_id
			 *  - vcpu->vcpu_id
			 *  - msr
			 */
			pr_warn("%s(): vm%d vcpu%d reading MSR %lx not supported", __func__, vcpu->vm->vm_id,
				vcpu->vcpu_id, msr);
			/** Set 'err' to -EACCES, which indicates that an exception needs to be injected
			 *  to guest software by the caller */
			err = -EACCES;
			/** Set 'v' to 0H */
			v = 0UL;
		}
		/** End of case */
		break;
	}
	}

	/** If 'err' is equal to 0, which indicates that no exception needs to be injected to guest software */
	if (err == 0) {
		/* Store the MSR contents in RAX and RDX */
		/** Call vcpu_set_gpreg with the following parameters, in order to write low 32-bits of 'v'
		 *  into EAX register associated with \a vcpu.
		 *  - vcpu
		 *  - CPU_REG_RAX
		 *  - v & 0xffffffffU
		 */
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, v & 0xffffffffU);
		/** Call vcpu_set_gpreg with the following parameters, in order to write high 32-bits of 'v'
		 *  into EDX register associated with \a vcpu.
		 *  - vcpu
		 *  - CPU_REG_RDX
		 *  - v >> 32U
		 */
		vcpu_set_gpreg(vcpu, CPU_REG_RDX, v >> 32U);
	}

	/** Call TRACE_2L with the following parameters, in order to trace the VM exit caused by a RDMSR instruction
	 *  executing from guest software.
	 *  - TRACE_VMEXIT_RDMSR
	 *  - msr
	 *  - v
	 */
	TRACE_2L(TRACE_VMEXIT_RDMSR, msr, v);

	/** Return 'err' */
	return err;
}

/*
 * If VMX_TSC_OFFSET_FULL is 0, no need to trap the write of IA32_TSC_DEADLINE because there is
 * no offset between vTSC and pTSC, in this case, only write to vTSC_ADJUST is trapped.
 */
/**
 * @brief Update system state according to the changes of the interception associated with the MSR IA32_TSC_DEADLINE.
 *
 * Update system state according to the changes of the interception associated with the MSR IA32_TSC_DEADLINE.
 *
 * If a MSR is not intercepted, it means that RDMSR and WRMSR instructions executed from guest associated with this MSR
 * would not cause VM exit.
 * If a MSR is intercepted, it means that either RDMSR or WRMSR instruction executed from guest associated with this MSR
 * would cause VM exit.
 *
 * If the interception associated with the MSR IA32_TSC_DEADLINE is to be changed, the following states might be
 * changed accordingly: the MSR bitmap associated with the specified vCPU, the contents of the MSR IA32_TSC_DEADLINE
 * associated with the specified vCPU, and the contents of the native MSR IA32_TSC_DEADLINE.
 *
 * It is supposed to be called by 'set_guest_tsc', 'set_guest_tsc_adjust', and 'update_msr_bitmap_x2apic_passthru'.
 *
 * @param[inout] vcpu A pointer which points to a structure representing the vCPU whose states are to be updated.
 * @param[in] interception A Boolean value indicating whether hypervisor needs to intercept the MSR IA32_TSC_DEADLINE
 *            or not.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void set_tsc_msr_interception(struct acrn_vcpu *vcpu, bool interception)
{
	/** Declare the following local variables of type 'uint8_t *'.
	 *  - msr_bitmap representing a pointer to the MSR bitmap associated with \a vcpu,
	 *  initialized as vcpu->arch.msr_bitmap. */
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	/** Declare the following local variables of type bool.
	 *  - is_intercepted representing whether the MSR IA32_TSC_DEADLINE is intercepted or not at this moment,
	 *  initialized as 'true' if the corresponding bit for the MSR IA32_TSC_DEADLINE in current MSR read bitmap is
	 *  1H, initialized as 'false' if that bit is 0H. */
	bool is_intercepted =
		((msr_bitmap[MSR_IA32_TSC_DEADLINE >> 3U] & (1U << (MSR_IA32_TSC_DEADLINE & 0x7U))) != 0U);

	/** If the MSR IA32_TSC_DEADLINE is intercepted at this moment and it is requested to be not intercepted */
	if (!interception && is_intercepted) {
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'MSR_IA32_TSC_DEADLINE' and the specified mode INTERCEPT_DISABLE.
		 *  - msr_bitmap
		 *  - MSR_IA32_TSC_DEADLINE
		 *  - INTERCEPT_DISABLE
		 */
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_DISABLE);
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'MSR_IA32_TSC_ADJUST' and the specified mode INTERCEPT_WRITE.
		 *  - msr_bitmap
		 *  - MSR_IA32_TSC_ADJUST
		 *  - INTERCEPT_WRITE
		 */
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_WRITE);
		/* If the timer hasn't expired, sync virtual TSC_DEADLINE to physical TSC_DEADLINE, to make the guest
		 * read the same tsc_deadline as it writes. This may change when the timer actually trigger. If the
		 * timer has expired, write 0 to the virtual TSC_DEADLINE.
		 */
		/** If the contents of the native MSR IA32_TSC_DEADLINE is not 0H */
		if (msr_read(MSR_IA32_TSC_DEADLINE) != 0UL) {
			/** Call msr_write with the following parameters, in order to write the contents of the
			 *  guest MSR IA32_TSC_DEADLINE into the native MSR IA32_TSC_DEADLINE.
			 *  - MSR_IA32_TSC_DEADLINE
			 *  - vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE)
			 */
			msr_write(MSR_IA32_TSC_DEADLINE, vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE));
		} else {
			/** Call vcpu_set_guest_msr with the following parameters, in order to write 0H into
			 *  the MSR IA32_TSC_DEADLINE associated with \a vcpu.
			 *  - vcpu
			 *  - MSR_IA32_TSC_DEADLINE
			 *  - 0H
			 */
			vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, 0UL);
		}
	/** If the MSR IA32_TSC_DEADLINE is not intercepted at this moment and it is requested to be intercepted */
	} else if (interception && !is_intercepted) {
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'MSR_IA32_TSC_DEADLINE' and the specified mode INTERCEPT_READ_WRITE.
		 *  - msr_bitmap
		 *  - MSR_IA32_TSC_DEADLINE
		 *  - INTERCEPT_READ_WRITE
		 */
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_READ_WRITE);
		/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
		 *  according to the specified MSR 'MSR_IA32_TSC_ADJUST' and the specified mode INTERCEPT_READ_WRITE.
		 *  - msr_bitmap
		 *  - MSR_IA32_TSC_ADJUST
		 *  - INTERCEPT_READ_WRITE
		 */
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_READ_WRITE);
		/* sync physical TSC_DEADLINE to virtual TSC_DEADLINE */
		/** Call vcpu_set_guest_msr with the following parameters, in order to write the contents of the
		 *  native MSR IA32_TSC_DEADLINE into the MSR IA32_TSC_DEADLINE associated with \a vcpu.
		 *  - vcpu
		 *  - MSR_IA32_TSC_DEADLINE
		 *  - msr_read(MSR_IA32_TSC_DEADLINE)
		 */
		vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, msr_read(MSR_IA32_TSC_DEADLINE));
	} else {
		/* Do nothing */
	}
}

/*
 * Intel SDM 17.17.3: If an execution of WRMSR to the
 * IA32_TIME_STAMP_COUNTER MSR adds (or subtracts) value X from the
 * TSC, the logical processor also adds (or subtracts) value X from
 * the IA32_TSC_ADJUST MSR.
 *
 * So, here we should update VMCS.OFFSET and vAdjust accordingly.
 *   - VMCS.OFFSET = vTSC - pTSC
 *   - vAdjust += VMCS.OFFSET's delta
 */

/**
 * @brief Emulate the write operation into the MSR IA32_TIME_STAMP_COUNTER for guest.
 *
 * Emulate the write operation into the MSR IA32_TIME_STAMP_COUNTER for guest.
 *
 * It is supposed to be called only by 'wrmsr_vmexit_handler'.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write the specified value into its
 *                    MSR IA32_TIME_STAMP_COUNTER.
 * @param[in] guest_tsc The specified value that the vCPU attempts to write into its MSR IA32_TIME_STAMP_COUNTER.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void set_guest_tsc(struct acrn_vcpu *vcpu, uint64_t guest_tsc)
{
	/** Declare the following local variables of type uint64_t.
	 *  - tsc_delta representing the delta between the TSC value to be updated and current TSC value,
	 *  not initialized.
	 *  - tsc_offset_delta representing the delta between the TSC offset to be updated and the TSC offset in
	 *  current VMCS, not initialized.
	 *  - tsc_adjust representing current contents of the MSR IA32_TSC_ADJUST associated with \a vcpu,
	 *  not initialized.
	 */
	uint64_t tsc_delta, tsc_offset_delta, tsc_adjust;

	/** Set 'tsc_delta' to the value of subtracting current TSC value from \a guest_tsc,
	 *  this value also specifies the TSC offset to be updated in VMCS */
	tsc_delta = guest_tsc - rdtsc();

	/* the delta between new and existing TSC_OFFSET */
	/** Set 'tsc_offset_delta' to the value of subtracting the TSC offset in current VMCS from 'tsc_delta' */
	tsc_offset_delta = tsc_delta - exec_vmread64(VMX_TSC_OFFSET_FULL);

	/* apply this delta to TSC_ADJUST */
	/** Set 'tsc_adjust' to the return value of 'vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST)',
	 *  which specifies the contents of the MSR IA32_TSC_ADJUST associated with \a vcpu */
	tsc_adjust = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
	/** Call vcpu_set_guest_msr with the following parameters, in order to write 'tsc_adjust + tsc_offset_delta'
	 *  into the MSR IA32_TSC_ADJUST associated with \a vcpu.
	 *  - vcpu
	 *  - MSR_IA32_TSC_ADJUST
	 *  - tsc_adjust + tsc_offset_delta
	 */
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust + tsc_offset_delta);

	/* write to VMCS because rdtsc and rdtscp are not intercepted */
	/** Call exec_vmwrite64 with the following parameters, in order to write 'tsc_delta' into
	 *  the 64-Bit control field 'TSC offset (full)' in current VMCS.
	 *  - VMX_TSC_OFFSET_FULL
	 *  - tsc_delta
	 */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_delta);

	/** Call set_tsc_msr_interception with the following parameters, in order to
	 *  update system state according to the changes of the interception associated with the MSR IA32_TSC_DEADLINE.
	 *  MSR IA32_TSC_DEADLINE needs to be intercepted if 'tsc_delta' is not equal to 0H.
	 *  - vcpu
	 *  - tsc_delta != 0H
	 */
	set_tsc_msr_interception(vcpu, tsc_delta != 0UL);
}

/*
 * The policy of vART is that software in native can run in VM too. And in native side,
 * the relationship between the ART hardware and TSC is:
 *
 *   pTSC = (pART * M) / N + pAdjust
 *
 * The vART solution is:
 *   - Present the ART capability to guest through CPUID leaf
 *     15H for M/N which identical to the physical values.
 *   - PT devices see the pART (vART = pART).
 *   - Guest expect: vTSC = vART * M / N + vAdjust.
 *   - VMCS.OFFSET = vTSC - pTSC = vAdjust - pAdjust.
 *
 * So to support vART, we should do the following:
 *   1. if vAdjust and vTSC are changed by guest, we should change
 *      VMCS.OFFSET accordingly.
 *   2. Make the assumption that the pAjust is never touched by ACRN.
 */

/*
 * Intel SDM 17.17.3: "If an execution of WRMSR to the IA32_TSC_ADJUST
 * MSR adds (or subtracts) value X from that MSR, the logical
 * processor also adds (or subtracts) value X from the TSC."
 *
 * So, here we should update VMCS.OFFSET and vAdjust accordingly.
 *   - VMCS.OFFSET += vAdjust's delta
 *   - vAdjust = new vAdjust set by guest
 */

/**
 * @brief Emulate the write operation into the MSR IA32_TSC_ADJUST for guest.
 *
 * Emulate the write operation into the MSR IA32_TSC_ADJUST for guest.
 *
 * It is supposed to be called only by 'wrmsr_vmexit_handler'.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write the specified value into its
 *                    MSR IA32_TSC_ADJUST.
 * @param[in] tsc_adjust The specified value that the vCPU attempts to write into its MSR IA32_TSC_ADJUST.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void set_guest_tsc_adjust(struct acrn_vcpu *vcpu, uint64_t tsc_adjust)
{
	/** Declare the following local variables of type uint64_t.
	 *  - tsc_offset representing the TSC offset in current VMCS, not initialized.
	 *  - tsc_adjust_delta representing the delta between \a tsc_adjust and the current contents of the
	 *  MSR IA32_TSC_ADJUST associated with \a vcpu, not initialized.
	 */
	uint64_t tsc_offset, tsc_adjust_delta;

	/* delta of the new and existing IA32_TSC_ADJUST */
	/** Set 'tsc_adjust_delta' to the value of subtracting the current contents of the guest MSR IA32_TSC_ADJUST
	 *  from \a tsc_adjust */
	tsc_adjust_delta = tsc_adjust - vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);

	/* apply this delta to existing TSC_OFFSET */
	/** Set tsc_offset to the return value of 'exec_vmread64(VMX_TSC_OFFSET_FULL)',
	 *  which indicates the TSC offset in current VMCS */
	tsc_offset = exec_vmread64(VMX_TSC_OFFSET_FULL);
	/** Call exec_vmwrite64 with the following parameters, in order to write 'tsc_offset + tsc_adjust_delta' into
	 *  the 64-Bit control field 'TSC offset (full)' in current VMCS.
	 *  - VMX_TSC_OFFSET_FULL
	 *  - tsc_offset + tsc_adjust_delta
	 */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_offset + tsc_adjust_delta);

	/* IA32_TSC_ADJUST is supposed to carry the value it's written to */
	/** Call vcpu_set_guest_msr with the following parameters, in order to write \a tsc_adjust
	 *  into the MSR IA32_TSC_ADJUST associated with \a vcpu.
	 *  - vcpu
	 *  - MSR_IA32_TSC_ADJUST
	 *  - tsc_adjust
	 */
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust);

	/** Call set_tsc_msr_interception with the following parameters, in order to
	 *  update system state according to the changes of the interception associated with the MSR IA32_TSC_DEADLINE.
	 *  MSR IA32_TSC_DEADLINE needs to be intercepted if 'tsc_offset + tsc_adjust_delta' is not equal to 0H.
	 *  Overflow of 'tsc_offset + tsc_adjust_delta' is acceptable.
	 *  - vcpu
	 *  - (tsc_offset + tsc_adjust_delta) != 0H
	 */
	set_tsc_msr_interception(vcpu, (tsc_offset + tsc_adjust_delta) != 0UL);
}

/**
 * @brief Emulate the write operation into the MSR IA32_MISC_ENABLE for guest.
 *
 * Emulate the write operation into the MSR IA32_MISC_ENABLE for guest.
 *
 * It is supposed to be called only by 'wrmsr_vmexit_handler'.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write the specified value into its
 *                    MSR IA32_MISC_ENABLE.
 * @param[in] v The specified value that the vCPU attempts to write into its MSR IA32_MISC_ENABLE.
 *
 * @return A status code indicating whether the specified value is valid or not to be written into guest
 *         MSR IA32_MISC_ENABLE.
 *
 * @retval 0 The specified value is valid to be written into guest MSR IA32_MISC_ENABLE.
 * @retval -EACCES The specified value is not valid to be written into guest MSR IA32_MISC_ENABLE.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t set_guest_ia32_misc_enable(struct acrn_vcpu *vcpu, uint64_t v)
{
	/** Declare the following local variables of type uint64_t.
	 *  - guest_ia32_misc_enable representing the contents of the guest MSR IA32_MISC_ENABLE,
	 *  not initialized.
	 *  - guest_efer representing the contents of the guest MSR IA32_EFER,
	 *  not initialized.
	 *  - misc_enable_changed_bits representing the bits (in guest MSR IA32_MISC_ENABLE) that the specified
	 *  vCPU attempts to change, not initialized.
	 */
	uint64_t guest_ia32_misc_enable, guest_efer, misc_enable_changed_bits;
	/** Declare the following local variables of type int32_t.
	 *  - err representing the validity of \a v, initialized as 0. */
	int32_t err = 0;

	/** Set 'guest_ia32_misc_enable' to the return value of 'vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE)',
	 *  which specifies the current contents of the MSR IA32_MISC_ENABLE associated with \a vcpu */
	guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	/** Set 'misc_enable_changed_bits' to the bitwise exclusive OR of two operands: v and guest_ia32_misc_enable,
	 *  which indicates the different bits between \a v and 'guest_ia32_misc_enable' */
	misc_enable_changed_bits = v ^ guest_ia32_misc_enable;

	/** If the specified vCPU attempts to change the value of the reserved bits in guest MSR IA32_MISC_ENABLE,
	 *  where reserved bits are specified in (~MSR_IA32_MISC_ENABLE_MASK) */
	if ((misc_enable_changed_bits & (~MSR_IA32_MISC_ENABLE_MASK)) != 0UL) {
		/** Set 'err' to -EACCES, which indicates that \a v is not valid to be written into
		 *  guest MSR IA32_MISC_ENABLE */
		err = -EACCES;
	} else {
		/* Write value of bit 22 and bit 34 in specified "v" to guest IA32_MISC_ENABLE [bit 22] and [bit 34] */
		/** If the specified vCPU attempts to change the value of any non-reserved bit in
		 *  guest MSR IA32_MISC_ENABLE, where non-reserved bits are specified in MSR_IA32_MISC_ENABLE_MASK */
		if ((misc_enable_changed_bits & MSR_IA32_MISC_ENABLE_MASK) != 0UL) {
			/** Clear the non-reserved bits in 'guest_ia32_misc_enable' */
			guest_ia32_misc_enable &= ~MSR_IA32_MISC_ENABLE_MASK;
			/** Set the non-reserved bits in 'guest_ia32_misc_enable' to the non-reserved bits in \a v */
			guest_ia32_misc_enable |= v & MSR_IA32_MISC_ENABLE_MASK;
			/** Call vcpu_set_guest_msr with the following parameters, in order to write
			 *  'guest_ia32_misc_enable' into the MSR IA32_MISC_ENABLE associated with \a vcpu.
			 *  - vcpu
			 *  - MSR_IA32_MISC_ENABLE
			 *  - guest_ia32_misc_enable
			 */
			vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, guest_ia32_misc_enable);
		}

		/* According to SDM Vol4 2.1 & Vol 3A 4.1.4,
		 * EFER.NXE should be cleared if guest disable XD in IA32_MISC_ENABLE
		 */
		/** If the 'XD Bit Disable' Bit (Bit 34) in \a v is 1H */
		if ((v & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
			/** Set 'guest_efer' to to the return value of 'vcpu_get_efer(vcpu)',
			 *  which specifies the contents of the MSR IA32_EFER associated with \a vcpu */
			guest_efer = vcpu_get_efer(vcpu);

			/** If the 'Execute Disable Bit Enable' Bit (Bit 11) in 'guest_efer' is 1H */
			if ((guest_efer & MSR_IA32_EFER_NXE_BIT) != 0UL) {
				/** Call vcpu_set_efer with the following parameters, in order to clear the
				 *  'Execute Disable Bit Enable' Bit (Bit 11) in the MSR IA32_EFER associated with
				 *  \a vcpu.
				 *  - vcpu
				 *  - guest_efer & ~MSR_IA32_EFER_NXE_BIT
				 */
				vcpu_set_efer(vcpu, guest_efer & ~MSR_IA32_EFER_NXE_BIT);

				/** Call vcpu_make_request with the following parameters, in order to
				 *  flush TLB entries and paging structure cache entries applicable to \a vcpu.
				 *  - vcpu
				 *  - ACRN_REQUEST_EPT_FLUSH
				 */
				vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
			}
		}
	}

	/** Return 'err' */
	return err;
}

/**
 * @brief Emulate the write operation into the MSR IA32_EFER for guest.
 *
 * Emulate the write operation into the MSR IA32_EFER for guest.
 *
 * It is supposed to be called only by 'wrmsr_vmexit_handler'.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to write the specified value into its
 *                    MSR IA32_EFER.
 * @param[in] value The specified value that the vCPU attempts to write into its MSR IA32_EFER.
 *
 * @return A status code indicating whether the specified value is valid or not to be written into guest
 *         MSR IA32_EFER.
 *
 * @retval 0 The specified value is valid to be written into guest MSR IA32_EFER.
 * @retval -EACCES The specified value is not valid to be written into guest MSR IA32_EFER.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static int32_t write_efer_msr(struct acrn_vcpu *vcpu, uint64_t value)
{
	/** Declare the following local variables of type int32_t.
	 *  - err representing the validity of \a value, initialized as 0. */
	int32_t err = 0;
	/** Declare the following local variables of type uint32_t.
	 *  - cpuid_xd_feat_flag representing a flag showing the 'Execute Disable' Bit (Bit 20) in
	 *  guest CPUID.80000001H:EDX (with other bits cleared), not initialized.
	 *  - eax representing the contents of EAX register, not initialized.
	 *  - ebx representing the contents of EBX register, not initialized.
	 *  - ecx representing the contents of ECX register, not initialized.
	 *  - edx representing the contents of EDX register, not initialized.
	 */
	uint32_t cpuid_xd_feat_flag, eax, ebx, ecx, edx;
	/** Declare the following local variables of type uint64_t.
	 *  - guest_efer representing the contents of the guest MSR IA32_EFER,
	 *  not initialized.
	 *  - efer_changed_bits representing the bits (in guest MSR IA32_EFER) that the specified
	 *  vCPU attempts to change, not initialized.
	 *  - new_val representing the final value to be written into guest MSR IA32_EFER,
	 *  not initialized.
	 */
	uint64_t guest_efer, efer_changed_bits, new_val;

	/** Set 'guest_efer' to the return value of 'vcpu_get_efer(vcpu)',
	 *  which specifies the current contents of the MSR IA32_EFER associated with \a vcpu */
	guest_efer = vcpu_get_efer(vcpu);
	/** Set 'efer_changed_bits' to the bitwise exclusive OR of two operands: guest_efer and value,
	 *  which indicates the different bits between 'guest_efer' and \a value */
	efer_changed_bits = guest_efer ^ value;

	/** If the specified vCPU attempts to change the value of the reserved bits in guest MSR IA32_EFER,
	 *  reserved bits are specified in (~MSR_IA32_EFER_MASK) */
	if ((efer_changed_bits & (~MSR_IA32_EFER_MASK)) != 0UL) {
		/*
		 * Handle invalid write to Reserved bits.
		 *
		 * Modifying Reserved bits causes a general-protection exception (#GP(0)).
		 */
		/** Set 'err' to -EACCES, which indicates that \a value is not valid to be written into
		 *  guest MSR IA32_EFER */
		err = -EACCES;
	/** If the specified vCPU attempts to modify the 'IA-32e Mode Enable' Bit (Bit 8) in its MSR IA32_EFER
	 *  while paging is enabled */
	} else if (((efer_changed_bits & MSR_IA32_EFER_LME_BIT) != 0UL) && (is_paging_enabled(vcpu))) {
		/*
		 * Handle invalid write to LME bit.
		 *
		 * Modifying LME bit while paging is enabled causes a general-protection exception (#GP(0)).
		 */
		/** Set 'err' to -EACCES, which indicates that \a value is not valid to be written into
		 *  guest MSR IA32_EFER */
		err = -EACCES;
	} else {
		/* Get guest XD Bit extended feature flag (CPUID.80000001H:EDX[20]). */
		/** Set 'eax' to CPUID_EXTEND_FUNCTION_1 */
		eax = CPUID_EXTEND_FUNCTION_1;
		/** Call guest_cpuid with the following parameters, in order to get the emulated processor information
		 *  for CPUID.CPUID_EXTEND_FUNCTION_1 and fill the emulated processor information into the output
		 *  parameters.
		 *  - vcpu
		 *  - &eax
		 *  - &ebx
		 *  - &ecx
		 *  - &edx
		 */
		guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
		/** Set 'cpuid_xd_feat_flag' to the result of bitwise AND 'edx' by CPUID_EDX_XD_BIT_AVIL */
		cpuid_xd_feat_flag = edx & CPUID_EDX_XD_BIT_AVIL;

		/** If the 'Execute Disable' Bit (Bit 20) in guest CPUID.80000001H:EDX is 0H and
		 *  the 'Execute Disable Bit Enable' Bit (Bit 11) in \a value is 1H */
		if ((cpuid_xd_feat_flag == 0U) && ((value & MSR_IA32_EFER_NXE_BIT) != 0UL)) {
			/*
			 * Handle invalid write to NXE bit.
			 *
			 * Writing NXE bit to 1 when the XD Bit extended feature flag is set to 0
			 * causes a general-protection exception (#GP(0)).
			 */
			/** Set 'err' to -EACCES, which indicates that \a value is not valid to be written into
			 *  guest MSR IA32_EFER */
			err = -EACCES;
		} else {
			/** Set 'new_val' to \a value */
			new_val = value;
			/* Handle LMA bit (read-only). Write operation is ignored. */
			/** If the specified vCPU attempts to modify the 'IA-32e Mode Active' Bit (Bit 10) in its
			 *  MSR IA32_EFER. (This bit is read-only and write operation shall be ignored.) */
			if ((efer_changed_bits & MSR_IA32_EFER_LMA_BIT) != 0UL) {
				/** Clear the 'IA-32e Mode Active' Bit (Bit 10) in 'new_val' */
				new_val &= ~MSR_IA32_EFER_LMA_BIT;
				/** Set the 'IA-32e Mode Active' Bit (Bit 10) in 'new_val' to
				 *  the 'IA-32e Mode Active' Bit (Bit 10) in 'guest_efer' */
				new_val |= guest_efer & MSR_IA32_EFER_LMA_BIT;
			}

			/** Call vcpu_set_efer with the following parameters, in order to write 'new_val' into
			 *  the MSR IA32_EFER associated with \a vcpu.
			 *  - vcpu
			 *  - new_val
			 */
			vcpu_set_efer(vcpu, new_val);

			/** If the specified vCPU attempts to modify the 'Execute Disable Bit Enable' Bit (Bit 11) in
			 *  its MSR IA32_EFER */
			if ((efer_changed_bits & MSR_IA32_EFER_NXE_BIT) != 0UL) {
				/*
				 * When NXE bit is changed, flush TLB entries and paging structure cache entries
				 * applicable to the vCPU.
				 */
				/** Call vcpu_make_request with the following parameters, in order to
				 *  flush TLB entries and paging structure cache entries applicable to \a vcpu.
				 *  - vcpu
				 *  - ACRN_REQUEST_EPT_FLUSH
				 */
				vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
			}
		}
	}

	/** Return 'err' */
	return err;
}

/**
 * @brief Handle the VM exit caused by a WRMSR instruction executing from guest software.
 *
 * Handle the VM exit caused by a WRMSR instruction executing from guest software and return a status code indicating
 * whether further handling operations are required from the caller.
 * If the status code is 0, it indicates that no further operation is required.
 * If the status code is negative, it indicates that the caller needs to inject #GP(0) to guest software.
 *
 * It is supposed to be used as a callback in 'vmexit_handler' from 'vp-base.hv_main' module.
 *
 * @param[inout] vcpu A structure representing the vCPU attempting to execute a WRMSR instruction.
 *
 * @return A status code indicating whether further handling operations are required.
 *
 * @retval 0 The WRMSR instruction executing from guest software would not cause an exception being generated.
 * @retval negative The WRMSR instruction executing from guest software would cause an exception being generated.
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 * @remark This API shall be called after init_msr_emulation has been called on \a vcpu once on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
int32_t wrmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type int32_t.
	 *  - err representing a status code, initialized as 0. */
	int32_t err = 0;
	/** Declare the following local variables of type uint32_t.
	 *  - msr representing the MSR to be written into, not initialized. */
	uint32_t msr;
	/** Declare the following local variables of type uint64_t.
	 *  - v representing the value to be written into the specified MSR, not initialized. */
	uint64_t v;

	/* Read the MSR ID */
	/** Set 'msr' to the return value of '(uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX)',
	 *  which is the contents of ECX register associated with \a vcpu and it specifies the MSR to be written into.
	 */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/* Get the MSR contents */
	/** Set high 32-bits of 'v' to low 32-bits of the return value of 'vcpu_get_gpreg(vcpu, CPU_REG_RDX)' and
	 *  set low 32-bits of 'v' to low 32-bits of the return value of 'vcpu_get_gpreg(vcpu, CPU_REG_RAX)' */
	v = (vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U) | (vcpu_get_gpreg(vcpu, CPU_REG_RAX) & 0xFFFFFFFFUL);

	/* Do the required processing for each msr case */
	/** Depending on 'msr' */
	switch (msr) {
	/** 'msr' is MSR_IA32_TSC_DEADLINE */
	case MSR_IA32_TSC_DEADLINE: {
		/** Call vlapic_set_tsc_deadline_msr with the following parameters, in order to
		 *  emulate the write operation into the MSR IA32_TSC_DEADLINE for \a vcpu (write value is 'v').
		 *  - vcpu_vlapic(vcpu)
		 *  - v
		 */
		vlapic_set_tsc_deadline_msr(vcpu_vlapic(vcpu), v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_TSC_ADJUST */
	case MSR_IA32_TSC_ADJUST: {
		/** Call set_guest_tsc_adjust with the following parameters, in order to
		 *  emulate the write operation into the MSR IA32_TSC_ADJUST for \a vcpu (write value is 'v').
		 *  - vcpu
		 *  - v
		 */
		set_guest_tsc_adjust(vcpu, v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_TIME_STAMP_COUNTER */
	case MSR_IA32_TIME_STAMP_COUNTER: {
		/** Call set_guest_tsc with the following parameters, in order to
		 *  emulate the write operation into the MSR IA32_TIME_STAMP_COUNTER for \a vcpu (write value is 'v').
		 *  - vcpu
		 *  - v
		 */
		set_guest_tsc(vcpu, v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_BIOS_SIGN_ID */
	case MSR_IA32_BIOS_SIGN_ID: {
		/* Writing non-zero value causes a general-protection exception (#GP(0)). */
		/** If 'v' is not equal to 0H */
		if (v != 0UL) {
			/** Set 'err' to -EACCES, which indicates that an exception needs to be injected
			 *  to guest software by the caller */
			err = -EACCES;
		}
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_PAT */
	case MSR_IA32_PAT: {
		/** Set 'err' to the return value of 'write_pat_msr(vcpu, v)', in order to
		 *  emulate the write operation into the MSR IA32_PAT for \a vcpu (write value is 'v') and
		 *  save the status code to 'err'. */
		err = write_pat_msr(vcpu, v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_EFER */
	case MSR_IA32_EFER: {
		/** Set 'err' to the return value of 'write_efer_msr(vcpu, v)', in order to
		 *  emulate the write operation into the MSR IA32_EFER for \a vcpu (write value is 'v') and
		 *  save the status code to 'err'. */
		err = write_efer_msr(vcpu, v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_MISC_ENABLE */
	case MSR_IA32_MISC_ENABLE: {
		/** Set 'err' to the return value of 'set_guest_ia32_misc_enable(vcpu, v)', in order to
		 *  emulate the write operation into the MSR IA32_MISC_ENABLE for \a vcpu (write value is 'v') and
		 *  save the status code to 'err'. */
		err = set_guest_ia32_misc_enable(vcpu, v);
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_SPEC_CTRL */
	case MSR_IA32_SPEC_CTRL: {
		/** Call msr_write with the following parameters, in order to write "v & (~MSR_IA32_SPEC_CTRL_STIBP)"
		 *  to the native MSR IA32_SPEC_CTRL.
		 *  - MSR_IA32_SPEC_CTRL
		 *  - v & (~MSR_IA32_SPEC_CTRL_STIBP)
		 */
		msr_write(MSR_IA32_SPEC_CTRL, v & (~MSR_IA32_SPEC_CTRL_STIBP));
		/** End of case */
		break;
	}
	/** 'msr' is MSR_IA32_MONITOR_FILTER_SIZE */
	case MSR_IA32_MONITOR_FILTER_SIZE: {
		/* No operations with "return == 0". */
		/** End of case, as writing into guest MSR IA32_MONITOR_FILTER_SIZE is ignored. */
		break;
	}
	/** Otherwise */
	default: {
		/** If 'msr' is a valid IA32_MCi_CTL2 MSR, or IA32_MCi_CTL MSR, or IA32_MCi_STATUS MSR */
		if (is_mc_ctl2_msr(msr) || is_mc_ctl_msr(msr) || is_mc_status_msr(msr)) {
			/* handle Machine Check related MSRs */
			/** If 'vcpu->vm' is not a safety VM */
			if (!is_safety_vm(vcpu->vm)) {
				/** Set 'err' to -EACCES, which indicates that an exception needs to be injected
				 *  to guest software by the caller */
				err = -EACCES;
			}
			/* For safety VM, these MSRs are either not trapped or no operations with "return == 0". */
		/** If 'msr' belongs to x2APIC MSRs */
		} else if (is_x2apic_msr(msr)) {
			/** Set 'err' to the return value of 'vlapic_x2apic_write(vcpu, msr, v)', in order to
			 *  emulate the write operation into 'msr' for \a vcpu (write value is 'v') and
			 *  save the status code to 'err'. */
			err = vlapic_x2apic_write(vcpu, msr, v);
		} else {
			/** Logging the following information with a log level of 4.
			 *  - __func__
			 *  - vcpu->vm->vm_id
			 *  - vcpu->vcpu_id
			 *  - msr
			 */
			pr_warn("%s(): vm%d vcpu%d writing MSR %lx not supported", __func__, vcpu->vm->vm_id,
				vcpu->vcpu_id, msr);
			/** Set 'err' to -EACCES, which indicates that an exception needs to be injected
			 *  to guest software by the caller */
			err = -EACCES;
		}
		/** End of case */
		break;
	}
	}

	/** Call TRACE_2L with the following parameters, in order to trace the VM exit caused by a WRMSR instruction
	 *  executing from guest software.
	 *  - TRACE_VMEXIT_WRMSR
	 *  - msr
	 *  - v
	 */
	TRACE_2L(TRACE_VMEXIT_WRMSR, msr, v);

	/** Return 'err' */
	return err;
}

/*
 * After switch to x2apic mode, most MSRs are passthrough to guest, but vlapic is still valid
 * for virtualization of some MSRs for security consideration:
 * - XAPICID/LDR: Read to XAPICID/LDR need to be trapped to guarantee guest always see right vlapic_id.
 * - ICR: Write to ICR need to be trapped to avoid milicious IPI.
 */

/**
 * @brief Update the MSR bitmap associated with the specified vCPU to support x2APIC and LAPIC passthrough.
 *
 * Update the MSR bitmap associated with the specified vCPU to support x2APIC and LAPIC passthrough.
 *
 * It is supposed to be called only by init_msr_emulation.
 *
 * @param[inout] vcpu A pointer which points to a structure representing the vCPU whose MSR bitmap is to be updated.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark The host physical address calculated by hva2hpa(vcpu->arch.vmcs) is equal to the current-VMCS pointer
 *         of the current pCPU.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type 'uint8_t *'.
	 *  - msr_bitmap representing a pointer which points to the MSR bitmap associated with the specified \a vcpu,
	 *  initialized as vcpu->arch.msr_bitmap. */
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;

	/** Call intercept_x2apic_msrs with the following parameters, in order to update 'msr_bitmap'
	 *  with regard to x2APIC MSRs, the specified mode for these MSRs is INTERCEPT_DISABLE.
	 *  - msr_bitmap
	 *  - INTERCEPT_DISABLE
	 */
	intercept_x2apic_msrs(msr_bitmap, INTERCEPT_DISABLE);
	/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
	 *  according to the specified MSR 'MSR_IA32_EXT_XAPICID' and the specified mode INTERCEPT_READ.
	 *  - msr_bitmap
	 *  - MSR_IA32_EXT_XAPICID
	 *  - INTERCEPT_READ
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_XAPICID, INTERCEPT_READ);
	/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
	 *  according to the specified MSR 'MSR_IA32_EXT_APIC_LDR' and the specified mode INTERCEPT_READ.
	 *  - msr_bitmap
	 *  - MSR_IA32_EXT_APIC_LDR
	 *  - INTERCEPT_READ
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_LDR, INTERCEPT_READ);
	/** Call enable_msr_interception with the following parameters, in order to update 'msr_bitmap'
	 *  according to the specified MSR 'MSR_IA32_EXT_APIC_ICR' and the specified mode INTERCEPT_READ_WRITE.
	 *  - msr_bitmap
	 *  - MSR_IA32_EXT_APIC_ICR
	 *  - INTERCEPT_READ_WRITE
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_ICR, INTERCEPT_READ_WRITE);
	/** Call set_tsc_msr_interception with the following parameters, in order to
	 *  update system state according to the changes of the interception associated with the MSR IA32_TSC_DEADLINE.
	 *  MSR IA32_TSC_DEADLINE needs to be intercepted if 'exec_vmread64(VMX_TSC_OFFSET_FULL)' is not equal to 0H.
	 *  - vcpu
	 *  - exec_vmread64(VMX_TSC_OFFSET_FULL) != 0H
	 */
	set_tsc_msr_interception(vcpu, exec_vmread64(VMX_TSC_OFFSET_FULL) != 0UL);
}

/**
 * @}
 */
