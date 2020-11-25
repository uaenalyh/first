/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUINFO_H
#define CPUINFO_H

/**
 * @addtogroup hwmgmt_cpu_caps
 *
 * @{
 */

/**
 * @file
 * @brief Declaration of external APIs of the hwmgmt.cpu_caps module
 *
 * This file declares all external functions, data structures, and macros
 * that shall be provided by the hwmgmt.cpu_caps module.
 *
 */

#include <types.h>

/* CPUID feature words */
#define FEAT_1_ECX         0U /**< ECX register returned from the CPUID instruction CPUID.1H */
#define FEAT_1_EDX         1U /**< EDX register returned from the CPUID instruction CPUID.1H */
#define FEAT_7_0_EBX       2U /**< EBX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H) */
#define FEAT_7_0_ECX       3U /**< ECX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H) */
#define FEAT_7_0_EDX       4U /**< EDX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H) */
#define FEAT_8000_0001_ECX 5U /**< ECX register returned from the CPUID instruction CPUID.80000001H */
#define FEAT_8000_0001_EDX 6U /**< EDX register returned from the CPUID instruction CPUID.80000001H */
#define FEAT_8000_0007_EDX 7U /**< EDX register returned from the CPUID instruction CPUID.80000007H */
#define FEAT_8000_0008_EBX 8U /**< EBX register returned from the CPUID instruction CPUID.80000008H */
#define FEATURE_WORDS      9U /**< Total numbers of CPUID feature words */

/**
 * @brief Data Structure to store the CPU information data.
 *
 * Data structure contains the CPU information data.
 * It is supposed to be used when obtaining CPU information.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct cpuinfo_x86 {
	/**
	 * @brief The native processor display family value.
	 */
	uint8_t displayfamily;
	/**
	 * @brief The native processor display model value.
	 */
	uint8_t displaymodel;
	/**
	 * @brief Linear address size, number of address bits supported
	 * by the CPU for linear address.
	 */
	uint8_t virt_bits;
	/**
	 * @brief Physical address size, number of address bits supported
	 * by the CPU for physical address.
	 */
	uint8_t phys_bits;
	/**
	 * @brief Maximum input value for basic CPUID information.
	 */
	uint32_t cpuid_level;
	/**
	 * @brief Maximum input value for extended function CPUID information.
	 */
	uint32_t extended_cpuid_level;
	/**
	 * @brief Contains various feature details returned from a CPUID instruction with different
	 * contents in EAX register and ECX register upon execution of the instruction.
	 *
	 * cpuid_leaves[0] contains the contents of
	 * ECX register returned from the CPUID instruction CPUID.1H
	 *
	 * cpuid_leaves[1] contains the contents of
	 * EDX register returned from the CPUID instruction CPUID.1H
	 *
	 * cpuid_leaves[2] contains the contents of
	 * EBX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H)
	 *
	 * cpuid_leaves[3] contains the contents of
	 * ECX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H)
	 *
	 * cpuid_leaves[4] contains the contents of
	 * EDX register returned from the CPUID instruction CPUID.(EAX=7H,ECX=0H)
	 *
	 * cpuid_leaves[5] contains the contents of
	 * ECX register returned from the CPUID instruction CPUID.80000001H
	 *
	 * cpuid_leaves[6] contains the contents of
	 * EDX register returned from the CPUID instruction CPUID.80000001H
	 *
	 * cpuid_leaves[7] contains the contents of
	 * EDX register returned from the CPUID instruction CPUID.80000007H
	 *
	 * cpuid_leaves[8] contains the contents of
	 * EBX register returned from the CPUID instruction CPUID.80000008H
	 */
	uint32_t cpuid_leaves[FEATURE_WORDS];
	/**
	 * @brief Model name of the processor.
	 */
	char model_name[64];
};

bool has_monitor_cap(void);
bool pcpu_has_cap(uint32_t bit);
void init_pcpu_capabilities(void);
void init_pcpu_model_name(void);
struct cpuinfo_x86 *get_pcpu_info(void);

/**
 * @}
 */

#endif /* CPUINFO_H */
