/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vcpu.h>
#include <vm.h>
#include <cpuid.h>
#include <cpufeatures.h>
#include <vmx.h>
#include <sgx.h>
#include <logmsg.h>

/**
 * @defgroup vp-base_vcpuid vp-base.vcpuid
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

static inline const struct vcpuid_entry *local_find_vcpuid_entry(
	const struct acrn_vcpu *vcpu, uint32_t leaf, uint32_t subleaf)
{
	uint32_t i = 0U, nr, half;
	const struct vcpuid_entry *found_entry = NULL;
	struct acrn_vm *vm = vcpu->vm;

	nr = vm->vcpuid_entry_nr;
	half = nr >> 1U;
	if (vm->vcpuid_entries[half].leaf < leaf) {
		i = half;
	}

	for (; i < nr; i++) {
		const struct vcpuid_entry *tmp = (const struct vcpuid_entry *)(&vm->vcpuid_entries[i]);

		if (tmp->leaf < leaf) {
			continue;
		} else if (tmp->leaf == leaf) {
			if (((tmp->flags & CPUID_CHECK_SUBLEAF) != 0U) && (tmp->subleaf != subleaf)) {
				continue;
			}
			found_entry = tmp;
			break;
		} else {
			/* tmp->leaf > leaf */
			break;
		}
	}

	return found_entry;
}

static inline const struct vcpuid_entry *find_vcpuid_entry(
	const struct acrn_vcpu *vcpu, uint32_t leaf_arg, uint32_t subleaf)
{
	const struct vcpuid_entry *entry;
	uint32_t leaf = leaf_arg;

	entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
	if (entry == NULL) {
		uint32_t limit;
		struct acrn_vm *vm = vcpu->vm;

		if ((leaf & CPUID_MAX_EXTENDED_FUNCTION) != 0U) {
			limit = vm->vcpuid_xlevel;
		} else {
			limit = vm->vcpuid_level;
		}

		if (leaf > limit) {
			/* Intel documentation states that invalid EAX input
			 * will return the same information as EAX=cpuid_level
			 * (Intel SDM Vol. 2A - Instruction Set Reference -
			 * CPUID)
			 */
			leaf = vm->vcpuid_level;
			entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
		}
	}

	return entry;
}

static inline void set_vcpuid_entry(struct acrn_vm *vm, const struct vcpuid_entry *entry)
{
	struct vcpuid_entry *tmp;
	size_t entry_size = sizeof(struct vcpuid_entry);

	tmp = &vm->vcpuid_entries[vm->vcpuid_entry_nr];
	vm->vcpuid_entry_nr++;
	(void)memcpy_s(tmp, entry_size, entry, entry_size);

	return;
}

#define L2_WAYS_OF_ASSOCIATIVITY	4U

/**
 * initialization of virtual CPUID leaf
 */
static void init_vcpuid_entry(uint32_t leaf, uint32_t subleaf, uint32_t flags, struct vcpuid_entry *entry)
{
#ifdef QEMU
	struct cpuinfo_x86 *cpu_info;
#endif
	entry->leaf = leaf;
	entry->subleaf = subleaf;
	entry->flags = flags;

	switch (leaf) {
	case 0x06U:
		entry->eax = CPUID_EAX_ARAT;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;

	case 0x07U:
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		/* mask invpcid */
		entry->ebx &= ~(CPUID_EBX_INVPCID | CPUID_EBX_PQM | CPUID_EBX_PQE);

		/* mask SGX and SGX_LC */
		entry->ebx &= ~CPUID_EBX_SGX;
		entry->ecx &= ~CPUID_ECX_SGX_LC;

		/* mask MPX */
		entry->ebx &= ~CPUID_EBX_MPX;

		/* mask Intel Processor Trace, since 14h is disabled */
		entry->ebx &= ~CPUID_EBX_PROC_TRC;

		/* mask STIBP */
		entry->edx &= ~CPUID_EDX_STIBP;

		/* TODO: May be revised later */
		/* mask TSX_FORCE_ABORT */
		entry->edx &= ~CPUID_EDX_TSX_FORCE_ABORT;
		break;

	case 0x15U:
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		entry->ecx = VIRT_CRYSTAL_CLOCK_FREQ;
		break;

#ifdef QEMU
	case 0x16U:
		cpu_info = get_pcpu_info();
		if (cpu_info->cpuid_level >= 0x16U) {
			/* call the cpuid when 0x16 is supported */
			cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		} else {
			/* Use the tsc to derive the emulated 0x16U cpuid. */
			entry->eax = (uint32_t)(get_tsc_khz() / 1000U);
			entry->ebx = entry->eax;
			/* Bus frequency: hard coded to 100M */
			entry->ecx = 100U;
			entry->edx = 0U;
		}
		break;
#endif

	case 0x80000006U:
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		entry->ecx = (entry->ecx & ~CPUID_ECX_L2_ASSOCIATIVITY_FIELD_MASK) |
				(L2_WAYS_OF_ASSOCIATIVITY << CPUID_ECX_L2_ASSOCIATIVITY_FIELD_POS);
		break;

	default:
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		break;
	}
}

