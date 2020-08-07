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
#include <cpu_caps.h>

/**
 * @defgroup hwmgmt_cpu_caps hwmgmt.cpu_caps
 * @ingroup hwmgmt
 * @brief The definition and implementation of CPU capabilities related stuff.
 *
 * It provides external APIs for manipulating CPU capabilities information.
 *
 * Usage:
 * - 'hwmgmt.security' depends on this module to check physical platform security capability
 *   and get the CPU information.
 * - 'hwmgmt.cpu' depends on this module to initialize CPU capabilities, initialize processor model name,
 *   get the CPU information and check CPU monitor capability.
 * - 'vp-base.vcpu' depends on this module to get the CPU information.
 * - 'vp-base.vcpuid' depends on this module to get the CPU information.
 * - 'vp-base.vcr' depends on this module to get the CPU information.
 *
 * Dependency:
 * - This module depends on 'hwmgmt.cpu' module to read the MSR register and use CPUID related APIs.
 * - This module depends on 'hwmgmt.page' module to get page related information.
 * - This module depends on 'lib.utils' module to use type related macros.
 *
 * @{
 */

/**
 * @file
 * @brief This file contains the data structures and functions for the hwmgmt.cpu_caps module.
 *
 * This file implements all following external APIs that shall be provided
 * by the hwmgmt.cpu_caps module.
 * - pcpu_has_cap
 * - has_monitor_cap
 * - init_pcpu_capabilities
 * - init_pcpu_model_name
 * - get_pcpu_info
 *
 */


/**
 * @brief Global variable storing the CPU information data.
 *
 * Declare static variable boot_cpu_data with type 'struct cpuinfo_x86'
 * to store the CPU information data.
 */
static struct cpuinfo_x86 boot_cpu_data;

/**
 * @brief The interface checks whether a specified feature is supported by the CPU or not.
 *
 * @param[in] bit The bit that specifies the feature to check.
 * High 27 bits indicate which register and which CPUID leaf shall be checked.
 * It is the index of array boot_cpu_data.cpuid_leaves.
 * Low 5 bits indicate which bit of the register shall be checked.
 *
 * @return A Boolean value indicating whether a specified feature is supported by the CPU.
 *
 * @retval true The specified feature is supported by the CPU.
 * @retval false The specified feature is not cached in array boot_cpu_data.cpuid_leaves
 * or the specified feature is not supported by the CPU.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT
 *
 * @remark This API shall be called after init_pcpu_capabilities has been called once on the bootstrap processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
bool pcpu_has_cap(uint32_t bit)
{
	/** Declare the following local variables of type uint32_t.
	 *  - feat_idx representing the index of array boot_cpu_data.cpuid_leaves,
	 *  initialized as (\a bit >> 5). */
	uint32_t feat_idx = bit >> 5U;
	/** Declare the following local variables of type uint32_t.
	 *  - feat_bit representing the bit index of 'boot_cpu_data.cpuid_leaves[feat_idx]',
	 *  initialized as (\a bit & 1fH). */
	uint32_t feat_bit = bit & 0x1fU;
	/** Declare the following local variables of type bool.
	 *  - ret indicating whether the specified feature is supported by the CPU, not initialized */
	bool ret;

	/** If feat_idx is greater or equal to FEATURE_WORDS,
	 *  meaning feat_idx is out of bounds of array boot_cpu_data.cpuid_leaves */
	if (feat_idx >= FEATURE_WORDS) {
		/** Set ret to false,
		 *  Meaning the specified feature is not cached in array boot_cpu_data.cpuid_leaves */
		ret = false;
	} else {
		/** Set ret to ((boot_cpu_data.cpuid_leaves[feat_idx] & (1 << feat_bit)) != 0).
		 *  If bit 'feat_bit' of 'boot_cpu_data.cpuid_leaves[feat_idx]' is set,
		 *  then the specified feature is supported by the CPU, set ret to true.
		 *  If bit 'feat_bit' of 'boot_cpu_data.cpuid_leaves[feat_idx]' is not set,
		 *  then the specified feature is not supported by the CPU, set ret to false. */
		ret = ((boot_cpu_data.cpuid_leaves[feat_idx] & (1U << feat_bit)) != 0U);
	}

	/** Return the value of ret.
	 *  If the specified feature is supported by the CPU, return true.
	 *  Otherwise, return false.
	 */
	return ret;
}

/**
 * @brief The interface checks whether the CPU supports MONITOR/MWAIT instructions.
 *
 * @return A Boolean value indicating whether the CPU supports MONITOR/MWAIT instructions.
 *
 * @retval true  The CPU supports MONITOR/MWAIT instructions.
 * @retval false The CPU doesn't support MONITOR/MWAIT instructions.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark This API shall be called after init_pcpu_capabilities has been called once on the bootstrap processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
bool has_monitor_cap(void)
{
	/** Return pcpu_has_cap(X86_FEATURE_MONITOR).
	 *  If CPU supports MONITOR/MWAIT instructions, return true.
	 *  Otherwise, return false. */
	return pcpu_has_cap(X86_FEATURE_MONITOR);
}

