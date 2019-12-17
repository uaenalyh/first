/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MSR_H
#define MSR_H

/**
 * @addtogroup vp-base_vmsr
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/* architectural (common) MSRs */

/* Machine check address for MC exception handler */
#define MSR_IA32_P5_MC_ADDR			0x00000000U
/* Machine check error type for MC exception handler */
#define MSR_IA32_P5_MC_TYPE			0x00000001U

/* System coherence line size for MWAIT/MONITOR */
#define MSR_IA32_MONITOR_FILTER_SIZE		0x00000006U

#define MSR_IA32_PLATFORM_ID			0x00000017U
#define MSR_SMI_COUNT				0x00000034U
#define MSR_PLATFORM_INFO			0x000000CEU
#define MSR_IA32_FLUSH_CMD			0x0000010BU
#define MSR_FEATURE_CONFIG			0x0000013CU

#define MSR_IA32_SYSENTER_CS			0x00000174U
#define MSR_IA32_SYSENTER_ESP			0x00000175U
#define MSR_IA32_SYSENTER_EIP			0x00000176U
#define MSR_IA32_CSTAR				0xC0000083U

#define MSR_IA32_MC0_CTL2			0x00000280U
#define MSR_IA32_MC4_CTL2			0x00000284U
#define MSR_IA32_MC9_CTL2			0x00000289U
#define MSR_IA32_MC0_CTL			0x00000400U
#define MSR_IA32_MC0_STATUS			0x00000401U

/* Speculation Control */
#define MSR_IA32_SPEC_CTRL			0x00000048U

#define MSR_IA32_TIME_STAMP_COUNTER 0x00000010U
#define MSR_IA32_APIC_BASE          0x0000001BU
#define MSR_IA32_FEATURE_CONTROL    0x0000003AU
#define MSR_IA32_TSC_ADJUST         0x0000003BU
/* Prediction Command */
#define MSR_IA32_PRED_CMD                0x00000049U
#define MSR_IA32_BIOS_SIGN_ID            0x0000008BU
#define MSR_IA32_MTRR_CAP                0x000000FEU
#define MSR_IA32_ARCH_CAPABILITIES       0x0000010AU
#define MSR_IA32_MCG_CAP                 0x00000179U
#define MSR_IA32_MCG_STATUS              0x0000017AU
#define MSR_IA32_MCG_CTL                 0x0000017BU
#define MSR_IA32_PERF_CTL                0x00000199U
#define MSR_IA32_MISC_ENABLE             0x000001A0U
#define MSR_IA32_PAT                     0x00000277U
#define MSR_IA32_VMX_BASIC               0x00000480U
#define MSR_IA32_VMX_PINBASED_CTLS       0x00000481U
#define MSR_IA32_VMX_PROCBASED_CTLS      0x00000482U
#define MSR_IA32_VMX_EXIT_CTLS           0x00000483U
#define MSR_IA32_VMX_ENTRY_CTLS          0x00000484U
#define MSR_IA32_VMX_MISC                0x00000485U
#define MSR_IA32_VMX_CR0_FIXED0          0x00000486U
#define MSR_IA32_VMX_CR0_FIXED1          0x00000487U
#define MSR_IA32_VMX_CR4_FIXED0          0x00000488U
#define MSR_IA32_VMX_CR4_FIXED1          0x00000489U
#define MSR_IA32_VMX_VMCS_ENUM           0x0000048AU
#define MSR_IA32_VMX_PROCBASED_CTLS2     0x0000048BU
#define MSR_IA32_VMX_EPT_VPID_CAP        0x0000048CU
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS  0x0000048DU
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS 0x0000048EU
#define MSR_IA32_VMX_TRUE_EXIT_CTLS      0x0000048FU
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS     0x00000490U
#define MSR_IA32_VMX_VMFUNC              0x00000491U
#define MSR_IA32_DS_AREA                 0x00000600U
#define MSR_IA32_TSC_DEADLINE            0x000006E0U

