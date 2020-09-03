/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCPUID_H_
#define VCPUID_H_

/**
 * @addtogroup vp-base_vcpuid
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the vp-base.vcpuid module.
 *
 * This file declares all external functions, data structures, and macros that shall be provided by the
 * vp-base.vcpuid module.
 *
 */

#include <types.h>

#define MAX_VM_VCPUID_ENTRIES 64U /**< Pre-defined maximum number of virtual CPUID entries. */

/**
 * @brief Data structure to cache the emulated contents for a CPUID instruction.
 *
 * Data structure to cache the emulated contents for a CPUID instruction, calling each one as a virtual CPUID entry.
 * It is supposed to be used when hypervisor accesses and manipulates the virtual CPUID entries for guest software.
 *
 * @consistency N/A
 * @alignment 4
 *
 * @remark N/A
 */
struct vcpuid_entry {
	/**
	 * @brief The emulated contents put to guest EAX register after execution of a CPUID instruction by a vCPU.
	 */
	uint32_t eax;
	/**
	 * @brief The emulated contents put to guest EBX register after execution of a CPUID instruction by a vCPU.
	 */
	uint32_t ebx;
	/**
	 * @brief The emulated contents put to guest ECX register after execution of a CPUID instruction by a vCPU.
	 */
	uint32_t ecx;
	/**
	 * @brief The emulated contents put to guest EDX register after execution of a CPUID instruction by a vCPU.
	 */
	uint32_t edx;
	uint32_t leaf; /**< The contents of EAX register upon execution of a CPUID instruction. */
	uint32_t subleaf; /**< The contents of ECX register upon execution of a CPUID instruction. */

	/**
	 * @brief A flag for the virtual CPUID entry.
	 *
	 * A flag to indicate whether hypervisor needs to check the contents of ECX register upon execution of a CPUID
	 * instruction.
	 */
	uint32_t flags;
};

void set_vcpuid_entries(struct acrn_vm *vm);
void guest_cpuid(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

/**
 * @}
 */

#endif /* VCPUID_H_ */
