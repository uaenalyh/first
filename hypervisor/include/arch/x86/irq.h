/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

/**
 * @addtogroup hwmgmt_irq
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the hwmgmt.irq module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * hwmgmt.irq module.
 *
 */

#include <acrn_common.h>
#include <util.h>
#include <spinlock.h>

/**
 * @brief The log level of interrupt related debug information.
 */
#define ACRN_DBG_IRQ   6U

#define NR_MAX_VECTOR  0xFFU                /**< the maximum vector number */
#define VECTOR_INVALID (NR_MAX_VECTOR + 1U) /**< A pre-defined value to indicate the invalid vector */

#define HV_ARCH_VCPU_RFLAGS_RF (1UL << 16U) /**< Bitmask of resume flag in RFLAG register. */

/**
 * @brief Data structure to store the context of the interrupt/exception.
 *
 * Data structure to store the general purpose registers, vector number, error code,
 * RIP, CS, RFLAGS, RSP and SS of the interrupt/exception.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct intr_excp_ctx {
	struct acrn_gp_regs gp_regs; /**< the general purpose registers */
	uint64_t vector;             /**< the vector number */
	uint64_t error_code;         /**< the error code */
	uint64_t rip;                /**< the host RIP */
	uint64_t cs;                 /**< the host CS */
	uint64_t rflags;             /**< the host RFLAGS */
	uint64_t rsp;                /**< the host RSP */
	uint64_t ss;                 /**< the host SS */
};

void init_default_irqs(uint16_t cpu_id);

void dispatch_exception(struct intr_excp_ctx *ctx);

void dispatch_interrupt(__unused const struct intr_excp_ctx *ctx);

void handle_nmi(struct intr_excp_ctx *ctx);

void init_interrupt(uint16_t pcpu_id);

/**
 * @}
 */

#endif /* ARCH_IRQ_H */