static void set_vcpuid_extended_function(struct acrn_vm *vm)
{
	uint32_t i, limit;
	struct vcpuid_entry entry;

	init_vcpuid_entry(CPUID_MAX_EXTENDED_FUNCTION, 0U, 0U, &entry);
	set_vcpuid_entry(vm, &entry);

	limit = entry.eax;
	vm->vcpuid_xlevel = limit;
	for (i = CPUID_EXTEND_FUNCTION_2; i <= limit; i++) {
		init_vcpuid_entry(i, 0U, 0U, &entry);
		set_vcpuid_entry(vm, &entry);
	}

	return;
}

void set_vcpuid_entries(struct acrn_vm *vm)
{
	struct vcpuid_entry entry;
	uint32_t limit;
	uint32_t i, j;

	init_vcpuid_entry(0U, 0U, 0U, &entry);
#ifdef QEMU
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	if (cpu_info->cpuid_level < 0x16U) {
		/* The cpuid with zero leaf returns the max level. Emulate that the 0x16U is supported */
		entry.eax = 0x16U;
	}
#endif
	set_vcpuid_entry(vm, &entry);

	limit = entry.eax;
	vm->vcpuid_level = limit;

	for (i = 1U; i <= limit; i++) {
		/* cpuid 1/0xb is percpu related */
		if ((i == 1U) || (i == 0xbU) || (i == 0xdU)) {
			continue;
		}

		switch (i) {
		case 0x04U:
			for (j = 0U;; j++) {
				init_vcpuid_entry(i, j, CPUID_CHECK_SUBLEAF, &entry);
				if (entry.eax == 0U) {
					break;
				}
				set_vcpuid_entry(vm, &entry);
			}
			break;
		case 0x07U:
			init_vcpuid_entry(i, 0U, CPUID_CHECK_SUBLEAF, &entry);
			set_vcpuid_entry(vm, &entry);
			break;
		/* These features are disabled */
		case 0x05U: /* Monitor/Mwait */
		case 0x08U: /* unimplemented leaf */
		case 0x09U: /* Cache */
		case 0x0aU: /* PMU is not supported */
		case 0x0cU: /* unimplemented leaf */
		case 0x0eU: /* unimplemented leaf */
		case 0x0fU: /* Intel RDT */
		case 0x10U: /* Intel RDT */
		case 0x11U: /* unimplemented leaf */
		case 0x12U: /* SGX */
		case 0x13U: /* unimplemented leaf */
		case 0x14U: /* Intel Processor Trace */
			break;
		default:
			init_vcpuid_entry(i, 0U, 0U, &entry);
			set_vcpuid_entry(vm, &entry);
			break;
		}
	}

	set_vcpuid_extended_function(vm);

	return;
}

static inline bool is_percpu_related(uint32_t leaf)
{
	return ((leaf == 0x1U) || (leaf == 0xbU) || (leaf == 0xdU) || (leaf == 0x80000001U));
}

