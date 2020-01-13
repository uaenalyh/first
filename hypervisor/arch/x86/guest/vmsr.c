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
#include <sgx.h>
#include <guest_pm.h>
#include <ucode.h>
#include <cat.h>
#include <trace.h>
#include <logmsg.h>

/**
 * @defgroup vp-base_vmsr vp-base.vmsr
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

#define INTERCEPT_DISABLE    (0U)
#define INTERCEPT_READ       (1U << 0U)
#define INTERCEPT_WRITE      (1U << 1U)
#define INTERCEPT_READ_WRITE (INTERCEPT_READ | INTERCEPT_WRITE)

#define MSR_IA32_SPEC_CTRL_STIBP	(1UL << 1U)
#define MCG_CAP_FOR_SAFETY_VM		0x040AUL

/* Only following bits are not reserved: 22 and 34. */
#define MSR_IA32_MISC_ENABLE_MASK	0x400400000UL
/* Only following bits are not reserved: 0, 8, 10, and 11. */
#define MSR_IA32_EFER_MASK		0xD01UL

#define LOW_MSR_START			0U
#define LOW_MSR_END			0x1FFFU
#define HIGH_MSR_START			0xc0000000U
#define HIGH_MSR_END			0xc0001FFFU

/* used to reserve entries for scope extension in the future */
#define MSR_RSVD			0xFFFFFFFFU

/* Machine Check Capability: Number of reporting banks */
#define NUM_MC_BANKS			10U

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

#define NUM_UNINTERCEPTED_MSRS 20U
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

#define NUM_X2APIC_MSRS 44U
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

static bool is_x2apic_msr(uint32_t msr)
{
	uint32_t i;
	bool ret = false;

	for (i = 0U; i < NUM_X2APIC_MSRS; i++) {
		if (msr == x2apic_msrs[i]) {
			ret = true;
			break;
		}
	}

	return ret;
}

/* emulated_guest_msrs[] shares same indexes with array vcpu->arch->guest_msrs[] */
uint32_t vmsr_get_guest_msr_index(uint32_t msr)
{
	uint32_t index;

	for (index = 0U; index < NUM_GUEST_MSRS; index++) {
		if (emulated_guest_msrs[index] == msr) {
			break;
		}
	}

	if (index == NUM_GUEST_MSRS) {
		pr_err("%s, MSR %x is not defined in array emulated_guest_msrs[]", __func__, msr);
	}

	return index;
}

static void enable_msr_interception(uint8_t *bitmap, uint32_t msr_arg, uint32_t mode)
{
	uint32_t read_offset = 0U;
	uint32_t write_offset = 2048U;
	uint32_t msr = msr_arg;
	uint8_t msr_bit;
	uint32_t msr_index;

	if ((msr & HIGH_MSR_START) != 0U) {
		read_offset = read_offset + 1024U;
		write_offset = write_offset + 1024U;
	}

	msr &= 0x1FFFU;
	msr_bit = 1U << (msr & 0x7U);
	msr_index = msr >> 3U;

	if ((mode & INTERCEPT_READ) == INTERCEPT_READ) {
		bitmap[read_offset + msr_index] |= msr_bit;
	} else {
		bitmap[read_offset + msr_index] &= ~msr_bit;
	}

	if ((mode & INTERCEPT_WRITE) == INTERCEPT_WRITE) {
		bitmap[write_offset + msr_index] |= msr_bit;
	} else {
		bitmap[write_offset + msr_index] &= ~msr_bit;
	}
}

/*
 * Enable read and write msr interception for x2APIC MSRs
 */
static void intercept_x2apic_msrs(uint8_t *msr_bitmap, uint32_t mode)
{
	uint32_t i;

	for (i = 0U; i < NUM_X2APIC_MSRS; i++) {
		enable_msr_interception(msr_bitmap, x2apic_msrs[i], mode);
	}
}

/**
 * @pre vcpu != NULL
 */
static void init_msr_area(struct acrn_vcpu *vcpu)
{
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value = vcpu->vcpu_id;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value = pcpuid_from_vcpu(vcpu);
}

static void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu);

