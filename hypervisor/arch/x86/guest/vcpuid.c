/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vcpu.h>
#include <vm.h>
#include <cpuid.h>
#include <cpufeatures.h>
#include <logmsg.h>

/**
 * @defgroup vp-base_vcpuid vp-base.vcpuid
 * @ingroup vp-base
 * @brief Implementation of all external APIs to virtualize CPUID instructions.
 *
 * This module implements the virtualization of CPUID instruction executed from guest software.
 * CPUID execution from guest would cause VM exit unconditionally if executed in VMX non-root operation.
 * Hypervisor would return the emulated processor identification and feature information in the EAX, EBX, ECX, and EDX
 * registers.
 *
 * Usage:
 * - 'vp-base.vm' module depends on this module to fill virtual CPUID entries for each VM.
 * - 'vp-base.hv_main' module depends on this module to access guest CPUID.
 * - 'vp-base.vmsr' module depends on this module to access guest CPUID.80000001H.
 *
 * Dependency:
 * - This module depends on 'vp-base.vlapic' module to get the APIC ID associated with the specified vCPU.
 * - This module depends on 'vp-base.vcpu' module to access guest states, such as MSR, and vLAPIC of the specified vCPU.
 * - This module depends on 'vp-base.vm' module to check if the specified VM is a safety VM or not.
 * - This module depends on 'vp-base.vcr' module to get guest CR4.
 * - This module depends on 'hwmgmt.cpu_caps' module to get the native processor information regarding to CPUID
 * instruction.
 * - This module depends on 'lib.bits' module to find the most significant bit set in specified value.
 * - This module depends on 'lib.utils' module to do memory copy.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the vp-base.vcpuid module.
 *
 * This file implements all external functions, data structures, and macros that shall be provided by the
 * vp-base.vcpuid module.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: is_percpu_related, local_find_vcpuid_entry, find_vcpuid_entry, set_vcpuid_entry,
 * and init_vcpuid_entry.
 *
 * Decomposed functions include: set_vcpuid_extended_function, guest_cpuid_01h, guest_cpuid_0bh, guest_cpuid_0dh,
 * guest_cpuid_80000001h, and guest_limit_cpuid.
 *
 */

/**
 * @brief A virtual CPUID entry flag which indicates whether sub-leaf value needs to be checked.
 */
#define CPUID_CHECK_SUBLEAF		(1U << 0U)

#define VIRT_CRYSTAL_CLOCK_FREQ		0x16C2154U /**< Virtual crystal clock frequency. The value is defined in SRS. */
#define L2_WAYS_OF_ASSOCIATIVITY	4U         /**< L2 ways of associativity. The value is defined in SRS. */

/**
 * @brief Find whether there exists a virtual CPUID entry that matches with input arguments.
 *
 * Find whether there exists a virtual CPUID entry in vcpu->vm->vcpuid_entries that matches with input arguments.
 * Matching means that its member 'leaf' is equal to \a leaf and its member 'subleaf' is equal to \a subleaf.
 * It is supposed to be called only by 'find_vcpuid_entry'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[in] leaf The contents of EAX register upon execution of a CPUID instruction.
 * @param[in] subleaf The contents of ECX register upon execution of a CPUID instruction.
 *
 * @return A pointer which points to a virtual CPUID entry.
 *
 * @retval non-NULL There exists a virtual CPUID entry in vcpu->vm->vcpuid_entries that matches with input arguments.
 * @retval NULL There is no virtual CPUID entry in vcpu->vm->vcpuid_entries that matches with input arguments.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after set_vcpuid_entries has been called on vcpu->vm once on any processor.
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static inline const struct vcpuid_entry *local_find_vcpuid_entry(
	const struct acrn_vcpu *vcpu, uint32_t leaf, uint32_t subleaf)
{
	/** Declare the following local variables of type 'uint32_t'.
	 *  - i representing the loop counter as array index, initialized as 0.
	 *  - nr representing the number of virtual CPUID entries, not initialized.
	 *  - half representing the half number of virtual CPUID entries, not initialized. */
	uint32_t i = 0U, nr, half;
	/** Declare the following local variables of type 'const struct vcpuid_entry *'.
	 *  - found_entry representing a pointer to a virtual CPUID entry matching the given \a leaf and \a subleaf,
	 *  initialized as NULL. */
	const struct vcpuid_entry *found_entry = NULL;
	/** Declare the following local variables of type 'struct acrn_vm *'.
	 *  - vm representing a pointer which points to the vm associated with \a vcpu, initialized as 'vcpu->vm'. */
	struct acrn_vm *vm = vcpu->vm;

	/** Set 'nr' to 'vm->vcpuid_entry_nr', which is the number of virtual CPUID entries of 'vm' */
	nr = vm->vcpuid_entry_nr;
	/** Set 'half' to half of 'nr' (round down) */
	half = nr >> 1U;
	/** If the specified \a leaf is larger than 'vm->vcpuid_entries[half].leaf' */
	if (vm->vcpuid_entries[half].leaf < leaf) {
		/** Set 'i' to 'half' */
		i = half;
	}

	/** For each 'i' ranging from the start counter to 'nr - 1' [with a step of 1]. The start counter is
	 * 'half' if the specified \a leaf is larger than 'vm->vcpuid_entries[half].leaf', otherwise, it is 0U. */
	for (; i < nr; i++) {
		/** Declare the following local variables of type 'const struct vcpuid_entry *'.
		 *  - tmp representing a pointer to the ith virtual CPUID entry of 'vm', initialized as
		 *  &vm->vcpuid_entries[i]. */
		const struct vcpuid_entry *tmp = (const struct vcpuid_entry *)(&vm->vcpuid_entries[i]);

		/** If the specified \a leaf is larger than 'tmp->leaf' */
		if (tmp->leaf < leaf) {
			/** Continue to next iteration */
			continue;
		/** If the specified \a leaf is equal to 'tmp->leaf' */
		} else if (tmp->leaf == leaf) {
			/** If 'tmp->flags' indicates that subleaf value needs to be checked and \a subleaf is not equal
			 *  to 'tmp->subleaf' */
			if (((tmp->flags & CPUID_CHECK_SUBLEAF) != 0U) && (tmp->subleaf != subleaf)) {
				/** Continue to next iteration */
				continue;
			}
			/** Set 'found_entry' to 'tmp', meaning that the virtual CPUID entry matched with input
			 *  arguments has been found */
			found_entry = tmp;
			/** Terminate the loop */
			break;
		} else {
			/* tmp->leaf > leaf */
			/** Terminate the loop as a matching virtual CPUID entry of 'vm' does not exist */
			break;
		}
	}

	/** Return 'found_entry' */
	return found_entry;
}

