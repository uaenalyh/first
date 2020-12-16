/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFEATURES_H
#define CPUFEATURES_H

/**
 * @addtogroup hwmgmt_cpu_caps
 *
 * @{
 */

/**
 * @file
 * @brief Define the macros related to CPU features.
 */

/**
 * @brief A flag used to check whether the processor supports the MONITOR/MWAIT feature.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_MONITOR ((FEAT_1_ECX << 5U) + 3U)
/**
 * @brief A flag used to check whether the OS has set CR4.OSXSAVE.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_OSXSAVE ((FEAT_1_ECX << 5U) + 27U)

/* Intel-defined CPU features, CPUID level 0x00000007 (EDX) */
/**
 * @brief CPUID: Enumerates support for clearing CPU internal buffer.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_MDS_CLEAR ((FEAT_7_0_EDX << 5U) + 10U)
/**
 * @brief Feature that enumerates support for IBRS and IBPB.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_IBRS_IBPB ((FEAT_7_0_EDX << 5U) + 26U)
/**
 * @brief Feature that enumerates support for L1D_FLUSH.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_L1D_FLUSH ((FEAT_7_0_EDX << 5U) + 28U)
/**
 * @brief Feature that enumerates support for the IA32_ARCH_CAPABILITIES MSR.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_ARCH_CAP  ((FEAT_7_0_EDX << 5U) + 29U)
/**
 * @brief Feature that enumerates support for SSBD.
 *
 * This flag is associated with the array "cpuid_leaves" defined in the data structure "struct cpuinfo_x86".
 * The higher 27 bits represent the index of the element (associated the specified feature) in the array.
 * The lower 5 bits represent the bit position (associated with the specified feature) to be checked.
 */
#define X86_FEATURE_SSBD      ((FEAT_7_0_EDX << 5U) + 31U)

/**
 * @}
 */

#endif /* CPUFEATURES_H */
