/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <msr.h>
#include <page.h>
#include <cpufeatures.h>
#include <cpuid.h>
#include <cpu.h>
#include <per_cpu.h>
#include <vmx.h>
#include <cpu_caps.h>
#include <errno.h>
#include <logmsg.h>
#include <vmcs.h>

/**
 * @defgroup hwmgmt_cpu_caps hwmgmt.cpu_caps
 * @ingroup hwmgmt
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

/* TODO: add more capability per requirement */

static struct cpu_capability {
	uint32_t vmx_ept;
	uint32_t vmx_vpid;
} cpu_caps;

static struct cpuinfo_x86 boot_cpu_data;

bool pcpu_has_cap(uint32_t bit)
{
	uint32_t feat_idx = bit >> 5U;
	uint32_t feat_bit = bit & 0x1fU;
	bool ret;

	if (feat_idx >= FEATURE_WORDS) {
		ret = false;
	} else {
		ret = ((boot_cpu_data.cpuid_leaves[feat_idx] & (1U << feat_bit)) != 0U);
	}

	return ret;
}

bool monitor_cap_buggy(void)
{
	bool buggy = false;

	if ((boot_cpu_data.family == 0x6U) && (boot_cpu_data.model == 0x5cU)) {
		buggy = true;
	}

	return buggy;
}

bool has_monitor_cap(void)
{
	bool ret = false;

	if (pcpu_has_cap(X86_FEATURE_MONITOR)) {
		/* don't use monitor for CPU (family: 0x6 model: 0x5c)
		 * in hypervisor, but still expose it to the guests and
		 * let them handle it correctly
		 */
		if (!monitor_cap_buggy()) {
			ret = true;
		}
	}

	return ret;
}

static void detect_vmx_mmu_cap(void)
{
	uint64_t val;

	/* Read the MSR register of EPT and VPID Capability -  SDM A.10 */
	val = msr_read(MSR_IA32_VMX_EPT_VPID_CAP);
	cpu_caps.vmx_ept = (uint32_t)val;
	cpu_caps.vmx_vpid = (uint32_t)(val >> 32U);
}

static void detect_xsave_cap(void)
{
	uint32_t unused;

	cpuid_subleaf(CPUID_XSAVE_FEATURES, 0U, &boot_cpu_data.cpuid_leaves[FEAT_D_0_EAX], &unused, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_D_0_EDX]);
	cpuid_subleaf(CPUID_XSAVE_FEATURES, 1U, &boot_cpu_data.cpuid_leaves[FEAT_D_1_EAX], &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_D_1_ECX], &boot_cpu_data.cpuid_leaves[FEAT_D_1_EDX]);
}

static void detect_pcpu_cap(void)
{
	detect_vmx_mmu_cap();
	detect_xsave_cap();
}

static uint64_t get_address_mask(uint8_t limit)
{
	return ((1UL << limit) - 1UL) & PAGE_MASK;
}

void init_pcpu_capabilities(void)
{
	uint32_t eax, unused;
	uint32_t family, model;

	cpuid(CPUID_VENDORSTRING, &boot_cpu_data.cpuid_level, &unused, &unused, &unused);

	cpuid(CPUID_FEATURES, &eax, &unused, &boot_cpu_data.cpuid_leaves[FEAT_1_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_1_EDX]);
	family = (eax >> 8U) & 0xfU;
	if (family == 0xFU) {
		family += ((eax >> 20U) & 0xffU) << 4U;
	}
	boot_cpu_data.family = (uint8_t)family;

	model = (eax >> 4U) & 0xfU;
	if ((family == 0x06U) || (family == 0xFU)) {
		model += ((eax >> 16U) & 0xfU) << 4U;
	}
	boot_cpu_data.model = (uint8_t)model;

	cpuid(CPUID_EXTEND_FEATURE, &unused, &boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX], &boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

	cpuid(CPUID_MAX_EXTENDED_FUNCTION, &boot_cpu_data.extended_cpuid_level, &unused, &unused, &unused);

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_FUNCTION_1) {
		cpuid(CPUID_EXTEND_FUNCTION_1, &unused, &unused, &boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX]);
	}

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_INVA_TSC) {
		cpuid(CPUID_EXTEND_INVA_TSC, &eax, &unused, &unused, &boot_cpu_data.cpuid_leaves[FEAT_8000_0007_EDX]);
	}

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_ADDRESS_SIZE) {
		cpuid(CPUID_EXTEND_ADDRESS_SIZE, &eax, &boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX], &unused,
			&unused);

		/* EAX bits 07-00: #Physical Address Bits
		 *     bits 15-08: #Linear Address Bits
		 */
		boot_cpu_data.virt_bits = (uint8_t)((eax >> 8U) & 0xffU);
		boot_cpu_data.phys_bits = (uint8_t)(eax & 0xffU);
		boot_cpu_data.physical_address_mask = get_address_mask(boot_cpu_data.phys_bits);
	}

	detect_pcpu_cap();
}

bool pcpu_has_vmx_ept_cap(uint32_t bit_mask)
{
	return ((cpu_caps.vmx_ept & bit_mask) != 0U);
}

void init_pcpu_model_name(void)
{
	cpuid(CPUID_EXTEND_FUNCTION_2, (uint32_t *)(boot_cpu_data.model_name),
		(uint32_t *)(&boot_cpu_data.model_name[4]), (uint32_t *)(&boot_cpu_data.model_name[8]),
		(uint32_t *)(&boot_cpu_data.model_name[12]));
	cpuid(CPUID_EXTEND_FUNCTION_3, (uint32_t *)(&boot_cpu_data.model_name[16]),
		(uint32_t *)(&boot_cpu_data.model_name[20]), (uint32_t *)(&boot_cpu_data.model_name[24]),
		(uint32_t *)(&boot_cpu_data.model_name[28]));
	cpuid(CPUID_EXTEND_FUNCTION_4, (uint32_t *)(&boot_cpu_data.model_name[32]),
		(uint32_t *)(&boot_cpu_data.model_name[36]), (uint32_t *)(&boot_cpu_data.model_name[40]),
		(uint32_t *)(&boot_cpu_data.model_name[44]));

	boot_cpu_data.model_name[48] = '\0';
}

struct cpuinfo_x86 *get_pcpu_info(void)
{
	return &boot_cpu_data;
}

/**
 * @}
 */