/**
 * @brief Find whether there exists a virtual CPUID entry that contains the requested virtual processor information.
 *
 * Find whether there exists a virtual CPUID entry in vcpu->vm->vcpuid_entries that contains
 * the requested virtual processor information. If there exists one, it is either the one that matches with
 * input arguments, or the one that contains the highest basic leaf information (when \a leaf_arg is an invalid input
 * EAX value).
 * It is supposed to be used when hypervisor attempts to get the virtual processor information for specific CPUID leaf
 * of a vCPU.
 *
 * @param[in] vcpu The vCPU whose virtual processor information is requested.
 * @param[in] leaf_arg The contents of EAX register upon execution of a CPUID instruction.
 * @param[in] subleaf The contents of ECX register upon execution of a CPUID instruction.
 *
 * @return A pointer which points to a virtual CPUID entry.
 *
 * @retval non-NULL There exists a virtual CPUID entry in vcpu->vm->vcpuid_entries that
 *                  contains the requested virtual processor information.
 * @retval NULL There is no virtual CPUID entry in vcpu->vm->vcpuid_entries that
 *              contains the requested virtual processor information.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after set_vcpuid_entries has been called on vcpu->vm once on any processor.
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static inline const struct vcpuid_entry *find_vcpuid_entry(
	const struct acrn_vcpu *vcpu, uint32_t leaf_arg, uint32_t subleaf)
{
	/** Declare the following local variables of type 'const struct vcpuid_entry *'.
	 *  - entry representing a pointer which points to the virtual CPUID entry containing the requested virtual
	 *  processor information, not initialized. */
	const struct vcpuid_entry *entry;
	/** Declare the following local variables of type uint32_t.
	 *  - leaf representing the contents of EAX register upon execution of a CPUID instruction, initialized as
	 *  \a leaf_arg. */
	uint32_t leaf = leaf_arg;

	/** Set 'entry' to the return value of 'local_find_vcpuid_entry(vcpu, leaf, subleaf)'. If 'entry' is not a NULL
	 *  pointer, it contains the virtual processor information for CPUID.(EAX='leaf',ECX='subleaf'). */
	entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
	/** If 'entry' is a NULL pointer */
	if (entry == NULL) {
		/** Declare the following local variables of type uint32_t.
		 *  - limit representing the maximum leaf of basic or extended function for the processor, not
		 *  initialized. */
		uint32_t limit;
		/** Declare the following local variables of type 'struct acrn_vm *'.
		 *  - vm representing a pointer which points to the vm associated with \a vcpu, initialized as
		 *  vcpu->vm. */
		struct acrn_vm *vm = vcpu->vm;

		/** If 'leaf' represents an extended function for the processor */
		if ((leaf & CPUID_MAX_EXTENDED_FUNCTION) != 0U) {
			/** Set 'limit' to 'vm->vcpuid_xlevel', which is the maximum leaf of extended function
			 *  for the virtual processor */
			limit = vm->vcpuid_xlevel;
		} else {
			/** Set 'limit' to 'vm->vcpuid_level', which is the maximum leaf of basic function for
			 *  the virtual processor */
			limit = vm->vcpuid_level;
		}

		/** If 'leaf' is larger than 'limit', meaning that it is an invalid input EAX value */
		if (leaf > limit) {
			/** Set 'leaf' to 'vm->vcpuid_level', which is the maximum leaf of basic function for
			 *  the virtual processor. Intel documentation states that invalid EAX input will return the
			 *  same information as EAX=vm->vcpuid_level (Intel SDM Vol. 2A - Instruction Set Reference -
			 *  CPUID). */
			leaf = vm->vcpuid_level;
			/** Set 'entry' to the return value of 'local_find_vcpuid_entry(vcpu, leaf, subleaf)', which
			 *  contains the highest basic leaf information */
			entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
		}
	}

	/** Return 'entry' */
	return entry;
}

/**
 * @brief Fill a virtual CPUID entry in vm->vcpuid_entries.
 *
 * Fill a virtual CPUID entry in vm->vcpuid_entries with the information provided by
 * \a entry, and increment the number of virtual CPUID entries 'vm->vcpuid_entry_nr' by 1.
 * It is supposed to be used when hypervisor initializes virtual CPUID entries for the specified VM.
 *
 * @param[inout] vm A pointer to a virtual machine data structure whose vcpuid_entries is to be filled.
 * @param[in] entry A pointer to a virtual CPUID entry providing the information to be filled.
 *
 * @return void
 *
 * @pre vm != NULL
 * @pre vm->vcpuid_entry_nr < MAX_VM_VCPUID_ENTRIES
 * @pre entry != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vm is different among parallel invocation.
 */
static inline void set_vcpuid_entry(struct acrn_vm *vm, const struct vcpuid_entry *entry)
{
	/** Declare the following local variables of type 'struct vcpuid_entry *'.
	 *  - tmp representing a pointer which points to the virtual CPUID entry to be filled, not initialized. */
	struct vcpuid_entry *tmp;
	/** Declare the following local variables of type size_t.
	 *  - entry_size representing the size of data structure 'struct vcpuid_entry', initialized as
	 * 'sizeof(struct vcpuid_entry)'. */
	size_t entry_size = sizeof(struct vcpuid_entry);

	/** Set 'tmp' to the address of a virtual CPUID entry in vm->vcpuid_entries,
	 *  the corresponding index is 'vm->vcpuid_entry_nr' */
	tmp = &vm->vcpuid_entries[vm->vcpuid_entry_nr];
	/** Increment the number of virtual CPUID entries 'vm->vcpuid_entry_nr' by 1 */
	vm->vcpuid_entry_nr++;
	/** Call memcpy_s with the following parameters, in order to copy the data stored in \a entry to the address of
	 *  a virtual CPUID entry specified by 'tmp', and discard its return value.
	 *  - tmp
	 *  - entry_size
	 *  - entry
	 *  - entry_size
	 */
	(void)memcpy_s(tmp, entry_size, entry, entry_size);
}

/**
 * initialization of virtual CPUID leaf
 */