/**
 * @brief The interface initializes an internal data structure that stores the CPU information data.
 * These information data could be accessed by other modules by invoking "get_pcpu_info".
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_pcpu_capabilities(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - eax representing the value of EAX register, not initialized. */
	uint32_t eax;
	/** Declare the following local variables of type uint32_t.
	 *  - unused representing a variable used as a placeholder of
	 *  unused CPUID information upon 'cpuid' calls, not initialized. */
	uint32_t unused;
	/** Declare the following local variables of type uint32_t.
	 *  - family_id representing the native processor family id, not initialized. */
	uint32_t family_id;
	/** Declare the following local variables of type uint32_t.
	 *  - model_id representing the native processor model id, not initialized. */
	uint32_t model_id;
	/** Declare the following local variables of type uint32_t.
	 *  - displayfamily representing the native processor display family, not initialized. */
	uint32_t displayfamily;
	/** Declare the following local variables of type uint32_t.
	 *  - displaymodel representing the native processor display model, not initialized. */
	uint32_t displaymodel;

	/** Call cpuid with the following parameters, in order to get the native processor information for CPUID.0H
	 *  and fill native CPUID.0H:EAX into boot_cpu_data.cpuid_level.
	 *  - CPUID_VENDORSTRING
	 *  - &boot_cpu_data.cpuid_level
	 *  - &unused
	 *  - &unused
	 *  - &unused */
	cpuid(CPUID_VENDORSTRING, &boot_cpu_data.cpuid_level, &unused, &unused, &unused);

	/** Call cpuid with the following parameters, in order to get the native processor information for CPUID.1H
	 *  and fill native CPUID.1H:EAX into eax, fill native CPUID.1H:ECX into boot_cpu_data.cpuid_leaves[FEAT_1_ECX],
	 *  fill native CPUID.1H:EDX into boot_cpu_data.cpuid_leaves[FEAT_1_EDX].
	 *  - CPUID_FEATURES
	 *  - &eax
	 *  - &unused
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_1_ECX]
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_1_EDX] */
	cpuid(CPUID_FEATURES, &eax, &unused, &boot_cpu_data.cpuid_leaves[FEAT_1_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_1_EDX]);
	/** Set 'family_id' to ((eax >> 8) & fH) */
	family_id = (eax >> 8U) & 0xfU;
	/** Set 'displayfamily' to 'family_id' */
	displayfamily = family_id;
	/** If native processor family ID (specified by 'family_id') is FH */
	if (family_id == 0xFU) {
		/** Update the value of 'displayfamily', increased by
		 *  the native processor extended family ID (calculated by ((eax >> 20) & ffH)) */
		displayfamily += ((eax >> 20U) & 0xffU);
	}
	/** Set the displayfamily field of boot_cpu_data to lower 8 bits of 'displayfamily' */
	boot_cpu_data.displayfamily = (uint8_t)displayfamily;
	/** Set 'model_id' to ((eax >> 4) & fH)  */
	model_id = (eax >> 4U) & 0xfU;
	/** Set 'displaymodel' to 'model_id' */
	displaymodel = model_id;
	/** If native processor family id (specified by 'family_id') is 6H or FH */
	if ((family_id == 0x06U) || (family_id == 0xFU)) {
		/** Update the value of 'displaymodel', bits 7:4 of the new value is
		 *  the native processor extended model ID (calculated by ((eax >> 16) & fH)),
		 *  bits 3:0 of the new value is the native processor model ID (calculated by ((eax >> 4) & fH)) */
		displaymodel += ((eax >> 16U) & 0xfU) << 4U;
	}
	/** Set the displaymodel field of boot_cpu_data to lower 8 bits of 'displaymodel' */
	boot_cpu_data.displaymodel = (uint8_t)displaymodel;

	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.(EAX=7H,ECX=0H)
	 *  and fill native CPUID.(EAX=7H,ECX=0H):EBX into boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
	 *  fill native CPUID.(EAX=7H,ECX=0H):ECX into boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX],
	 *  fill native CPUID.(EAX=7H,ECX=0H):EDX into boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX].
	 *  - CPUID_EXTEND_FEATURE
	 *  - &unused
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX]
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX]
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX] */
	cpuid(CPUID_EXTEND_FEATURE, &unused, &boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX], &boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000000H
	 *  and fill native CPUID.80000000H:EAX into boot_cpu_data.extended_cpuid_level.
	 *  - CPUID_MAX_EXTENDED_FUNCTION
	 *  - &boot_cpu_data.extended_cpuid_level
	 *  - &unused
	 *  - &unused
	 *  - &unused */
	cpuid(CPUID_MAX_EXTENDED_FUNCTION, &boot_cpu_data.extended_cpuid_level, &unused, &unused, &unused);

	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000001H
	 *  and fill native CPUID.80000001H:ECX into boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
	 *  fill native CPUID.80000001H:EDX into boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX].
	 *  - CPUID_EXTEND_FUNCTION_1
	 *  - &unused
	 *  - &unused
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX]
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX] */
	cpuid(CPUID_EXTEND_FUNCTION_1, &unused, &unused, &boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX]);


	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000007H
	 *  and fill native CPUID.80000007H:EDX into boot_cpu_data.cpuid_leaves[FEAT_8000_0007_EDX].
	 *  - CPUID_EXTEND_INVA_TSC
	 *  - &eax
	 *  - &unused
	 *  - &unused
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_8000_0007_EDX] */
	cpuid(CPUID_EXTEND_INVA_TSC, &eax, &unused, &unused, &boot_cpu_data.cpuid_leaves[FEAT_8000_0007_EDX]);

	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000008H
	 *  and fill native CPUID.80000008H:EBX into boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX].
	 *  - CPUID_EXTEND_ADDRESS_SIZE
	 *  - &eax
	 *  - &boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX]
	 *  - &unused
	 *  - &unused */
	cpuid(CPUID_EXTEND_ADDRESS_SIZE, &eax, &boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX], &unused,
		&unused);

	/* EAX bits 07-00: #Physical Address Bits
	 *     bits 15-08: #Linear Address Bits
	 */
	/** Set the virt_bits field of boot_cpu_data to ((eax >> 8) & ffH),
	 *  Meaning store native processor supported linear address bits into boot_cpu_data */
	boot_cpu_data.virt_bits = (uint8_t)((eax >> 8U) & 0xffU);
	/** Set the phys_bits field of boot_cpu_data to (eax & 0xffU),
	 *  Meaning store native processor supported physical address bits into boot_cpu_data */
	boot_cpu_data.phys_bits = (uint8_t)(eax & 0xffU);
}

