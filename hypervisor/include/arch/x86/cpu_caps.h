/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUINFO_H
#define CPUINFO_H

/* type of speculation control
 * 0 - no speculation control support
 * 1 - raw IBRS + IPBP support
 * 2 - with STIBP optimization support
 */
#define IBRS_NONE	0
#define IBRS_RAW	1
#define IBRS_OPT	2

#ifndef ASSEMBLER

extern int32_t ibrs_type;

struct cpu_state_info {
	uint8_t			 px_cnt;	/* count of all Px states */
	const struct cpu_px_data *px_data;
	uint8_t			 cx_cnt;	/* count of all Cx entries */
	const struct cpu_cx_data *cx_data;
};

#define MAX_PSTATE	20U	/* max num of supported Px count */
#define MAX_CSTATE	8U	/* max num of supported Cx count */
/* We support MAX_CSTATE num of Cx, means have (MAX_CSTATE - 1) Cx entries,
 * i.e. supported Cx entry index range from 1 to MAX_CX_ENTRY.
 */
#define MAX_CX_ENTRY	(MAX_CSTATE - 1U)

/* CPUID feature words */
#define	FEAT_1_ECX		0U     /* CPUID[1].ECX */
#define	FEAT_1_EDX		1U     /* CPUID[1].EDX */
#define	FEAT_7_0_EBX		2U     /* CPUID[EAX=7,ECX=0].EBX */
#define	FEAT_7_0_ECX		3U     /* CPUID[EAX=7,ECX=0].ECX */
#define	FEAT_7_0_EDX		4U     /* CPUID[EAX=7,ECX=0].EDX */
#define	FEAT_8000_0001_ECX	5U     /* CPUID[8000_0001].ECX */
#define	FEAT_8000_0001_EDX	6U     /* CPUID[8000_0001].EDX */
#define	FEAT_8000_0008_EBX	7U     /* CPUID[8000_0008].EAX */
#define	FEATURE_WORDS		8U

struct cpuinfo_x86 {
	uint8_t family, model;
	uint8_t virt_bits;
	uint8_t phys_bits;
	uint32_t cpuid_level;
	uint32_t extended_cpuid_level;
	uint64_t physical_address_mask;
	uint32_t cpuid_leaves[FEATURE_WORDS];
	char model_name[64];
	struct cpu_state_info state_info;
};

extern struct cpuinfo_x86 boot_cpu_data;

bool get_monitor_cap(void);
bool is_apicv_reg_virtualization_supported(void);
bool is_apicv_intr_delivery_supported(void);
bool is_apicv_posted_intr_supported(void);
bool is_ept_supported(void);
bool cpu_has_cap(uint32_t bit);
bool cpu_has_vmx_ept_cap(uint32_t bit_mask);
bool cpu_has_vmx_vpid_cap(uint32_t bit_mask);
void get_cpu_capabilities(void);
void get_cpu_name(void);
void cpu_cap_detect(void);
bool check_cpu_security_config(void);
void cpu_l1d_flush(void);
int hardware_detect_support(void);

#endif /* ASSEMBLER */

#endif /* CPUINFO_H */