/**
 * @brief Initialize a virtual CPUID entry.
 *
 * Initialize a virtual CPUID entry for CPUID leaf specified by input arguments.
 * It is supposed to be used when hypervisor initializes virtual CPUID entries for VMs.
 *
 * @param[in] leaf The contents of EAX register upon execution of a CPUID instruction.
 * @param[in] subleaf The contents of ECX register upon execution of a CPUID instruction.
 * @param[in] flags Flags to indicate whether \a subleaf value needs to be checked.
 * @param[out] entry The pointer which points to the virtual CPUID entry to be initialized.
 *                   It will hold guest CPUID.(EAX=leaf,ECX=subleaf) after being initialized.
 *
 * @return void
 *
 * @pre entry != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a entry is different among parallel invocation.
 */
static void init_vcpuid_entry(uint32_t leaf, uint32_t subleaf, uint32_t flags, struct vcpuid_entry *entry)
{
#ifdef QEMU
	struct cpuinfo_x86 *cpu_info;
#endif
	/** Set entry->leaf to \a leaf */
	entry->leaf = leaf;
	/** Set entry->subleaf to \a subleaf */
	entry->subleaf = subleaf;
	/** Set entry->flags to \a flags */
	entry->flags = flags;

	/** Depending on CPUID leaf \a leaf */
	switch (leaf) {
	/** CPUID leaf \a leaf is 6H */
	case 0x06U:
		/** Set guest CPUID.6H:EAX to CPUID_EAX_ARAT */
		entry->eax = CPUID_EAX_ARAT;
		/** Set guest CPUID.6H:EBX to 0H */
		entry->ebx = 0U;
		/** Set guest CPUID.6H:ECX to 0H */
		entry->ecx = 0U;
		/** Set guest CPUID.6H:EDX to 0H */
		entry->edx = 0U;
		/** End of case */
		break;

	/** CPUID leaf \a leaf is 7H */
	case 0x07U:
		/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
		 *  for CPUID.(EAX=leaf,ECX=subleaf) and fill the native processor information into the output parameter
		 *  \a entry.
		 *  - leaf
		 *  - subleaf
		 *  - &entry->eax
		 *  - &entry->ebx
		 *  - &entry->ecx
		 *  - &entry->edx
		 */
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		/** Clear the following bits in guest CPUID.(EAX=7H,ECX=0H):EBX:
		 *  INVPCID Bit (Bit 10),
		 *  Resource Director Technology Monitoring Capability Bit (Bit 12),
		 *  and Resource Director Technology Monitoring Allocation Bit (Bit 15)
		 */
		entry->ebx &= ~(CPUID_EBX_INVPCID | CPUID_EBX_PQM | CPUID_EBX_PQE);

		/** Clear SGX Bit (Bit 2) in guest CPUID.(EAX=7H,ECX=0H):EBX */
		entry->ebx &= ~CPUID_EBX_SGX;
		/** Clear SGX_LC Bit (Bit 30) in guest CPUID.(EAX=7H,ECX=0H):ECX */
		entry->ecx &= ~CPUID_ECX_SGX_LC;

		/** Clear MPX Bit (Bit 14) in guest CPUID.(EAX=7H,ECX=0H):EBX */
		entry->ebx &= ~CPUID_EBX_MPX;

		/** Clear Intel Processor Trace Bit (Bit 25) in guest CPUID.(EAX=7H,ECX=0H):EBX */
		entry->ebx &= ~CPUID_EBX_PROC_TRC;

		/** Clear TSX HLE Bit in guest CPUID.(EAX=7H,ECX=0H):EBX */
		entry->ebx &= ~CPUID_EBX_HLE;

		/** Clear STIBP Bit (Bit 27) in guest CPUID.(EAX=7H,ECX=0H):EDX */
		entry->edx &= ~CPUID_EDX_STIBP;

		/** Clear TSX Force Abort Bit (Bit 13) in guest CPUID.(EAX=7H,ECX=0H):EDX */
		entry->edx &= ~CPUID_EDX_TSX_FORCE_ABORT;
		/** End of case */
		break;

	/** CPUID leaf \a leaf is 15H */
	case 0x15U:
		/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
		 *  for CPUID.(EAX=leaf,ECX=subleaf) and fill the native processor information into the output parameter
		 *  \a entry.
		 *  - leaf
		 *  - subleaf
		 *  - &entry->eax
		 *  - &entry->ebx
		 *  - &entry->ecx
		 *  - &entry->edx
		 */
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		/** Set guest CPUID.15H:ECX to VIRT_CRYSTAL_CLOCK_FREQ */
		entry->ecx = VIRT_CRYSTAL_CLOCK_FREQ;
		/** End of case */
		break;

#ifdef QEMU
	case 0x16U:
		cpu_info = get_pcpu_info();
		if (cpu_info->cpuid_level >= 0x16U) {
			/* call the cpuid when 0x16 is supported */
			cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		} else {
			/* Use the tsc to derive the emulated 0x16U cpuid. */
			entry->eax = (uint32_t)(get_tsc_khz() / 1000U);
			entry->ebx = entry->eax;
			/* Bus frequency: hard coded to 100M */
			entry->ecx = 100U;
			entry->edx = 0U;
		}
		break;
#endif

	/** CPUID leaf \a leaf is 80000006H */
	case 0x80000006U:
		/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
		 *  for CPUID.(EAX=leaf,ECX=subleaf) and fill the native processor information into the output parameter
		 *  \a entry.
		 *  - leaf
		 *  - subleaf
		 *  - &entry->eax
		 *  - &entry->ebx
		 *  - &entry->ecx
		 *  - &entry->edx
		 */
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		/** Set L2 Associativity field Bits (Bits 15 - 12) in guest CPUID.80000006H:ECX to
		 *  L2_WAYS_OF_ASSOCIATIVITY */
		entry->ecx = (entry->ecx & ~CPUID_ECX_L2_ASSOCIATIVITY_FIELD_MASK) |
				(L2_WAYS_OF_ASSOCIATIVITY << CPUID_ECX_L2_ASSOCIATIVITY_FIELD_POS);
		/** End of case */
		break;

	/** Otherwise */
	default:
		/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
		 *  for CPUID.(EAX=leaf,ECX=subleaf) and fill the native processor information into the output parameter
		 *  \a entry.
		 *  - leaf
		 *  - subleaf
		 *  - &entry->eax
		 *  - &entry->ebx
		 *  - &entry->ecx
		 *  - &entry->edx
		 */
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		/** End of case */
		break;
	}
}