/**
 * @brief The interface initializes an internal string that stores the CPU model name.
 * The string could be accessed by other modules via invoking "get_pcpu_info()".
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_pcpu_model_name(void)
{
	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000002H
	 *  and fill native CPUID.80000002H:EAX, CPUID.80000002H:EBX, CPUID.80000002H:ECX, CPUID.80000002H:EDX
	 *  into boot_cpu_data.model_name 0 ~ 15 bits.
	 *  - CPUID_EXTEND_FUNCTION_2
	 *  - (uint32_t *)(boot_cpu_data.model_name)
	 *  - (uint32_t *)(&boot_cpu_data.model_name[4])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[8])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[12]) */
	cpuid(CPUID_EXTEND_FUNCTION_2, (uint32_t *)(boot_cpu_data.model_name),
		(uint32_t *)(&boot_cpu_data.model_name[4]), (uint32_t *)(&boot_cpu_data.model_name[8]),
		(uint32_t *)(&boot_cpu_data.model_name[12]));
	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000003H
	 *  and fill native CPUID.80000003H:EAX, CPUID.80000003H:EBX, CPUID.80000003H:ECX, CPUID.80000003H:EDX
	 *  into boot_cpu_data.model_name 16 ~ 31 bits.
	 *  - CPUID_EXTEND_FUNCTION_3
	 *  - (uint32_t *)(&boot_cpu_data.model_name[16])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[20])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[24])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[28]) */
	cpuid(CPUID_EXTEND_FUNCTION_3, (uint32_t *)(&boot_cpu_data.model_name[16]),
		(uint32_t *)(&boot_cpu_data.model_name[20]), (uint32_t *)(&boot_cpu_data.model_name[24]),
		(uint32_t *)(&boot_cpu_data.model_name[28]));
	/** Call cpuid with the following parameters, in order to
	 *  get the native processor information for CPUID.80000004H
	 *  and fill native CPUID.80000004H:EAX, CPUID.80000004H:EBX, CPUID.80000004H:ECX, CPUID.80000004H:EDX
	 *  into boot_cpu_data.model_name 32 ~ 47 bits.
	 *  - CPUID_EXTEND_FUNCTION_4
	 *  - (uint32_t *)(&boot_cpu_data.model_name[32])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[36])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[40])
	 *  - (uint32_t *)(&boot_cpu_data.model_name[44]) */
	cpuid(CPUID_EXTEND_FUNCTION_4, (uint32_t *)(&boot_cpu_data.model_name[32]),
		(uint32_t *)(&boot_cpu_data.model_name[36]), (uint32_t *)(&boot_cpu_data.model_name[40]),
		(uint32_t *)(&boot_cpu_data.model_name[44]));

	/** Set boot_cpu_data.model_name[48] to '\0',
	 *  so far, native processor model name information has filled into boot_cpu_data */
	boot_cpu_data.model_name[48] = '\0';
}

/**
 * @brief The interface returns a pointer which points to a data structure
 *        containing the CPU information data
 *
 * @return A pointer which points to a data structure containing the CPU information data.
 *
 * @pre N/A
 *
 * @post return != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API shall be called after init_pcpu_capabilities has been called once
 * on the physical bootstrap processor.
 *
 * @remark The physical platform shall guarantee that the following CPUID leaves are supported:
 * 0H, 1H, 7H, 80000000H, 80000001H, 80000007H, and 80000008H.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
struct cpuinfo_x86 *get_pcpu_info(void)
{
	/** Return '&boot_cpu_data' */
	return &boot_cpu_data;
}

/**
 * @}
 */
