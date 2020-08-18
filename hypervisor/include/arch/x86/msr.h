/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MSR_H
#define MSR_H

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief Define macros related to MSRs.
 */

/* architectural (common) MSRs */

/**
 * @brief The register address of IA32_P5_MC_ADDR MSR.
 *
 * Machine check address for MC exception handler.
 */
#define MSR_IA32_P5_MC_ADDR			0x00000000U
/**
 * @brief The register address of IA32_P5_MC_TYPE MSR.
 *
 * Machine check error type for MC exception handler.
 */
#define MSR_IA32_P5_MC_TYPE			0x00000001U

/**
 * @brief The register address of IA32_MONITOR_FILTER_SIZE MSR.
 *
 * System coherence line size for MWAIT/MONITOR.
 */
#define MSR_IA32_MONITOR_FILTER_SIZE		0x00000006U

/**
 * @brief The register address of IA32_PLATFORM_ID MSR.
 *
 * The operating system can use this MSR to determine slot information for the processor
 * and the proper microcode update to load.
 */
#define MSR_IA32_PLATFORM_ID			0x00000017U
/**
 * @brief The register address of MSR_SMI_COUNT MSR.
 *
 * SMI Counter.
 */
#define MSR_SMI_COUNT				0x00000034U
/**
 * @brief The register address of MSR_PLATFORM_INFO MSR.
 */
#define MSR_PLATFORM_INFO			0x000000CEU
/**
 * @brief The register address of IA32_FLUSH_CMD MSR.
 */
#define MSR_IA32_FLUSH_CMD			0x0000010BU
/**
 * @brief The register address of MSR_FEATURE_CONFIG MSR.
 */
#define MSR_FEATURE_CONFIG			0x0000013CU
/**
 * @brief The register address of IA32_SYSENTER_CS MSR.
 */
#define MSR_IA32_SYSENTER_CS			0x00000174U
/**
 * @brief The register address of IA32_SYSENTER_ESP MSR.
 */
#define MSR_IA32_SYSENTER_ESP			0x00000175U
/**
 * @brief The register address of IA32_SYSENTER_EIP MSR.
 */
#define MSR_IA32_SYSENTER_EIP			0x00000176U
/**
 * @brief The register address of IA32_CSTAR MSR.
 */
#define MSR_IA32_CSTAR				0xC0000083U
/**
 * @brief The register address of IA32_MC0_CTL2 MSR.
 */
#define MSR_IA32_MC0_CTL2			0x00000280U
/**
 * @brief The register address of IA32_MC4_CTL2 MSR.
 */
#define MSR_IA32_MC4_CTL2			0x00000284U
/**
 * @brief The register address of IA32_MC9_CTL2 MSR.
 */
#define MSR_IA32_MC9_CTL2			0x00000289U
/**
 * @brief The register address of IA32_MC0_CTL MSR.
 */
#define MSR_IA32_MC0_CTL			0x00000400U
/**
 * @brief The register address of IA32_MC0_STATUS MSR.
 */
#define MSR_IA32_MC0_STATUS			0x00000401U

/* Speculation Control */
/**
 * @brief The register address of IA32_SPEC_CTRL MSR.
 */
#define MSR_IA32_SPEC_CTRL			0x00000048U

/**
 * @brief The register address of IA32_TIME_STAMP_COUNTER MSR.
 */
#define MSR_IA32_TIME_STAMP_COUNTER 0x00000010U
/**
 * @brief The register address of IA32_APIC_BASE MSR.
 */
#define MSR_IA32_APIC_BASE          0x0000001BU
/**
 * @brief The register address of IA32_FEATURE_CONTROL MSR.
 */
#define MSR_IA32_FEATURE_CONTROL    0x0000003AU
/**
 * @brief The register address of IA32_TSC_ADJUST MSR.
 */
#define MSR_IA32_TSC_ADJUST         0x0000003BU
/* Prediction Command */
/**
 * @brief The register address of IA32_PRED_CMD MSR.
 */
#define MSR_IA32_PRED_CMD                0x00000049U
/**
 * @brief The register address of IA32_BIOS_SIGN_ID MSR.
 */
#define MSR_IA32_BIOS_SIGN_ID            0x0000008BU
/**
 * @brief The register address of IA32_MTRRCAP MSR.
 */