/**
 * @brief Fill virtual CPUID entries for extended functions in vm->vcpuid_entries.
 *
 * Fill virtual CPUID entries for extended functions in vm->vcpuid_entries.
 * It includes all supported extended function CPUID leaves except 80000001H.
 * It is supposed to be used when hypervisor initializes virtual CPUID entries for the specified VM.
 *
 * @param[inout] vm A pointer to a virtual machine data structure whose vcpuid_entries is to be filled.
 *
 * @return void
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vm is different among parallel invocation.
 */
static void set_vcpuid_extended_function(struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as CPUID leaf, not initialized.
	 *  - limit representing the maximum leaf of extended function for the processor, not initialized. */
	uint32_t i, limit;
	/** Declare the following local variables of type 'struct vcpuid_entry'.
	 *  - entry representing a temporary virtual CPUID entry containing the virtual processor information for one
	 *  extended function, not initialized. */
	struct vcpuid_entry entry;

	/** Call init_vcpuid_entry with the following parameters, in order to initialize the virtual CPUID entry for
	 *  CPUID.80000000H.
	 *  - CPUID_MAX_EXTENDED_FUNCTION
	 *  - 0H
	 *  - 0H
	 *  - &entry
	 */
	init_vcpuid_entry(CPUID_MAX_EXTENDED_FUNCTION, 0U, 0U, &entry);
	/** Call set_vcpuid_entry with the following parameters, in order to fill the virtual CPUID entry in
	 *  vm->vcpuid_entries for CPUID.80000000H.
	 *  - vm
	 *  - &entry
	 */
	set_vcpuid_entry(vm, &entry);

	/** Set 'limit' to guest CPUID.80000000H:EAX, which is the maximum leaf of extended function CPUID information
	 */
	limit = entry.eax;
	/** Set 'vm->vcpuid_xlevel' to 'limit' */
	vm->vcpuid_xlevel = limit;
	/** For each 'i' ranging from 80000002H to 'limit' [with a step of 1] */
	for (i = CPUID_EXTEND_FUNCTION_2; i <= limit; i++) {
		/** Call init_vcpuid_entry with the following parameters, in order to initialize the virtual CPUID entry
		 *  for CPUID.(EAX='i').
		 *  - i
		 *  - 0H
		 *  - 0H
		 *  - &entry
		 */
		init_vcpuid_entry(i, 0U, 0U, &entry);
		/** Call set_vcpuid_entry with the following parameters, in order to fill the virtual CPUID entry
		 *  in vm->vcpuid_entries for CPUID.(EAX='i').
		 *  - vm
		 *  - &entry
		 */
		set_vcpuid_entry(vm, &entry);
	}
}

/**
 * @brief Fill virtual CPUID entries in vm->vcpuid_entries.
 *
 * Fill virtual CPUID entries in vm->vcpuid_entries.
 * It caches the emulated contents after execution of a CPUID instruction by a vCPU and it is designed for those leaves
 * that the emulated contents are consistent inside one VM and would not be changed at run time.
 * This includes CPUID leaf 0H, 2H, 3H, 4H (with all its sub-leaves), 6H, 7H (with sub-leaf 0H), 15H, 16H and all
 * supported extended function CPUID leaves except 80000001H.
 * It is supposed to be called only by 'create_vm' from 'vp-base.vm' module.
 *
 * @param[inout] vm A pointer to a virtual machine data structure whose vcpuid_entries is to be filled.
 *
 * @return void
 *
 * @pre vm != NULL
 * @pre vm->vcpuid_entry_nr == 0
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vm is different among parallel invocation.
 */