/**
 * @pre vcpu != NULL
 */
void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	uint32_t msr, i;
	uint64_t value64;

	/* Trap all MSRs by default */
	for (msr = LOW_MSR_START; msr <= LOW_MSR_END; msr++) {
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
	}

	for (msr = HIGH_MSR_START; msr <= HIGH_MSR_END; msr++) {
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
	}

	/* unintercepted_msrs read_write 0_0 */
	for (i = 0U; i < NUM_UNINTERCEPTED_MSRS; i++) {
		enable_msr_interception(msr_bitmap, unintercepted_msrs[i], INTERCEPT_DISABLE);
	}

	/* only intercept wrmsr for MSR_IA32_TIME_STAMP_COUNTER, MSR_IA32_EFER */
	enable_msr_interception(msr_bitmap, MSR_IA32_TIME_STAMP_COUNTER, INTERCEPT_WRITE);
	enable_msr_interception(msr_bitmap, MSR_IA32_EFER, INTERCEPT_WRITE);

	/* handle cases different between safety VM and non-safety VM */
	/* Machine Check */
	if(is_safety_vm(vcpu->vm)) {
		for (msr = MSR_IA32_MC0_CTL2; msr < MSR_IA32_MC4_CTL2; msr++) {
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}

		for (msr = MSR_IA32_MC0_CTL; msr < MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS; msr = msr + 4U) {
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}

		for (msr = MSR_IA32_MC0_STATUS; msr < MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS; msr = msr + 4U) {
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
		}
	}

	update_msr_bitmap_x2apic_passthru(vcpu);

	/* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
	value64 = hva2hpa(vcpu->arch.msr_bitmap);
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	pr_dbg("VMX_MSR_BITMAP: 0x%016lx ", value64);

	/* Initialize the MSR save/store area */
	init_msr_area(vcpu);
}

/* 5 high-order bits in every field are reserved */
#define PAT_FIELD_RSV_BITS (0xF8UL)

static inline bool is_pat_mem_type_invalid(uint64_t x)
{
	return (((x & PAT_FIELD_RSV_BITS) != 0UL) || ((x & 0x6UL) == 0x2UL));
}

static int32_t write_pat_msr(struct acrn_vcpu *vcpu, uint64_t value)
{
	uint32_t i;
	uint64_t field;
	int32_t ret = 0;

	for (i = 0U; i < 8U; i++) {
		field = (value >> (i * 8U)) & 0xffUL;
		if (is_pat_mem_type_invalid(field)) {
			pr_err("invalid guest IA32_PAT: 0x%016lx", value);
			ret = -EINVAL;
			break;
		}
	}

	if (ret == 0) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_PAT, value);

		/*
		 * If context->cr0.CD is set, we defer any further requests to write
		 * guest's IA32_PAT, until the time when guest's CR0.CD is being cleared
		 */
		if ((vcpu_get_cr0(vcpu) & CR0_CD) == 0UL) {
			exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value);
		}
	}

	return ret;
}

static inline bool is_mc_ctl2_msr(uint32_t msr)
{
	return ((msr >= MSR_IA32_MC0_CTL2) && (msr < (MSR_IA32_MC0_CTL2 + NUM_MC_BANKS)));
}

static inline bool is_mc_ctl_msr(uint32_t msr)
{
	return ((msr >= MSR_IA32_MC0_CTL) && (msr < (MSR_IA32_MC0_CTL + 4U * NUM_MC_BANKS)) && ((msr % 4U) == 0U));
}

static inline bool is_mc_status_msr(uint32_t msr)
{
	return ((msr >= MSR_IA32_MC0_STATUS) && (msr < (MSR_IA32_MC0_STATUS + 4U * NUM_MC_BANKS))
			&& ((msr % 4U) == 1U));
}

/**
 * @pre vcpu != NULL
 */