#define MSR_IA32_MTRR_CAP                0x000000FEU

/**
 * @brief The register address of IA32_ARCH_CAPABILITIES MSR.
 */
#define MSR_IA32_ARCH_CAPABILITIES       0x0000010AU
/**
 * @brief The register address of IA32_MCG_CAP MSR.
 */
#define MSR_IA32_MCG_CAP                 0x00000179U
/**
 * @brief The register address of IA32_MCG_STATUS MSR.
 */
#define MSR_IA32_MCG_STATUS              0x0000017AU
/**
 * @brief The register address of IA32_MCG_CTL MSR.
 */
#define MSR_IA32_MCG_CTL                 0x0000017BU
/**
 * @brief The register address of IA32_PERF_CTL MSR.
 */
#define MSR_IA32_PERF_CTL                0x00000199U
/**
 * @brief The register address of IA32_MISC_ENABLE MSR.
 */
#define MSR_IA32_MISC_ENABLE             0x000001A0U
/**
 * @brief The register address of IA32_PAT MSR.
 */
#define MSR_IA32_PAT                     0x00000277U
/**
 * @brief The register address of IA32_VMX_BASIC MSR.
 */
#define MSR_IA32_VMX_BASIC               0x00000480U
/**
 * @brief The register address of IA32_VMX_PINBASED_CTLS MSR.
 */
#define MSR_IA32_VMX_PINBASED_CTLS       0x00000481U
/**
 * @brief The register address of IA32_VMX_PROCBASED_CTLS MSR.
 */
#define MSR_IA32_VMX_PROCBASED_CTLS      0x00000482U
/**
 * @brief The register address of IA32_VMX_EXIT_CTLS MSR.
 */
#define MSR_IA32_VMX_EXIT_CTLS           0x00000483U
/**
 * @brief The register address of IA32_VMX_ENTRY_CTLS MSR.
 */
#define MSR_IA32_VMX_ENTRY_CTLS          0x00000484U
/**
 * @brief The register address of IA32_VMX_MISC MSR.
 */
#define MSR_IA32_VMX_MISC                0x00000485U
/**
 * @brief The register address of IA32_VMX_CR0_FIXED0 MSR.
 */
#define MSR_IA32_VMX_CR0_FIXED0          0x00000486U
/**
 * @brief The register address of IA32_VMX_CR0_FIXED1 MSR.
 */
#define MSR_IA32_VMX_CR0_FIXED1          0x00000487U
/**
 * @brief The register address of IA32_VMX_CR4_FIXED0 MSR.
 */
#define MSR_IA32_VMX_CR4_FIXED0          0x00000488U
/**
 * @brief The register address of IA32_VMX_CR4_FIXED1 MSR.
 */
#define MSR_IA32_VMX_CR4_FIXED1          0x00000489U
/**
 * @brief The register address of IA32_VMX_VMCS_ENUM MSR.
 */
#define MSR_IA32_VMX_VMCS_ENUM           0x0000048AU
/**
 * @brief The register address of IA32_VMX_PROCBASED_CTLS2 MSR.
 */
#define MSR_IA32_VMX_PROCBASED_CTLS2     0x0000048BU
/**
 * @brief The register address of IA32_VMX_EPT_VPID_CAP MSR.
 */
#define MSR_IA32_VMX_EPT_VPID_CAP        0x0000048CU
/**
 * @brief The register address of IA32_VMX_TRUE_PINBASED_CTLS MSR.
 */
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS  0x0000048DU
/**
 * @brief The register address of IA32_VMX_TRUE_PROCBASED_CTLS MSR.
 */
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS 0x0000048EU
/**
 * @brief The register address of IA32_VMX_TRUE_EXIT_CTLS MSR.
 */
#define MSR_IA32_VMX_TRUE_EXIT_CTLS      0x0000048FU
/**
 * @brief The register address of IA32_VMX_TRUE_ENTRY_CTLS MSR.
 */
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS     0x00000490U
/**
 * @brief The register address of IA32_VMX_VMFUNC MSR.
 */
#define MSR_IA32_VMX_VMFUNC              0x00000491U
/**
 * @brief The register address of IA32_DS_AREA MSR.
 */