void set_vcpuid_entries(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct vcpuid_entry'.
	 *  - entry representing a temporary virtual CPUID entry, not initialized. */
	struct vcpuid_entry entry;
	/** Declare the following local variables of type uint32_t.
	 *  - limit representing the maximum leaf of basic function for the processor, not initialized. */
	uint32_t limit;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as CPUID leaf, not initialized.
	 *  - j representing the loop counter as CPUID sub-leaf, not initialized.
	 */
	uint32_t i, j;

	/** Call init_vcpuid_entry with the following parameters, in order to initialize the virtual CPUID entry for
	 *  CPUID.0H.
	 *  - 0H
	 *  - 0H
	 *  - 0H
	 *  - &entry
	 */
	init_vcpuid_entry(0U, 0U, 0U, &entry);
#ifdef QEMU
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	if (cpu_info->cpuid_level < 0x16U) {
		/* The cpuid with zero leaf returns the max level. Emulate that the 0x16U is supported */
		entry.eax = 0x16U;
	}
#endif
	/** Call set_vcpuid_entry with the following parameters, in order to fill the virtual CPUID entry
	 *  in vm->vcpuid_entries for CPUID.0H.
	 *  - vm
	 *  - &entry
	 */
	set_vcpuid_entry(vm, &entry);

	/** Set 'limit' to guest CPUID.0H:EAX, which is the maximum leaf of basic function */
	limit = entry.eax;
	/** Set 'vm->vcpuid_level' to 'limit' */
	vm->vcpuid_level = limit;

	/** For each 'i' ranging from 1H to 'limit' [with a step of 1] */
	for (i = 1U; i <= limit; i++) {
		/* cpuid 1/0xb is percpu related */
		/** If CPUID leaf 'i' is equal to 1H or BH or DH, which are the leaves that vary among vCPUs and can
		 *  be changed by vCPUs at runtime */
		if ((i == 1U) || (i == 0xbU) || (i == 0xdU)) {
			/** Continue to next iteration */
			continue;
		}

		/** Depending on CPUID leaf 'i' */
		switch (i) {
		/** CPUID leaf 'i' is 4H */
		case 0x04U:
			/** For each 'j' ranging from 0H to the first CPUID sub-leaf whose contents in EAX register
			 *  after execution of a CPUID instruction is equal to 0H [with a step of 1] */
			for (j = 0U;; j++) {
				/** Call init_vcpuid_entry with the following parameters, in order to initialize the
				 *  virtual CPUID entry for CPUID.(EAX=4H,ECX=j).
				 *  - i
				 *  - j
				 *  - CPUID_CHECK_SUBLEAF
				 *  - &entry
				 */
				init_vcpuid_entry(i, j, CPUID_CHECK_SUBLEAF, &entry);
				/** If guest CPUID.(EAX=4H,ECX=j):EAX is equal to 0H */
				if (entry.eax == 0U) {
					/** Terminate the loop */
					break;
				}
				/** Call set_vcpuid_entry with the following parameters, in order to fill the
				 *  virtual CPUID entry in vm->vcpuid_entries for CPUID.(EAX=4H,ECX=j).
				 *  - vm
				 *  - &entry
				 */
				set_vcpuid_entry(vm, &entry);
			}
			/** End of case */
			break;
		/** CPUID leaf 'i' is 7H */
		case 0x07U:
			/** Call init_vcpuid_entry with the following parameters, in order to initialize the
			 *  virtual CPUID entry for CPUID.(EAX=7H,ECX=0H).
			 *  - i
			 *  - 0H
			 *  - CPUID_CHECK_SUBLEAF
			 *  - &entry
			 */
			init_vcpuid_entry(i, 0U, CPUID_CHECK_SUBLEAF, &entry);
			/** Call set_vcpuid_entry with the following parameters, in order to fill the
			 *  virtual CPUID entry in vm->vcpuid_entries for CPUID.(EAX=7H,ECX=0H).
			 *  - vm
			 *  - &entry
			 */
			set_vcpuid_entry(vm, &entry);
			/** End of case */
			break;
		/* These features are disabled */
		/** CPUID leaf 'i' is 5H */
		case 0x05U: /* Monitor/Mwait */
		/** CPUID leaf 'i' is 8H */
		case 0x08U: /* unimplemented leaf */
		/** CPUID leaf 'i' is 9H */
		case 0x09U: /* Cache */
		/** CPUID leaf 'i' is AH */
		case 0x0aU: /* PMU is not supported */
		/** CPUID leaf 'i' is CH */
		case 0x0cU: /* unimplemented leaf */
		/** CPUID leaf 'i' is EH */
		case 0x0eU: /* unimplemented leaf */
		/** CPUID leaf 'i' is FH */
		case 0x0fU: /* Intel RDT */
		/** CPUID leaf 'i' is 10H */
		case 0x10U: /* Intel RDT */
		/** CPUID leaf 'i' is 11H */
		case 0x11U: /* unimplemented leaf */
		/** CPUID leaf 'i' is 12H */
		case 0x12U: /* SGX */
		/** CPUID leaf 'i' is 13H */
		case 0x13U: /* unimplemented leaf */
		/** CPUID leaf 'i' is 14H */
		case 0x14U: /* Intel Processor Trace */
			/** End of case, as the leaf is not supported. */
			break;
		/** Otherwise */
		default:
			/** Call init_vcpuid_entry with the following parameters, in order to initialize the
			 *  virtual CPUID entry for CPUID.(EAX='i').
			 *  - i
			 *  - 0H
			 *  - 0H
			 *  - &entry
			 */
			init_vcpuid_entry(i, 0U, 0U, &entry);
			/** Call set_vcpuid_entry with the following parameters, in order to fill the
			 *  virtual CPUID entry in vm->vcpuid_entries for CPUID.(EAX='i').
			 *  - vm
			 *  - &entry
			 */
			set_vcpuid_entry(vm, &entry);
			/** End of case */
			break;
		}
	}

	/** Call set_vcpuid_extended_function with the following parameters, in order to fill emulated
	 *  virtual CPUID entries for extended functions.
	 *  - vm
	 */
	set_vcpuid_extended_function(vm);
}

/**
 * @brief Check whether the specified CPUID leaf is per-CPU related or not.
 *
 * Check whether the specified CPUID leaf is per-CPU related or not. Per-CPU related means that the information returned
 * from a CPUID instruction are different among different vCPUs. The differences are normally caused by APIC ID, MSR,
 * or XCR0.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] leaf The contents of EAX register upon execution of a CPUID instruction.
 *
 * @return A Boolean value indicating whether the specified CPUID leaf is per-CPU related or not.
 *
 * @retval true The specified CPUID leaf is per-CPU related.
 * @retval false The specified CPUID leaf is not per-CPU related.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static inline bool is_percpu_related(uint32_t leaf)
{
	/** Return 'true' if \a leaf is equal to 1H, BH, DH, or 80000001H. Otherwise, return 'false'. */
	return ((leaf == 0x1U) || (leaf == 0xbU) || (leaf == 0xdU) || (leaf == 0x80000001U));
}

/**
 * @brief Emulate CPUID.1H for guest.
 *
 * Emulate CPUID.1H for guest.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[out] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[out] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static void guest_cpuid_01h(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - apicid representing the APIC ID associated with \a vcpu, initialized as the return value of
	 *  vlapic_get_apicid(vcpu_vlapic(vcpu)). */
	uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));
	/** Declare the following local variables of type uint64_t.
	 *  - cr4 representing the contents in guest CR4, not initialized. */
	uint64_t cr4;

	/** Call cpuid with the following parameters, in order to get the native processor information for CPUID.1H.
	 *  - 1H
	 *  - eax
	 *  - ebx
	 *  - ecx
	 *  - edx
	 */
	cpuid(0x1U, eax, ebx, ecx, edx);

	/** Clear Non-APIC-ID Bits (Bits 23 - 0) in guest CPUID.1H:EBX */
	*ebx &= ~APIC_ID_MASK;
	/** Set APIC ID Bits (Bits 31 - 24) in guest CPUID.1H:EBX to 'apicid' */
	*ebx |= (apicid << APIC_ID_SHIFT);

	/** Clear MONITOR/MWAIT Bit (Bit 3) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_MONITOR;

	/** Clear 64-bit DS Area Bit (Bit 2) and CPL Qualified Debug Store Bit (Bit 4) in guest CPUID.1H:ECX */
	*ecx &= ~(CPUID_ECX_DTES64 | CPUID_ECX_DS_CPL);

	/** Clear Safer Mode Extension Bit (Bit 6) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_SMX;

	/** Clear Enhanced Intel SpeedStep Technology Bit (Bit 7) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_EST;

	/** Clear Thermal Monitor 2 Bit (Bit 8) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_TM2;

	/** Clear Perfmon and Debug Capability Bit (Bit 15) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_PDCM;

	/** Clear Silicon Debug Bit (Bit 11) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_SDBG;

	/** Clear Process-context Identifiers Bit (Bit 17) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_PCID;

	/** Clear Virtual Machine Extensions Bit (Bit 5) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_VMX;

	/** Set 'cr4' to the return value of 'vcpu_get_cr4(vcpu)', which is guest CR4 */
	cr4 = vcpu_get_cr4(vcpu);
	/** Clear OSXSAVE Bit (Bit 27) in guest CPUID.1H:ECX */
	*ecx &= ~CPUID_ECX_OSXSAVE;
	/** If OSXSAVE Bit (Bit 18) in guest CR4 is equal to 1 */
	if ((cr4 & CR4_OSXSAVE) != 0UL) {
		/** Set OSXSAVE Bit (Bit 27) in guest CPUID.1H:ECX to 1 */
		*ecx |= CPUID_ECX_OSXSAVE;
	}

	/** Clear Debug Store Bit (Bit 21) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_DTES;

	/** Clear Virtual 8086 Mode Enhancements Bit (Bit 1) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_VME;

	/** Clear Debugging Extensions Bit (Bit 2) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_DE;

	/** Clear Memory Type Range Registers Bit (Bit 12) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_MTRR;

	/** Clear ACPI Bit (Bit 22) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_ACPI;

	/** Clear Thermal Monitor Bit (Bit 29) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_TM1;

	/** Clear Pending Break Enable Bit (Bit 31) in guest CPUID.1H:EDX */
	*edx &= ~CPUID_EDX_PBE;

	/** If 'vcpu->vm' is a safety VM */
	if (is_safety_vm(vcpu->vm)) {
		/** Clear HTT Bit (Bit 28) in guest CPUID.1H:EDX */
		*edx &= ~CPUID_EDX_HTT;
	} else {
		/** Clear Machine Check Exception Bit (Bit 7) in guest CPUID.1H:EDX */
		*edx &= ~CPUID_EDX_MCE;

		/** Clear Machine Check Architecture Bit (Bit 14) in guest CPUID.1H:EDX */
		*edx &= ~CPUID_EDX_MCA;
	}
}