int32_t rdmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
	uint32_t msr;
	uint64_t v = 0UL;

	/* Read the msr value */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/* Do the required processing for each msr case */
	switch (msr) {
	case MSR_IA32_TSC_DEADLINE: {
		v = vlapic_get_tsc_deadline_msr(vcpu_vlapic(vcpu));
		break;
	}
	case MSR_IA32_TSC_ADJUST: {
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID: {
		v = get_microcode_version();
		break;
	}
	case MSR_IA32_PAT: {
		/*
		 * note: if run_ctx->cr0.CD is set, the actual value in guest's
		 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
		 * the saved value guest_msrs[MSR_IA32_PAT]
		 */
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_PAT);
		break;
	}
	case MSR_IA32_APIC_BASE: {
		/* Read APIC base */
		v = vlapic_get_apicbase(vcpu_vlapic(vcpu));
		break;
	}
	case MSR_IA32_FEATURE_CONTROL: {
		v = MSR_IA32_FEATURE_CONTROL_LOCK;
		break;
	}
	case MSR_IA32_MISC_ENABLE: {
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
		break;
	}
	case MSR_IA32_SPEC_CTRL: {
		v = msr_read(MSR_IA32_SPEC_CTRL) & (~MSR_IA32_SPEC_CTRL_STIBP);
		break;
	}
	case MSR_IA32_MONITOR_FILTER_SIZE: {
		v = 0UL;
		break;
	}
	case MSR_IA32_MCG_CAP: {
		if(is_safety_vm(vcpu->vm)) {
			v = MCG_CAP_FOR_SAFETY_VM;
		} else {
			v = 0UL;
		}
		break;
	}

	default: {
		if (is_mc_ctl2_msr(msr) || is_mc_ctl_msr(msr) || is_mc_status_msr(msr)) {
			/* handle Machine Check related MSRs */
			if(is_safety_vm(vcpu->vm)) {
				if ((msr >= MSR_IA32_MC4_CTL2) && (msr <= MSR_IA32_MC9_CTL2)) {
					v = 0UL;
				}
				/* Otherwise, it's not trapped. */
			} else {
				err = -EACCES;
			}
		} else if (is_x2apic_msr(msr)) {
			err = vlapic_x2apic_read(vcpu, msr, &v);
		} else {
			pr_warn("%s(): vm%d vcpu%d reading MSR %lx not supported", __func__, vcpu->vm->vm_id,
				vcpu->vcpu_id, msr);
			err = -EACCES;
			v = 0UL;
		}
		break;
	}
	}

	if (err == 0) {
		/* Store the MSR contents in RAX and RDX */
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, v & 0xffffffffU);
		vcpu_set_gpreg(vcpu, CPU_REG_RDX, v >> 32U);
	}

	TRACE_2L(TRACE_VMEXIT_RDMSR, msr, v);

	return err;
}

/*
 * If VMX_TSC_OFFSET_FULL is 0, no need to trap the write of IA32_TSC_DEADLINE because there is
 * no offset between vTSC and pTSC, in this case, only write to vTSC_ADJUST is trapped.
 */
static void set_tsc_msr_interception(struct acrn_vcpu *vcpu, bool interception)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	bool is_intercepted =
		((msr_bitmap[MSR_IA32_TSC_DEADLINE >> 3U] & (1U << (MSR_IA32_TSC_DEADLINE & 0x7U))) != 0U);

	if (!interception && is_intercepted) {
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_DISABLE);
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_WRITE);
		/* If the timer hasn't expired, sync virtual TSC_DEADLINE to physical TSC_DEADLINE, to make the guest
		 * read the same tsc_deadline as it writes. This may change when the timer actually trigger. If the
		 * timer has expired, write 0 to the virtual TSC_DEADLINE.
		 */
		if (msr_read(MSR_IA32_TSC_DEADLINE) != 0UL) {
			msr_write(MSR_IA32_TSC_DEADLINE, vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE));
		} else {
			vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, 0UL);
		}
	} else if (interception && !is_intercepted) {
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_READ_WRITE);
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_READ_WRITE);
		/* sync physical TSC_DEADLINE to virtual TSC_DEADLINE */
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
 * @pre vcpu != NULL
 */