#define MSR_IA32_DS_AREA                 0x00000600U
/**
 * @brief The register address of IA32_TSC_DEADLINE MSR.
 */
#define MSR_IA32_TSC_DEADLINE            0x000006E0U

/**
 * @brief The register address of IA32_X2APIC_APICID MSR.
 */
#define MSR_IA32_EXT_XAPICID			0x00000802U
/**
 * @brief The register address of IA32_X2APIC_VERSION MSR.
 */
#define MSR_IA32_EXT_APIC_VERSION		0x00000803U
/**
 * @brief The register address of IA32_X2APIC_TPR MSR.
 */
#define MSR_IA32_EXT_APIC_TPR			0x00000808U
/**
 * @brief The register address of IA32_X2APIC_PPR MSR.
 */
#define MSR_IA32_EXT_APIC_PPR			0x0000080AU
/**
 * @brief The register address of IA32_X2APIC_EOI MSR.
 */
#define MSR_IA32_EXT_APIC_EOI			0x0000080BU
/**
 * @brief The register address of IA32_X2APIC_LDR MSR.
 */
#define MSR_IA32_EXT_APIC_LDR			0x0000080DU
/**
 * @brief The register address of IA32_X2APIC_SIVR MSR.
 */
#define MSR_IA32_EXT_APIC_SIVR			0x0000080FU
/**
 * @brief The register address of IA32_X2APIC_ISR0 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR0			0x00000810U
/**
 * @brief The register address of IA32_X2APIC_ISR1 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR1			0x00000811U
/**
 * @brief The register address of IA32_X2APIC_ISR2 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR2			0x00000812U
/**
 * @brief The register address of IA32_X2APIC_ISR3 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR3			0x00000813U
/**
 * @brief The register address of IA32_X2APIC_ISR4 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR4			0x00000814U
/**
 * @brief The register address of IA32_X2APIC_ISR5 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR5			0x00000815U
/**
 * @brief The register address of IA32_X2APIC_ISR6 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR6			0x00000816U
/**
 * @brief The register address of IA32_X2APIC_ISR7 MSR.
 */
#define MSR_IA32_EXT_APIC_ISR7			0x00000817U
/**
 * @brief The register address of IA32_X2APIC_TMR0 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR0			0x00000818U
/**
 * @brief The register address of IA32_X2APIC_TMR1 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR1			0x00000819U
/**
 * @brief The register address of IA32_X2APIC_TMR2 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR2			0x0000081AU
/**
 * @brief The register address of IA32_X2APIC_TMR3 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR3			0x0000081BU
/**
 * @brief The register address of IA32_X2APIC_TMR4 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR4			0x0000081CU
/**
 * @brief The register address of IA32_X2APIC_TMR5 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR5			0x0000081DU
/**
 * @brief The register address of IA32_X2APIC_TMR6 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR6			0x0000081EU
/**
 * @brief The register address of IA32_X2APIC_TMR7 MSR.
 */
#define MSR_IA32_EXT_APIC_TMR7			0x0000081FU
/**
 * @brief The register address of IA32_X2APIC_IRR0 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR0			0x00000820U
/**
 * @brief The register address of IA32_X2APIC_IRR1 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR1			0x00000821U
/**
 * @brief The register address of IA32_X2APIC_IRR2 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR2			0x00000822U
/**
 * @brief The register address of IA32_X2APIC_IRR3 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR3			0x00000823U
/**
 * @brief The register address of IA32_X2APIC_IRR4 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR4			0x00000824U
/**
 * @brief The register address of IA32_X2APIC_IRR5 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR5			0x00000825U
/**
 * @brief The register address of IA32_X2APIC_IRR6 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR6			0x00000826U
/**
 * @brief The register address of IA32_X2APIC_IRR7 MSR.
 */
#define MSR_IA32_EXT_APIC_IRR7			0x00000827U
/**
 * @brief The register address of IA32_X2APIC_ESR MSR.
 */
#define MSR_IA32_EXT_APIC_ESR			0x00000828U
/**
 * @brief The register address of IA32_X2APIC_LVT_CMCI MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_CMCI		0x0000082FU
/**
 * @brief The register address of IA32_X2APIC_ICR MSR.
 */