/**
 * @brief Emulate CPUID.BH for guest.
 *
 * Emulate CPUID.BH for guest.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf or sub-leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[out] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[inout] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after create_vm has been called with \a vcpu->vm->vm_id as first parameter once on
 *         any processor.
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static void guest_cpuid_0bh(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - subleaf representing the contents of ECX register upon execution of a CPUID instruction, initialized as
	 *    '*ecx'. */
	uint32_t subleaf = *ecx;

	/* Patching X2APIC */
	/** Set high 24-bits of guest CPUID.(EAX=BH,ECX='subleaf'):ECX to 0H and
	 *  low 8-bits of guest CPUID.(EAX=BH,ECX='subleaf'):ECX to low 8-bits of 'subleaf' */
	*ecx = subleaf & 0xFFU;
	/* No HT emulation for UOS */
	/** Depending on 'subleaf' */
	switch (subleaf) {
	/** 'subleaf' is 0H */
	case 0U:
		/** Set guest CPUID.(EAX=BH,ECX=0H):EAX to 0H */
		*eax = 0U;
		/** Set guest CPUID.(EAX=BH,ECX=0H):EBX to 1H */
		*ebx = 1U;
		/** Set high 24-bits of guest CPUID.(EAX=BH,ECX=0H):ECX to 1H */
		*ecx |= (1U << 8U);
		/** End of case */
		break;
	/** 'subleaf' is 1H */
	case 1U:
		/** If 'vcpu->vm->hw.created_vcpus' is equal to 1H */
		if (vcpu->vm->hw.created_vcpus == 1U) {
			/** Set guest CPUID.(EAX=BH,ECX=1H):EAX to 0H */
			*eax = 0U;
		} else {
			/** Set guest CPUID.(EAX=BH,ECX=1H):EAX to 1 plus the index of most significant bit set in
			 *  'vcpu->vm->hw.created_vcpus - 1', which is number of bits to shift right on x2APIC ID
			 *  to get a unique topology ID of the next level type
			 */
			*eax = (uint32_t)fls32(vcpu->vm->hw.created_vcpus - 1U) + 1U;
		}
		/** Set guest CPUID.(EAX=BH,ECX=1H):EBX to 'vcpu->vm->hw.created_vcpus' */
		*ebx = vcpu->vm->hw.created_vcpus;
		/** Set high 24-bits of guest CPUID.(EAX=BH,ECX=1H):ECX to 2H */
		*ecx |= (2U << 8U);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Set guest CPUID.(EAX=BH,ECX='subleaf'):EAX to 0H */
		*eax = 0U;
		/** Set guest CPUID.(EAX=BH,ECX='subleaf'):EBX to 0H */
		*ebx = 0U;
		/** End of case */
		break;
	}
	/** Set guest CPUID.(EAX=BH,ECX='subleaf'):EDX to the APIC ID associated with \a vcpu */
	*edx = vlapic_get_apicid(vcpu_vlapic(vcpu));
}