static void set_guest_tsc(struct acrn_vcpu *vcpu, uint64_t guest_tsc)
{
	uint64_t tsc_delta, tsc_offset_delta, tsc_adjust;

	tsc_delta = guest_tsc - rdtsc();

	/* the delta between new and existing TSC_OFFSET */
	tsc_offset_delta = tsc_delta - exec_vmread64(VMX_TSC_OFFSET_FULL);

	/* apply this delta to TSC_ADJUST */
	tsc_adjust = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust + tsc_offset_delta);

	/* write to VMCS because rdtsc and rdtscp are not intercepted */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_delta);

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
 * @pre vcpu != NULL
 */
static void set_guest_tsc_adjust(struct acrn_vcpu *vcpu, uint64_t tsc_adjust)
{
	uint64_t tsc_offset, tsc_adjust_delta;

	/* delta of the new and existing IA32_TSC_ADJUST */
	tsc_adjust_delta = tsc_adjust - vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);

	/* apply this delta to existing TSC_OFFSET */
	tsc_offset = exec_vmread64(VMX_TSC_OFFSET_FULL);
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_offset + tsc_adjust_delta);

	/* IA32_TSC_ADJUST is supposed to carry the value it's written to */
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust);

	set_tsc_msr_interception(vcpu, (tsc_offset + tsc_adjust_delta) != 0UL);
}

/**
 * @pre vcpu != NULL
 */
static int32_t set_guest_ia32_misc_enable(struct acrn_vcpu *vcpu, uint64_t v)
{
	uint64_t msr_value, guest_ia32_misc_enable, guest_efer;
	int32_t err = 0;

	guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);

	if (((v ^ guest_ia32_misc_enable) & (~MSR_IA32_MISC_ENABLE_MASK)) != 0UL) {
		err = -EACCES;
	} else {
		/* Write value of bit 22 in specified "v" to guest IA32_MISC_ENABLE[bit 22] */
		if (((v ^ guest_ia32_misc_enable) & MSR_IA32_MISC_ENABLE_LIMIT_CPUID) != 0UL) {
			msr_value = guest_ia32_misc_enable & (~MSR_IA32_MISC_ENABLE_LIMIT_CPUID);
			msr_value |= v & MSR_IA32_MISC_ENABLE_LIMIT_CPUID;
			vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, msr_value);
		}

		/* According to SDM Vol4 2.1 & Vol 3A 4.1.4,
		 * EFER.NXE should be cleared if guest disable XD in IA32_MISC_ENABLE
		 */
		if ((v & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
			guest_efer = vcpu_get_efer(vcpu);

			if ((guest_efer & MSR_IA32_EFER_NXE_BIT) != 0UL) {
				vcpu_set_efer(vcpu, guest_efer & ~MSR_IA32_EFER_NXE_BIT);
				/*
				 * When NXE bit is changed, flush TLB entries and paging structure cache entries
				 * applicable to the vCPU.
				 */
				vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
			}
		}
	}

	return err;
}

static int32_t write_efer_msr(struct acrn_vcpu *vcpu, uint64_t value)
{
	int32_t err = 0;
	uint32_t cpuid_xd_feat_flag, eax, ebx, ecx, edx;
	uint64_t guest_efer, efer_changed_bits, new_val;

	guest_efer = vcpu_get_efer(vcpu);
	efer_changed_bits = guest_efer ^ value;

	if ((efer_changed_bits & (~MSR_IA32_EFER_MASK)) != 0UL) {
		/*
		 * Handle invalid write to Reserved bits.
		 *
		 * Modifying Reserved bits causes a general-protection exception (#GP(0)).
		 */
		err = -EACCES;
	} else if (((efer_changed_bits & MSR_IA32_EFER_LME_BIT) != 0UL) && (is_paging_enabled(vcpu))) {
		/*
		 * Handle invalid write to LME bit.
		 *
		 * Modifying LME bit while paging is enabled causes a general-protection exception (#GP(0)).
		 */
		err = -EACCES;
	} else {
		/* Get guest XD Bit extended feature flag (CPUID.80000001H:EDX[20]). */
		eax = CPUID_EXTEND_FUNCTION_1;
		guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
		cpuid_xd_feat_flag = edx & CPUID_EDX_XD_BIT_AVIL;

		if ((cpuid_xd_feat_flag == 0U) && ((value & MSR_IA32_EFER_NXE_BIT) != 0UL)) {
			/*
			 * Handle invalid write to NXE bit.
			 *
			 * Writing NXE bit to 1 when the XD Bit extended feature flag is set to 0
			 * causes a general-protection exception (#GP(0)).
			 */
			err = -EACCES;
		} else {
			new_val = value;
			/* Handle LMA bit (read-only). Write operation is ignored. */
			if ((efer_changed_bits & MSR_IA32_EFER_LMA_BIT) != 0UL) {
				new_val &= ~MSR_IA32_EFER_LMA_BIT;
				new_val |= guest_efer & MSR_IA32_EFER_LMA_BIT;
			}

			vcpu_set_efer(vcpu, new_val);

			if ((efer_changed_bits & MSR_IA32_EFER_NXE_BIT) != 0UL) {
				/*
				 * When NXE bit is changed, flush TLB entries and paging structure cache entries
				 * applicable to the vCPU.
				 */
				vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
			}
		}
	}

	return err;
}