#define MSR_IA32_EXT_APIC_ICR			0x00000830U
/**
 * @brief The register address of IA32_X2APIC_LVT_TIMER MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_TIMER		0x00000832U
/**
 * @brief The register address of IA32_X2APIC_LVT_THERMAL MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_THERMAL		0x00000833U
/**
 * @brief The register address of IA32_X2APIC_LVT_PMI MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_PMI		0x00000834U
/**
 * @brief The register address of IA32_X2APIC_LVT_LINT0 MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_LINT0		0x00000835U
/**
 * @brief The register address of IA32_X2APIC_LVT_LINT1 MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_LINT1		0x00000836U
/**
 * @brief The register address of IA32_X2APIC_LVT_ERROR MSR.
 */
#define MSR_IA32_EXT_APIC_LVT_ERROR		0x00000837U
/**
 * @brief The register address of IA32_X2APIC_INIT_COUNT MSR.
 */
#define MSR_IA32_EXT_APIC_INIT_COUNT		0x00000838U
/**
 * @brief The register address of IA32_X2APIC_CUR_COUNT MSR.
 */
#define MSR_IA32_EXT_APIC_CUR_COUNT		0x00000839U
/**
 * @brief The register address of IA32_X2APIC_DIV_CONF MSR.
 */
#define MSR_IA32_EXT_APIC_DIV_CONF		0x0000083EU
/**
 * @brief The register address of IA32_X2APIC_SELF_IPI MSR.
 */
#define MSR_IA32_EXT_APIC_SELF_IPI		0x0000083FU

/**
 * @brief The register address of IA32_XSS MSR.
 */
#define MSR_IA32_XSS                  0x00000DA0U
/**
 * @brief The register address of IA32_BNDCFGS MSR.
 */
#define MSR_IA32_BNDCFGS              0x00000D90U
/**
 * @brief The register address of IA32_EFER MSR.
 */
#define MSR_IA32_EFER                 0xC0000080U
/**
 * @brief The register address of IA32_STAR MSR.
 */
#define MSR_IA32_STAR                 0xC0000081U
/**
 * @brief The register address of IA32_LSTAR MSR.
 */
#define MSR_IA32_LSTAR                0xC0000082U
/**
 * @brief The register address of IA32_FMASK MSR.
 */
#define MSR_IA32_FMASK                0xC0000084U
/**
 * @brief The register address of IA32_FS_BASE MSR.
 */
#define MSR_IA32_FS_BASE              0xC0000100U
/**
 * @brief The register address of IA32_GS_BASE MSR.
 */
#define MSR_IA32_GS_BASE              0xC0000101U
/**
 * @brief The register address of IA32_KERNEL_GS_BASE MSR.
 */
#define MSR_IA32_KERNEL_GS_BASE       0xC0000102U
/**
 * @brief The register address of IA32_TSC_AUX MSR.
 */
#define MSR_IA32_TSC_AUX              0xC0000103U

/**
 * @brief The register address of TSX_FORCE_ABORT MSR.
 *
 * Used to enable RTM force abort mode.
 */
#define MSR_TSX_FORCE_ABORT           0x0000010FU

/**
 * @brief The LME bit of IA32_EFER.
 *
 * IA32e mode enable.
 * Enables IA-32e mode operation.
 */
#define MSR_IA32_EFER_LME_BIT (1UL << 8U)
/**
 * @brief The LMA bit of IA32_EFER.
 *
 * IA32e mode active.
 * Indicates IA-32e mode is active when set.
 */
#define MSR_IA32_EFER_LMA_BIT (1UL << 10U)
/**
 * @brief The NXE bit of IA32_EFER.
 */
#define MSR_IA32_EFER_NXE_BIT (1UL << 11U)

/* FEATURE CONTROL bits */
/**
 * @brief The lock bit of the IA32_FEATURE_CONTROL MSR.
 */
#define MSR_IA32_FEATURE_CONTROL_LOCK       (1U << 0U)
/**
 * @brief The bit of the IA32_FEATURE_CONTROL MSR used to enable VMX outside SMX operation.
 *
 * This bit enables VMX for a system executive that does not require SMX.
 */
#define MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX (1U << 2U)

/* PAT memory type definitions */
/**
 * @brief The code of Uncacheable memory type in IA32_PAT MSR.
 */