/**
 * @brief Emulate CPUID.DH for guest.
 *
 * Emulate CPUID.DH for guest.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf or sub-leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction. This argument is not
 *                 used in current scope, keeping it here for scope extension in the future.
 * @param[out] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[inout] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static void guest_cpuid_0dh(__unused struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - subleaf representing the contents of ECX register upon execution of a CPUID instruction, initialized as
	 *    *ecx. */
	uint32_t subleaf = *ecx;
	/** Declare the following local variables of type uint32_t.
	 *  - avx_area_size representing the size of the XSAVE extended region area for AVX, not initialized.
	 *  - unused representing a variable used as a placeholder of unused CPUID information upon cpuid_subleaf calls,
	 *  not initialized.
	 */
	uint32_t avx_area_size, unused;

	/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
	 *  for CPUID.(EAX=DH,ECX=2H) and set 'avx_area_size' to native CPUID.(EAX=DH,ECX=2H):EAX.
	 *  - DH
	 *  - 2H
	 *  - &avx_area_size
	 *  - &unused
	 *  - &unused
	 *  - &unused
	 */
	cpuid_subleaf(0x0dU, 2U, &avx_area_size, &unused, &unused, &unused);

	/** Call cpuid_subleaf with the following parameters, in order to get the native processor information
	 *  for CPUID.(EAX=DH,ECX='subleaf') and fill the native processor information into the output parameter
	 *  \a eax, \a ebx, \a ecx, and \a edx respectively.
	 *  - DH
	 *  - subleaf
	 *  - eax
	 *  - ebx
	 *  - ecx
	 *  - edx
	 */
	cpuid_subleaf(0x0dU, subleaf, eax, ebx, ecx, edx);

	/** Depending on 'subleaf' */
	switch (subleaf) {
	/** 'subleaf' is 0H */
	case 0U:
		/* SDM Vol.1 17-2, On processors that do not support Intel MPX,
		 * CPUID.(EAX=0DH,ECX=0):EAX[3] and
		 * CPUID.(EAX=0DH,ECX=0):EAX[4] will both be 0 */
		/** Clear MPX BNDREG State Bit (Bit 3) in guest CPUID.(EAX=DH,ECX=0H):EAX */
		*eax &= ~CPUID_EAX_XCR0_BNDREGS;
		/** Clear MPX BNDCSR State Bit (Bit 4) in guest CPUID.(EAX=DH,ECX=0H):EAX */
		*eax &= ~CPUID_EAX_XCR0_BNDCSR;

		/*
		 * Correct EBX depends on correct initialization value of physical XCR0 and MSR IA32_XSS.
		 * Physical XCR0 shall be initialized to 1H.
		 * Physical MSR IA32_XSS shall be initialized to 0H.
		 */

		/** Set guest CPUID.(EAX=DH,ECX=0H):ECX to the sum of the size of the following areas:
		 *  XSAVE legacy region area, XSAVE header area, XSAVE extended region area for AVX. */
		*ecx = XSAVE_LEGACY_AREA_SIZE + XSAVE_HEADER_AREA_SIZE + avx_area_size;
		/** End of case */
		break;

	/** 'subleaf' is 1H */
	case 1U:
		/** Clear XSAVES/XRSTORS Bit (Bit 3) in guest CPUID.(EAX=DH,ECX=1H):EAX */
		*eax &= ~CPUID_EAX_XSAVES;

		/*
		 * Correct EBX depends on correct initialization value of physical XCR0 and MSR IA32_XSS.
		 * Physical XCR0 shall be initialized to 1H.
		 * Physical MSR IA32_XSS shall be initialized to 0H.
		 */

		/** Clear PT state Bit (Bit 8) in guest CPUID.(EAX=DH,ECX=1H):ECX */
		*ecx &= ~CPUID_ECX_PT_STATE;
		/** End of case */
		break;

	/** 'subleaf' is 2H */
	case 2U:
		/* AVX state: return native value */
		/** End of case, as AVX state component information is exposed as is. */
		break;

	/** Otherwise */
	default:
		/* hide all the other state */
		/** Set guest CPUID.(EAX=DH,ECX='subleaf'):EAX to 0H */
		*eax = 0U;
		/** Set guest CPUID.(EAX=DH,ECX='subleaf'):EBX to 0H */
		*ebx = 0U;
		/** Set guest CPUID.(EAX=DH,ECX='subleaf'):ECX to 0H */
		*ecx = 0U;
		/** Set guest CPUID.(EAX=DH,ECX='subleaf'):EDX to 0H */
		*edx = 0U;
		/** End of case */
		break;
	}
}

/**
 * @brief Emulate CPUID.80000001H for guest.
 *
 * Emulate CPUID.80000001H for guest.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[out] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[out] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre vcpu != NULL
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static void guest_cpuid_80000001h(
	const struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint64_t.
	 *  - guest_ia32_misc_enable representing the value of guest MSR IA32_MISC_ENABLE, initialized as return value
	 *    of 'vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE)'. */
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	/** Declare the following local variables of type uint32_t.
	 *  - leaf representing the contents of EAX register upon execution of a CPUID instruction,
	 *    initialized as 80000001H. */
	uint32_t leaf = 0x80000001U;

	/** Call cpuid with the following parameters, in order to get the native processor information for
	 *  CPUID.80000001H and set the initial values to the output parameters.
	 *  - leaf
	 *  - eax
	 *  - ebx
	 *  - ecx
	 *  - edx
	 */
	cpuid(leaf, eax, ebx, ecx, edx);
	/* SDM Vol4 2.1, XD Bit Disable of MSR_IA32_MISC_ENABLE
	 * When set to 1, the Execute Disable Bit feature (XD Bit) is disabled and the XD Bit
	 * extended feature flag will be clear (CPUID.80000001H: EDX[20]=0)
	 */
	/** If the XD Bit Disable Bit (Bit 34) of guest MSR IA32_MISC_ENABLE is equal to 1H */
	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
		/** Clear Execute Disable Bit (Bit 20) in guest CPUID.80000001H:EDX */
		*edx = *edx & ~CPUID_EDX_XD_BIT_AVIL;
	}
}

/**
 * @brief Emulate the CPUID instruction when the Limit CPUID Maxval Bit of guest MSR IA32_MISC_ENABLE is equal to 1H.
 *
 * Emulate the CPUID instruction when the Limit CPUID Maxval Bit (Bit 22) of guest MSR IA32_MISC_ENABLE is equal to 1H.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf or sub-leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'guest_cpuid'.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[in] leaf The contents of EAX register upon execution of a CPUID instruction.
 * @param[out] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[out] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre vcpu != NULL
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after set_vcpuid_entries has been called with \a vcpu->vm as parameter once on any
 *         processor.
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
static void guest_limit_cpuid(
	const struct acrn_vcpu *vcpu, uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint64_t.
	 *  - guest_ia32_misc_enable representing the value of guest MSR IA32_MISC_ENABLE, initialized as return value
	 *  of 'vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE)'. */
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	/** Declare the following local variables of type 'const struct vcpuid_entry *'.
	 *  - entry representing a pointer to a virtual CPUID entry containing the virtual processor information for
	 *  CPUID.2H, not initialized. */
	const struct vcpuid_entry *entry;

	/** If the Limit CPUID Maxval Bit (Bit 22) of guest MSR IA32_MISC_ENABLE is equal to 1H */
	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_LIMIT_CPUID) != 0UL) {
		/* limit the leaf number to 2 */
		/** If 'leaf' is equal to 0H */
		if (leaf == 0U) {
			/** Set guest CPUID.0H:EAX to 2H */
			*eax = 2U;
		/** If 'leaf' satisfies either one of the following conditions:
		 *  condition-a: 'leaf' is larger than 2H and smaller than 80000000H
		 *  condition-b: 'leaf' is larger than 'vcpu->vm->vcpuid_xlevel'
		 */
		} else if (((leaf > 2U) && (leaf < CPUID_MAX_EXTENDED_FUNCTION)) || (leaf > vcpu->vm->vcpuid_xlevel)) {
			/** Set 'entry' to the return value of 'find_vcpuid_entry(vcpu, 2U, 0U)', which contains the
			 *  virtual processor information for CPUID.2H */
			entry = find_vcpuid_entry(vcpu, 2U, 0U);
			/** Set guest CPUID.(EAX='leaf',ECX=*):EAX to guest CPUID.2H:EAX */
			*eax = entry->eax;
			/** Set guest CPUID.(EAX='leaf',ECX=*):EBX to guest CPUID.2H:EBX */
			*ebx = entry->ebx;
			/** Set guest CPUID.(EAX='leaf',ECX=*):ECX to guest CPUID.2H:ECX */
			*ecx = entry->ecx;
			/** Set guest CPUID.(EAX='leaf',ECX=*):EDX to guest CPUID.2H:EDX */
			*edx = entry->edx;
		} else {
			/* In this case,
			 * leaf is 1H, 2H, or extended function leaves in the range [80000000H, 80000008H],
			 * return the cpuid value get previously.
			 */
		}
	}
}