#define MSR_IA32_EXT_XAPICID			0x00000802U
#define MSR_IA32_EXT_APIC_VERSION		0x00000803U
#define MSR_IA32_EXT_APIC_TPR			0x00000808U
#define MSR_IA32_EXT_APIC_PPR			0x0000080AU
#define MSR_IA32_EXT_APIC_EOI			0x0000080BU
#define MSR_IA32_EXT_APIC_LDR			0x0000080DU
#define MSR_IA32_EXT_APIC_SIVR			0x0000080FU
#define MSR_IA32_EXT_APIC_ISR0			0x00000810U
#define MSR_IA32_EXT_APIC_ISR1			0x00000811U
#define MSR_IA32_EXT_APIC_ISR2			0x00000812U
#define MSR_IA32_EXT_APIC_ISR3			0x00000813U
#define MSR_IA32_EXT_APIC_ISR4			0x00000814U
#define MSR_IA32_EXT_APIC_ISR5			0x00000815U
#define MSR_IA32_EXT_APIC_ISR6			0x00000816U
#define MSR_IA32_EXT_APIC_ISR7			0x00000817U
#define MSR_IA32_EXT_APIC_TMR0			0x00000818U
#define MSR_IA32_EXT_APIC_TMR1			0x00000819U
#define MSR_IA32_EXT_APIC_TMR2			0x0000081AU
#define MSR_IA32_EXT_APIC_TMR3			0x0000081BU
#define MSR_IA32_EXT_APIC_TMR4			0x0000081CU
#define MSR_IA32_EXT_APIC_TMR5			0x0000081DU
#define MSR_IA32_EXT_APIC_TMR6			0x0000081EU
#define MSR_IA32_EXT_APIC_TMR7			0x0000081FU
#define MSR_IA32_EXT_APIC_IRR0			0x00000820U
#define MSR_IA32_EXT_APIC_IRR1			0x00000821U
#define MSR_IA32_EXT_APIC_IRR2			0x00000822U
#define MSR_IA32_EXT_APIC_IRR3			0x00000823U
#define MSR_IA32_EXT_APIC_IRR4			0x00000824U
#define MSR_IA32_EXT_APIC_IRR5			0x00000825U
#define MSR_IA32_EXT_APIC_IRR6			0x00000826U
#define MSR_IA32_EXT_APIC_IRR7			0x00000827U
#define MSR_IA32_EXT_APIC_ESR			0x00000828U
#define MSR_IA32_EXT_APIC_LVT_CMCI		0x0000082FU
#define MSR_IA32_EXT_APIC_ICR			0x00000830U
#define MSR_IA32_EXT_APIC_LVT_TIMER		0x00000832U
#define MSR_IA32_EXT_APIC_LVT_THERMAL		0x00000833U
#define MSR_IA32_EXT_APIC_LVT_PMI		0x00000834U
#define MSR_IA32_EXT_APIC_LVT_LINT0		0x00000835U
#define MSR_IA32_EXT_APIC_LVT_LINT1		0x00000836U
#define MSR_IA32_EXT_APIC_LVT_ERROR		0x00000837U
#define MSR_IA32_EXT_APIC_INIT_COUNT		0x00000838U
#define MSR_IA32_EXT_APIC_CUR_COUNT		0x00000839U
#define MSR_IA32_EXT_APIC_DIV_CONF		0x0000083EU
#define MSR_IA32_EXT_APIC_SELF_IPI		0x0000083FU

#define MSR_IA32_XSS                  0x00000DA0U
#define MSR_IA32_BNDCFGS              0x00000D90U
#define MSR_IA32_EFER                 0xC0000080U
#define MSR_IA32_STAR                 0xC0000081U
#define MSR_IA32_LSTAR                0xC0000082U
#define MSR_IA32_FMASK                0xC0000084U
#define MSR_IA32_FS_BASE              0xC0000100U
#define MSR_IA32_GS_BASE              0xC0000101U
#define MSR_IA32_KERNEL_GS_BASE       0xC0000102U
#define MSR_IA32_TSC_AUX              0xC0000103U

#define MSR_IA32_EFER_LME_BIT (1UL << 8U) /* IA32e mode enable */
#define MSR_IA32_EFER_LMA_BIT (1UL << 10U) /* IA32e mode active */
#define MSR_IA32_EFER_NXE_BIT (1UL << 11U)

/* FEATURE CONTROL bits */
#define MSR_IA32_FEATURE_CONTROL_LOCK       (1U << 0U)
#define MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX (1U << 2U)

/* PAT memory type definitions */
#define PAT_MEM_TYPE_UC  0x00UL /* uncached */
#define PAT_MEM_TYPE_WT  0x04UL /* write through */
#define PAT_MEM_TYPE_WB  0x06UL /* writeback */
#define PAT_MEM_TYPE_UCM 0x07UL /* uncached minus */

#define MSR_IA32_MISC_ENABLE_MONITOR_ENA (1UL << 18U)
#define MSR_IA32_MISC_ENABLE_LIMIT_CPUID (1UL << 22U)
#define MSR_IA32_MISC_ENABLE_XD_DISABLE  (1UL << 34U)

#ifndef ASSEMBLER

struct acrn_vcpu;

void init_msr_emulation(struct acrn_vcpu *vcpu);
uint32_t vmsr_get_guest_msr_index(uint32_t msr);
void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu);

#endif /* ASSEMBLER */

#define PAT_POWER_ON_VALUE                                                                                  \
	(PAT_MEM_TYPE_WB + (PAT_MEM_TYPE_WT << 8U) + (PAT_MEM_TYPE_UCM << 16U) + (PAT_MEM_TYPE_UC << 24U) + \
		(PAT_MEM_TYPE_WB << 32U) + (PAT_MEM_TYPE_WT << 40U) + (PAT_MEM_TYPE_UCM << 48U) +           \
		(PAT_MEM_TYPE_UC << 56U))

#define PAT_ALL_UC_VALUE                                                                                   \
	(PAT_MEM_TYPE_UC + (PAT_MEM_TYPE_UC << 8U) + (PAT_MEM_TYPE_UC << 16U) + (PAT_MEM_TYPE_UC << 24U) + \
		(PAT_MEM_TYPE_UC << 32U) + (PAT_MEM_TYPE_UC << 40U) + (PAT_MEM_TYPE_UC << 48U) +           \
		(PAT_MEM_TYPE_UC << 56U))

#define IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY (1UL << 3U)
#define IA32_ARCH_CAP_SSB_NO             (1UL << 4U)
#define IA32_ARCH_CAP_MDS_NO             (1UL << 5U)
#define IA32_ARCH_CAP_IF_PSCHANGE_MC_NO  (1UL << 6U)

/**
 * @}
 */

#endif /* MSR_H */