#define PAT_MEM_TYPE_UC  0x00UL /* uncached */
/**
 * @brief The code of Write Through memory type in IA32_PAT MSR.
 */
#define PAT_MEM_TYPE_WT  0x04UL /* write through */
/**
 * @brief The code of Write Back memory type in IA32_PAT MSR.
 */
#define PAT_MEM_TYPE_WB  0x06UL /* writeback */
/**
 * @brief The code of Uncached memory type in IA32_PAT MSR.
 */
#define PAT_MEM_TYPE_UCM 0x07UL /* uncached minus */

/**
 * @brief The bit of the IA32_MISC_ENABLE MSR used to enable MONITOR FSM.
 *
 * When this bit is set to 0, the MONITOR feature flag is not set (CPUID.01H:ECX[bit3] = 0).
 * This indicates that MONITOR/MWAIT are not supported.
 */
#define MSR_IA32_MISC_ENABLE_MONITOR_ENA (1UL << 18U)
/**
 * @brief The bit of the IA32_MISC_ENABLE MSR used to limit CPUID Maxval.
 */
#define MSR_IA32_MISC_ENABLE_LIMIT_CPUID (1UL << 22U)
/**
 * @brief The bit of the IA32_MISC_ENABLE MSR used to disable XD Bit.
 */
#define MSR_IA32_MISC_ENABLE_XD_DISABLE  (1UL << 34U)

/**
 * @brief The default value of IA32_PAT MSR when power-on.
 */
#define PAT_POWER_ON_VALUE                                                                                  \
	(PAT_MEM_TYPE_WB + (PAT_MEM_TYPE_WT << 8U) + (PAT_MEM_TYPE_UCM << 16U) + (PAT_MEM_TYPE_UC << 24U) + \
		(PAT_MEM_TYPE_WB << 32U) + (PAT_MEM_TYPE_WT << 40U) + (PAT_MEM_TYPE_UCM << 48U) +           \
		(PAT_MEM_TYPE_UC << 56U))

/**
 * @brief The value of IA32_PAT MSR that to be configured all of the memory type to Uncacheable memory type.
 */
#define PAT_ALL_UC_VALUE                                                                                   \
	(PAT_MEM_TYPE_UC + (PAT_MEM_TYPE_UC << 8U) + (PAT_MEM_TYPE_UC << 16U) + (PAT_MEM_TYPE_UC << 24U) + \
		(PAT_MEM_TYPE_UC << 32U) + (PAT_MEM_TYPE_UC << 40U) + (PAT_MEM_TYPE_UC << 48U) +           \
		(PAT_MEM_TYPE_UC << 56U))

/**
 * @brief The bit of IA32_ARCH_CAPABILITIES MSR representing whether the hypervisor need flush the L1D on VM entry.
 *
 * If this bit is set, indicate the hypervisor need not flush the L1D on VM entry.
 */
#define IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY (1UL << 3U)
/**
 * @brief The bit of IA32_ARCH_CAPABILITIES MSR representing whether the processor is susceptible to Speculative Store
 * Bypass.
 *
 * If this bit is set, indicate the processor is not susceptible to Speculative Store Bypass.
 */
#define IA32_ARCH_CAP_SSB_NO             (1UL << 4U)
/**
 * @brief The bit of IA32_ARCH_CAPABILITIES MSR representing whether the processor is susceptible to Microarchitectural
 * Data Sampling.
 *
 * If this bit is set, indicate the processor is not susceptible to Microarchitectural Data Sampling.
 */
#define IA32_ARCH_CAP_MDS_NO             (1UL << 5U)
/**
 * @brief The bit of IA32_ARCH_CAPABILITIES MSR representing whether the processor is susceptible to machine check
 * error due to modifying the size of a code page without TBL invalidation.
 *
 * If set, the processor is not susceptible to machine check error due to modifying the size of a code page without TBL
 * invalidation.
 */
#define IA32_ARCH_CAP_IF_PSCHANGE_MC_NO  (1UL << 6U)

/* Flush L1 D-cache */
/**
 * @brief The bit of IA32_FLUSH_CMD MSR used to writeback and invalidate the L1 data cache.
 */
#define IA32_L1D_FLUSH			(1UL << 0U)

/**
 * @}
 */

#endif /* MSR_H */