static void guest_cpuid_01h(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));
	uint64_t cr4;

	cpuid(0x1U, eax, ebx, ecx, edx);

	/* Patching initial APIC ID */
	*ebx &= ~APIC_ID_MASK;
	*ebx |= (apicid << APIC_ID_SHIFT);

	/* mask MONITOR/MWAIT */
	*ecx &= ~CPUID_ECX_MONITOR;

	/* mask Debug Store feature */
	*ecx &= ~(CPUID_ECX_DTES64 | CPUID_ECX_DS_CPL);

	/* mask Safer Mode Extension */
	*ecx &= ~CPUID_ECX_SMX;

	/* mask Enhanced Intel SpeedStep technology */
	*ecx &= ~CPUID_ECX_EST;

	/* mask Thermal Monitor 2 */
	*ecx &= ~CPUID_ECX_TM2;

	/* mask PDCM: Perfmon and Debug Capability */
	*ecx &= ~CPUID_ECX_PDCM;

	/* mask SDBG for silicon debug */
	*ecx &= ~CPUID_ECX_SDBG;

	/* mask pcid */
	*ecx &= ~CPUID_ECX_PCID;

	/*mask vmx to guest os */
	*ecx &= ~CPUID_ECX_VMX;

	/* read guest CR4, set CPUID_ECX_OSXSAVE only if guest sets OSXSAVE in CR4 */
	cr4 = exec_vmread(VMX_GUEST_CR4);
	*ecx &= ~CPUID_ECX_OSXSAVE;
	if ((cr4 & CR4_OSXSAVE) != 0UL) {
		*ecx |= CPUID_ECX_OSXSAVE;
	}

	/* mask Debug Store feature */
	*edx &= ~CPUID_EDX_DTES;

	/* mask Virtual 8086 Mode Enhancements */
	*edx &= ~CPUID_EDX_VME;

	/* mask Debugging Extensions */
	*edx &= ~CPUID_EDX_DE;

	/* mask mtrr */
	*edx &= ~CPUID_EDX_MTRR;

	/* mask ACPI */
	*edx &= ~CPUID_EDX_ACPI;

	/* mask Thermal Monitor */
	*edx &= ~CPUID_EDX_TM1;

	/* mask Pending Break Enable */
	*edx &= ~CPUID_EDX_PBE;

	if(is_safety_vm(vcpu->vm)) {
		/* mask HTT */
		*edx &= ~CPUID_EDX_HTT;
	} else {
		/* mask MCE */
		*edx &= ~CPUID_EDX_MCE;

		/* mask MCA */
		*edx &= ~CPUID_EDX_MCA;
	}
}

static void guest_cpuid_0bh(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t subleaf = *ecx;

	/* Patching X2APIC */
	*ecx = subleaf & 0xFFU;
	/* No HT emulation for UOS */
	switch (subleaf) {
	case 0U:
		*eax = 0U;
		*ebx = 1U;
		*ecx |= (1U << 8U);
		break;
	case 1U:
		if (vcpu->vm->hw.created_vcpus == 1U) {
			*eax = 0U;
		} else {
			*eax = (uint32_t)fls32(vcpu->vm->hw.created_vcpus - 1U) + 1U;
		}
		*ebx = vcpu->vm->hw.created_vcpus;
		*ecx |= (2U << 8U);
		break;
	default:
		*eax = 0U;
		*ebx = 0U;
		*ecx |= (0U << 8U);
		break;
	}
	*edx = vlapic_get_apicid(vcpu_vlapic(vcpu));
}