/**
 * @pre vcpu != NULL
 */
int32_t wrmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
	uint32_t msr;
	uint64_t v;

	/* Read the MSR ID */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/* Get the MSR contents */
	v = (vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U) | vcpu_get_gpreg(vcpu, CPU_REG_RAX);

	/* Do the required processing for each msr case */
	switch (msr) {
	case MSR_IA32_TSC_DEADLINE: {
		vlapic_set_tsc_deadline_msr(vcpu_vlapic(vcpu), v);
		break;
	}
	case MSR_IA32_TSC_ADJUST: {
		set_guest_tsc_adjust(vcpu, v);
		break;
	}
	case MSR_IA32_TIME_STAMP_COUNTER: {
		set_guest_tsc(vcpu, v);
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID: {
		/* No operations with "return == 0". */
		break;
	}
	case MSR_IA32_PAT: {
		err = write_pat_msr(vcpu, v);
		break;
	}
	case MSR_IA32_EFER: {
		err = write_efer_msr(vcpu, v);
		break;
	}
	case MSR_IA32_MISC_ENABLE: {
		err = set_guest_ia32_misc_enable(vcpu, v);
		break;
	}
	case MSR_IA32_SPEC_CTRL: {
		msr_write(MSR_IA32_SPEC_CTRL, v & (~MSR_IA32_SPEC_CTRL_STIBP));
		break;
	}
	case MSR_IA32_MONITOR_FILTER_SIZE: {
		/* No operations with "return == 0". */
		break;
	}
	default: {
		if (is_mc_ctl2_msr(msr) || is_mc_ctl_msr(msr) || is_mc_status_msr(msr)) {
			/* handle Machine Check related MSRs */
			if(!is_safety_vm(vcpu->vm)) {
				err = -EACCES;
			}
			/* For safety VM, these MSRs are either not trapped or no operations with "return == 0". */
		} else if (is_x2apic_msr(msr)) {
			err = vlapic_x2apic_write(vcpu, msr, v);
		} else {
			pr_warn("%s(): vm%d vcpu%d writing MSR %lx not supported", __func__, vcpu->vm->vm_id,
				vcpu->vcpu_id, msr);
			err = -EACCES;
		}
		break;
	}
	}

	TRACE_2L(TRACE_VMEXIT_WRMSR, msr, v);

	return err;
}

/*
 * After switch to x2apic mode, most MSRs are passthrough to guest, but vlapic is still valid
 * for virtualization of some MSRs for security consideration:
 * - XAPICID/LDR: Read to XAPICID/LDR need to be trapped to guarantee guest always see right vlapic_id.
 * - ICR: Write to ICR need to be trapped to avoid milicious IPI.
 */

/**
 * @pre vcpu != NULL
 */
static void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;

	intercept_x2apic_msrs(msr_bitmap, INTERCEPT_DISABLE);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_XAPICID, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_LDR, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_ICR, INTERCEPT_READ_WRITE);
	set_tsc_msr_interception(vcpu, exec_vmread64(VMX_TSC_OFFSET_FULL) != 0UL);
}

/**
 * @}
 */