/**
 * @brief Emulate the CPUID instruction executed from guest.
 *
 * Emulate the CPUID instruction executed from guest.
 * The integer pointed to by eax and ecx are considered as the CPUID leaf and sub-leaf of the vCPU to be read.
 * The EAX, EBX, ECX and EDX of the specified CPUID leaf or sub-leaf of the vCPU are stored to the integers pointed by
 * eax, ebx, ecx and edx, respectively.
 * It is supposed to be called only by 'cpuid_vmexit_handler' from 'vp-base.hv_main' module and 'write_efer_msr' from
 * 'vp-base.vmsr' module.
 *
 * @param[in] vcpu A structure representing the vCPU attempting to execute a CPUID instruction.
 * @param[inout] eax The pointer which points to an address that stores the contents of EAX register.
 * @param[out] ebx The pointer which points to an address that stores the contents of EBX register.
 * @param[inout] ecx The pointer which points to an address that stores the contents of ECX register.
 * @param[out] edx The pointer which points to an address that stores the contents of EDX register.
 *
 * @return void
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after set_vcpuid_entries has been called with \a vcpu->vm as parameter once on any
 *         processor.
 * @remark This API shall be called after create_vm has been called with \a vcpu->vm->vm_id as first parameter once on
 *         any processor.
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation.
 */
void guest_cpuid(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - leaf representing the contents of EAX register upon execution of a CPUID instruction, initialized as
	 *  '*eax'. */
	uint32_t leaf = *eax;
	/** Declare the following local variables of type uint32_t.
	 *  - subleaf representing the contents of ECX register upon execution of a CPUID instruction, initialized as
	 *  '*ecx'. */
	uint32_t subleaf = *ecx;

	/* vm related */
	/** If the specified CPUID leaf is not per-CPU related */
	if (!is_percpu_related(leaf)) {
		/** Declare the following local variables of type 'const struct vcpuid_entry *'.
		 *  - entry representing a pointer which points to a virtual CPUID entry, initialized as the return
		 *  value of 'find_vcpuid_entry(vcpu, leaf, subleaf)'. If 'entry' is not a NULL pointer, it contains the
		 *  virtual processor information for CPUID.(EAX='leaf',ECX='subleaf').
		 */
		const struct vcpuid_entry *entry = find_vcpuid_entry(vcpu, leaf, subleaf);

		/** If 'entry' is not a NULL pointer */
		if (entry != NULL) {
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EAX to 'entry->eax' */
			*eax = entry->eax;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EBX to 'entry->eax' */
			*ebx = entry->ebx;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):ECX to 'entry->eax' */
			*ecx = entry->ecx;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EDX to 'entry->eax' */
			*edx = entry->edx;
		} else {
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EAX to to 0H */
			*eax = 0U;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EBX to to 0H */
			*ebx = 0U;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):ECX to to 0H */
			*ecx = 0U;
			/** Set guest CPUID.(EAX='leaf',ECX='subleaf'):EDX to to 0H */
			*edx = 0U;
		}
	} else {
		/* percpu related */
		/** Depending on 'leaf' */
		switch (leaf) {
		/** 'leaf' is 1H */
		case 0x01U:
			/** Call guest_cpuid_01h with the following parameters, in order to emulate CPUID.1H for guest.
			 *  - vcpu
			 *  - eax
			 *  - ebx
			 *  - ecx
			 *  - edx
			 */
			guest_cpuid_01h(vcpu, eax, ebx, ecx, edx);
			/** End of case */
			break;

		/** 'leaf' is BH */
		case 0x0bU:
			/** Call guest_cpuid_0bh with the following parameters, in order to emulate CPUID.BH for guest.
			 *  - vcpu
			 *  - eax
			 *  - ebx
			 *  - ecx
			 *  - edx
			 */
			guest_cpuid_0bh(vcpu, eax, ebx, ecx, edx);
			/** End of case */
			break;

		/** 'leaf' is DH */
		case 0x0dU:
			/** Call guest_cpuid_0dh with the following parameters, in order to emulate CPUID.DH for guest.
			 *  - vcpu
			 *  - eax
			 *  - ebx
			 *  - ecx
			 *  - edx
			 */
			guest_cpuid_0dh(vcpu, eax, ebx, ecx, edx);
			/** End of case */
			break;

		/** 'leaf' is 80000001H */
		case 0x80000001U:
			/** Call guest_cpuid_80000001h with the following parameters, in order to emulate
			 *  CPUID.80000001H for guest.
			 *  - vcpu
			 *  - eax
			 *  - ebx
			 *  - ecx
			 *  - edx
			 */
			guest_cpuid_80000001h(vcpu, eax, ebx, ecx, edx);
			/** End of case */
			break;

		/** Otherwise */
		default:
			/*
			 * In this switch statement, leaf shall either be 0x01U or 0x0bU
			 * or 0x0dU or 0x80000001U. All the other cases have been handled properly
			 * before this switch statement.
			 * Gracefully return if prior case clauses have not been met.
			 */
			/** Unexpected case. Break gracefully as a defensive action. */
			break;
		}
	}

	/** Call guest_limit_cpuid with the following parameters, in order to emulate the CPUID instruction when
	 *  the Limit CPUID Maxval Bit (Bit 22) of guest MSR IA32_MISC_ENABLE is equal to 1H.
	 *  - vcpu
	 *  - leaf
	 *  - eax
	 *  - ebx
	 *  - ecx
	 *  - edx
	 */
	guest_limit_cpuid(vcpu, leaf, eax, ebx, ecx, edx);
}

/**
 * @}
 */
