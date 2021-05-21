/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PER_CPU_H
#define PER_CPU_H

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief Define structures and implement per-CPU APIs to hold per CPU information.
 *
 * 1. Define the per_cpu_region to hold all per CPU information.
 * 2. Implement functions to get data from per CPU region.
 *
 * This file is decomposed into the following functions:
 *
 * - per_cpu(name, pcpu_id): Get the per_cpu_data's specified field \a name of the CPU whose CPU id is \a pcpu_id.
 * - get_cpu_var(name): Get the per_cpu_data's specified field \a name of the current CPU.
 */

#include <types.h>
#include <irq.h>
#include <page.h>
#include <timer.h>
#include <profiling.h>
#include <logmsg.h>
#include <gdt.h>
#include <schedule.h>
#include <security.h>
#include <vm_config.h>

/**
 * @brief Size of the 2-MByte page.
 */
#define GUARD_PAGE_SIZE  0x200000U
/**
 * @brief The physical CPU normal stack size in Byte
 */
#define PCPU_STACK_SIZE  0x200000U
/**
 * @brief The structure to hold all per CPU information.
 *
 * @consistency For struct per_cpu_region p, p.lapic_id == 2 * pcpuid_from_vcpu(p.ever_run_vcpu)
 * @alignment PAGE_SIZE
 */
struct per_cpu_region {
	/**
	 * @brief The guard page.
	 *
	 * This guard page is to mitigate stack overflow. This page is 2M-byte
	 * alignment and will be unmapped during page initialization.
	 */
	uint8_t before_guard_page[GUARD_PAGE_SIZE] __aligned(GUARD_PAGE_SIZE);
	/**
	 * @brief The physical CPU stack.
	 *
	 * This stack is the physical CPU stack on the logical processor. This stack is 2M-byte
	 * alignment.
	 */
	uint8_t stack[PCPU_STACK_SIZE] __aligned(PCPU_STACK_SIZE);
	/**
	 * @brief The guard page.
	 *
	 * This guard page is to mitigate stack underflow. This page is 2M-byte
	 * alignment and will be unmapped during page initialization.
	 */
	uint8_t after_guard_page[GUARD_PAGE_SIZE] __aligned(GUARD_PAGE_SIZE);
	/* vmxon_region MUST be 4KB-aligned */
	uint8_t vmxon_region[PAGE_SIZE]; /**< Array that the logical processor uses to support VMX operation. */
	void *vmcs_run; /**< VMCS region used for vCPU run on the logical processor. */
	struct acrn_vcpu *ever_run_vcpu; /**< Pointer to acrn_vcpu data structure that runs on the current logical
					  *   processor. */
#ifdef STACK_PROTECTOR
	struct stack_canary stk_canary __aligned(8); /**< A struct including 64-bit integer used to detect a stack
						      *   buffer overflow before execution of malicious code can
						      *   occur. Aligned with 8-bytes. */
#endif
	struct sched_control sched_ctl; /**< The scheduler control block of the physical processor. */
	struct sched_noop_control sched_noop_ctl; /**< This field is to record the thread object of the thread which
						   *   will run or is running on the physical processor while the noop
						   *   scheduler is configured. */
	struct thread_object idle; /**< The struct that represents idle thread. */
	struct host_gdt gdt; /**< gdt represents the Global Descriptor Table used for the logical processor. */
	struct tss_64 tss; /**< This struct represents Task State Segment used for the logical processor. */
	enum pcpu_boot_state boot_state; /**< per CPU's boot state halt or running. */
	uint64_t pcpu_flag; /**< The field is to record to physical processor offline request or shutdown VM
			     *   request. */
	uint8_t mc_stack[CONFIG_STACK_SIZE] __aligned(16); /**< stack used to handle machine check on the logical
							    *   processor. This stack is 16-byte aligned. */
	uint8_t df_stack[CONFIG_STACK_SIZE] __aligned(16); /**< stack used to handle stack double fault on the logical
							    *   processor. This stack is 16-byte aligned. */
	uint8_t sf_stack[CONFIG_STACK_SIZE] __aligned(16); /**< stack used to handle stack segment fault on the logical
							    *   processor.This stack is 16-byte aligned. */
	uint32_t lapic_id; /**< lapic id. */
	uint32_t lapic_ldr; /**< lapic local destination register. */
	uint16_t shutdown_vm_id; /**< ID representing the VM that requests to be shutdown. */
} __aligned(GUARD_PAGE_SIZE); /* per_cpu_region size aligned with GUARD_PAGE_SIZE */

extern struct per_cpu_region per_cpu_data[MAX_PCPU_NUM];
/**
 * @brief Get the per_cpu_data's specified field \a name of the CPU whose CPU id is \a pcpu_id.
 *
 * Implementation: Return 'per_cpu_data[(pcpu_id)].name'
 *
 * @param[in]    name The name of the specified field (whose data is to be fetched) in 'struct per_cpu_region'.
 * @param[in]    pcpu_id The CPU id of the physical CPU that will be used.
 *
 * @pre N/A
 * @post N/A
 *
 * @return The \a name field of the per_cpu_data of the CPU whose CPU id is \a pcpu_id.
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
#define per_cpu(name, pcpu_id) (per_cpu_data[(pcpu_id)].name)

/**
 * @brief Get the per_cpu_data's specified field \a name of the current CPU.
 *
 * Implementation: Return 'per_cpu(name, get_pcpu_id())'
 *
 * @param[in]    name The name of the specified field (whose data is to be fetched) in 'struct per_cpu_region'.
 *
 * @pre N/A
 * @post N/A
 *
 * @return The \a name field of the per_cpu_data of the current CPU.
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
#define get_cpu_var(name) per_cpu(name, get_pcpu_id())

/**
 * @}
 */

#endif