static void guest_cpuid_0dh(__unused struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t subleaf = *ecx;
	uint32_t avx_area_size, unused;

	cpuid_subleaf(0x0dU, 2U, &avx_area_size, &unused, &unused, &unused);

	cpuid_subleaf(0x0dU, subleaf, eax, ebx, ecx, edx);

	switch (subleaf) {
	case 0U:
		/* SDM Vol.1 17-2, On processors that do not support Intel MPX,
		 * CPUID.(EAX=0DH,ECX=0):EAX[3] and
		 * CPUID.(EAX=0DH,ECX=0):EAX[4] will both be 0 */
		*eax &= ~CPUID_EAX_XCR0_BNDREGS;
		*eax &= ~CPUID_EAX_XCR0_BNDCSR;

		/*
		 * Correct EBX depends on correct initialization value of physical XCR0 and MSR IA32_XSS.
		 * Physical XCR0 shall be initialized to 1H.
		 * Physical MSR IA32_XSS shall be initialized to 0H.
		 */

		*ecx = XSAVE_LEGACY_AREA_SIZE + XSAVE_HEADER_AREA_SIZE + avx_area_size;
		break;

	case 1U:
		/* mask XSAVES/XRSTORS instructions */
		*eax &= ~CPUID_EAX_XSAVES;

		/*
		 * Correct EBX depends on correct initialization value of physical XCR0 and MSR IA32_XSS.
		 * Physical XCR0 shall be initialized to 1H.
		 * Physical MSR IA32_XSS shall be initialized to 0H.
		 */

		/* mask PT STATE */
		*ecx &= ~CPUID_ECX_PT_STATE;
		break;

	case 2U:
		/* AVX state: return native value */
		break;

	default:
		/* hide all the other state */
		*eax = 0U;
		*ebx = 0U;
		*ecx = 0U;
		*edx = 0U;
		break;
	}
}

static void guest_cpuid_80000001h(
	const struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	uint32_t leaf = 0x80000001U;

	cpuid(leaf, eax, ebx, ecx, edx);
	/* SDM Vol4 2.1, XD Bit Disable of MSR_IA32_MISC_ENABLE
	 * When set to 1, the Execute Disable Bit feature (XD Bit) is disabled and the XD Bit
	 * extended feature flag will be clear (CPUID.80000001H: EDX[20]=0)
	 */
	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
		*edx = *edx & ~CPUID_EDX_XD_BIT_AVIL;
	}
}

static void guest_limit_cpuid(
	const struct acrn_vcpu *vcpu, uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	const struct vcpuid_entry *entry;

	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_LIMIT_CPUID) != 0UL) {
		/* limit the leaf number to 2 */
		if (leaf == 0U) {
			*eax = 2U;
		} else if (((leaf > 2U) && (leaf < CPUID_MAX_EXTENDED_FUNCTION)) || (leaf > vcpu->vm->vcpuid_xlevel)) {
			entry = find_vcpuid_entry(vcpu, 2U, 0U);
			*eax = entry->eax;
			*ebx = entry->ebx;
			*ecx = entry->ecx;
			*edx = entry->edx;
		} else {
			/* In this case,
			 * leaf is 1H, 2H, or extended function leaves in the range [80000000H, 80000008H],
			 * return the cpuid value get previously.
			 */
		}
	}
}

void guest_cpuid(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t leaf = *eax;
	uint32_t subleaf = *ecx;

	/* vm related */
	if (!is_percpu_related(leaf)) {
		const struct vcpuid_entry *entry = find_vcpuid_entry(vcpu, leaf, subleaf);

		if (entry != NULL) {
			*eax = entry->eax;
			*ebx = entry->ebx;
			*ecx = entry->ecx;
			*edx = entry->edx;
		} else {
			*eax = 0U;
			*ebx = 0U;
			*ecx = 0U;
			*edx = 0U;
		}
	} else {
		/* percpu related */
		switch (leaf) {
		case 0x01U:
			guest_cpuid_01h(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x0bU:
			guest_cpuid_0bh(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x0dU:
			guest_cpuid_0dh(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x80000001U:
			guest_cpuid_80000001h(vcpu, eax, ebx, ecx, edx);
			break;

		default:
			/*
			 * In this switch statement, leaf shall either be 0x01U or 0x0bU
			 * or 0x0dU or 0x80000001U. All the other cases have been handled properly
			 * before this switch statement.
			 * Gracefully return if prior case clauses have not been met.
			 */
			break;
		}
	}

	guest_limit_cpuid(vcpu, leaf, eax, ebx, ecx, edx);
}

/**
 * @}
 */
