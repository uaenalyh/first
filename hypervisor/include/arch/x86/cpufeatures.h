/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFEATURES_H
#define CPUFEATURES_H

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define X86_FEATURE_MONITOR ((FEAT_1_ECX << 5U) + 3U)
#define X86_FEATURE_OSXSAVE ((FEAT_1_ECX << 5U) + 27U)

/* Intel-defined CPU features, CPUID level 0x00000007 (EDX)*/
#define X86_FEATURE_MDS_CLEAR ((FEAT_7_0_EDX << 5U) + 10U)
#define X86_FEATURE_IBRS_IBPB ((FEAT_7_0_EDX << 5U) + 26U)
#define X86_FEATURE_STIBP     ((FEAT_7_0_EDX << 5U) + 27U)
#define X86_FEATURE_L1D_FLUSH ((FEAT_7_0_EDX << 5U) + 28U)
#define X86_FEATURE_ARCH_CAP  ((FEAT_7_0_EDX << 5U) + 29U)
#define X86_FEATURE_SSBD      ((FEAT_7_0_EDX << 5U) + 31U)

/**
 * @}
 */

#endif /* CPUFEATURES_H */
