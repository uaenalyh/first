/*
 * ACRN TRACE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#ifndef TRACE_H
#define TRACE_H

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief Declare trace APIs and define trace event ID that shall be provided by debug module.
 *
 * The trace APIs are used to trace event for debugging, all of the trace APIs are do no operation in release version.
 */
#include <per_cpu.h>

/**
 * @brief A trace event ID indicating VM exit.
 */
#define TRACE_VM_EXIT      0x10U
/**
 * @brief A trace event ID indicating VM entry.
 */
#define TRACE_VM_ENTER     0X11U
/**
 * @brief A base value which is used to calculate the trace event IDs for different VM exit reasons.
 */
#define TRACE_VMEXIT_ENTRY 0x10000U

/**
 * @brief A trace event ID indicating a VM exit triggered by the CPUID instruction executed from guest software.
*/
#define TRACE_VMEXIT_CPUID                (TRACE_VMEXIT_ENTRY + 0x00000004U)
/**
 * @brief A trace event ID indicating a VM exit triggered by the CR registers accessed from guest software.
*/
#define TRACE_VMEXIT_CR_ACCESS            (TRACE_VMEXIT_ENTRY + 0x0000001CU)
/**
 * @brief A trace event ID indicating a VM exit triggered by the IO instruction executed from guest software.
*/
#define TRACE_VMEXIT_IO_INSTRUCTION       (TRACE_VMEXIT_ENTRY + 0x0000001EU)
/**
 * @brief A trace event ID indicating a VM exit triggered by the RDMSR instruction executed from guest software.
*/
#define TRACE_VMEXIT_RDMSR                (TRACE_VMEXIT_ENTRY + 0x0000001FU)
/**
 * @brief A trace event ID indicating a VM exit triggered by the WRMSR instruction executed from guest software.
*/
#define TRACE_VMEXIT_WRMSR                (TRACE_VMEXIT_ENTRY + 0x00000020U)
/**
 * @brief A trace event ID indicating a VM exit triggered by the EPT violations occurred during VM lifecycle.
*/
#define TRACE_VMEXIT_EPT_VIOLATION        (TRACE_VMEXIT_ENTRY + 0x00000030U)

/**
 * @brief A trace event ID indicating VM exit triggered by unexpected event during VM lifecycle.
*/
#define TRACE_VMEXIT_UNEXPECTED 0x20000U

void TRACE_2L(uint32_t evid, uint64_t e, uint64_t f);
void TRACE_4I(uint32_t evid, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

/**
 * @}
 */

#endif /* TRACE_H */
