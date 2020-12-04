/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <acrn_hv_defs.h>
#include <types.h>
#include <atomic.h>
#include <page.h>
#include <pgtable.h>
#include <cpu_caps.h>
#include <mmu.h>
#include <vmx.h>
#include <reloc.h>
#include <vm.h>
#include <ld_sym.h>
#include <logmsg.h>

/**
 * @defgroup hwmgmt_mmu hwmgmt.mmu
 * @ingroup hwmgmt
 * @brief The definition and implementaion of MMU related public APIs, macros and
 *        structures, as well as private helper functions for realizing the features
 *        of the external APIs.
 *
 * Usage:
 * - vp-base.vcpu depends on this module to invalidates all mappings in the TLBs
 *                and paging-structure caches that are tagged with all VPIDs.
 * - vp-base.virq depends on this module to invalidate mappings in the translation
 *                lookaside buffers (TLBs) and paging-structure caches that were
 *                derived from extended page tables (EPT).
 * - hwmgmt.page depends on this module to set the PTE entry in the sanitized page.
 * - vp-base.vm depends on this module to set PTE entries in the sanitized page.
 * - hwmgmt.cpu depends on this module to initialize and configure paging.
 * - vp-base.guest_mem depends on this module to flush address space.
 * - hwmgmt.apic depends on this module to update memory pages to be owned by hypervisor.
 * - hwmgmt.iommu depends on this module to update memory pages to be owned by hypervisor.
 *
 * Dependency:
 * - This module depends on debug module to print debug information.
 * - This module depends on hwmgmt.cpu_caps to check cpu capability.
 * - This module depends on hwmgmt.page for paging structure operations.
 * - This module depends on hwmgmt.cpu for accessing cpu registers.
 * - This module depends on hwmgmt.configs for e820 configurations.
 *
 * @{
 */

/**
 * @file
 * @brief The definition and implementation of public APIs, macros and
 *        structures for MMU related stuff.
 *
 * It provides external APIs for initializing and operating MMU, as well as
 * internal helper functions to implement these public APIs.
 *
 * This file implements following external APIs that shall be provided by the hwmgmt.mmu module.
 * - flush_vpid_global
 * - invept
 * - sanitize_pte_entry
 * - sanitize_pte
 * - enable_paging
 * - enable_smep
 * - enable_smap
 * - hv_access_memory_region_update
 * - init_paging
 * - flush_address_space
 *
 * This file implements following internal functions to help realize the features of the external APIs.
 * - asm_invvpid
 * - local_invvpid
 * - asm_invept
 * - local_invept
 * - get_sanitized_page
 */

/** Declare the following static variable of type void*
 *  - ppt_mmu_pml4_addr representing the HVA of the page map level 4,
 *                      not initialized.
 */
static void *ppt_mmu_pml4_addr;

/** Declare the following static variable of type 'uint8_t[]'
 *  - sanitized_page representing a sanitized page used in EPT to avoiding L1TF-based attack.
 *                   ACRN always enables EPT for all guests (SOS and UOS), thus a malicious
 *                   guest can directly control guest PTEs to construct L1TF-based attack to
 *                   hypervisor. Alternatively if ACRN EPT is not sanitized with some PTEs
 *                   (with present bit cleared, or reserved bit set) pointing to valid host PFNs,
 *                   a malicious guest may use those EPT PTEs to construct an attack.
 *                   not initialized.
 */
static uint8_t sanitized_page[PAGE_SIZE] __aligned(PAGE_SIZE);

/**
 * @brief Single-context invalidation
 *
 * If the INVEPT type is 1, the logical processor invalidates all mappings associated
 * with bits 51:12 of the EPT pointer (EPTP) specified in the INVEPT descriptor.
 */
#define INVEPT_TYPE_SINGLE_CONTEXT 1UL
/**
 * @brief Global invalidation
 *
 * If the INVEPT type is 2, the logical processor invalidates mappings associated
 * with all EPTPs.
 */
#define INVEPT_TYPE_ALL_CONTEXTS   2UL
/**
 * @brief A piece of assembly code for checking the error when implementing "invept".
 *
 * If the "invept" fails with RFLAGS.CF equals to 1, meaning this is a invalid invept type,
 * then set the error code to be 1; if the "invept" fails with RFLAGS.ZF equals to 1, meaning
 * fail due to the EPTP value in single-context invalidation, then set the error code to be 2.
 * If the "invept" succeed, then set the error code to be 0.
 */
#define VMFAIL_INVALID_EPT_VPID                   \
	"       jnc 1f\n"                         \
	"       mov $1, %0\n" /* CF: error = 1 */ \
	"       jmp 3f\n"                         \
	"1:     jnz 2f\n"                         \
	"       mov $2, %0\n" /* ZF: error = 2 */ \
	"       jmp 3f\n"                         \
	"2:     mov $0, %0\n"                     \
	"3:"

/**
 * @brief This data structure defines "invvpid" descriptor format.
 *
 * @consistency N/A
 * @alignment 1
 *
 * @remark N/A
 */
struct invvpid_operand {
	uint32_t vpid : 16; /**< VPID in the "invvpid" descriptor */
	uint32_t rsvd1 : 16; /**< Reserved bits in the "invvpid" descriptor */
	uint32_t rsvd2 : 32; /**< Reserved bits in the "invvpid" descriptor */
	uint64_t gva; /**< Guest linear address in the "invvpid" descriptor */
};

/**
 * @brief This data structure defines "invept" descriptor format.
 *
 * @consistency N/A
 * @alignment 1
 *
 * @remark N/A
 */
struct invept_desc {
	uint64_t eptp; /**< EPTP in the "invept" descriptor */
	uint64_t res;  /**< Reserved bits in the "invept" descriptor */
};

/**
 * @brief This is a wrap of "invvpid" inline assembly.
 *
 * @param[in] operand The "invvpid" descriptor.
 * @param[in] type The operation type parameter for "invvpid".
 *
 * @return The error code of "invvpid". Please refer to SDM 30.2 and 30.3 (INVVPID) Vol.3.
 *
 * @retval 1 If called with an invalid invvpid type.
 * @retval 2 If called with an invalid VPID in INVVPID Descriptor when the INVVPID type
 *           belongs to individual-address invalidation, single-context invalidation
 *           or single-context invalidation.
 * @retval 0 Otherwise.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline int32_t asm_invvpid(const struct invvpid_operand operand, uint64_t type)
{
	/** Declare the following local variable of type int32_t
	 *  - error representing the error code of "invvpid", not initialized.
	 */
	int32_t error;
	/** Execute inline assembly ("invvpid")
	 *  with following parameters, in order to invalidate mappings in the
	 *  translation lookaside buffers (TLBs) and paging-structure caches
	 *  based on virtual processor identifier (VPID).
	 *  - Instruction template: "invvpid %1, %2\n" VMFAIL_INVALID_EPT_VPID
	 *  - Input operands:
	 *	- Memory pointed to by operand holds a "invvpid" descriptor.
	 *	- A general register holds the operation type parameter for "invvpid".
	 *  - Output operands:
	 *	- error is stored to a general register.
	 *  - Clobbers: "memory"
	 */
	asm volatile("invvpid %1, %2\n" VMFAIL_INVALID_EPT_VPID : "=r"(error) : "m"(operand), "r"(type) : "memory");

	/** Return error */
	return error;
}

/**
 * @brief This function does "invvpid" based on a specific (vpid, gva).
 *
 * It builds "invvpid" descriptor from \a vpid and \a gva. Then it calls asm_invvpid
 * to do "invvpid".
 *
 * @param[in] type The operation type parameter for "invvpid".
 * @param[in] vpid The virtual processor identifier whose mappings in the
 *                 translation lookaside buffers (TLBs) and paging-structure
 *                 caches are going to be invalidated.
 * @param[in] gva The guest linear address whose mappings in the translation lookaside
 *                buffers (TLBs) and paging-structure caches are going to be invalidated.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void local_invvpid(uint64_t type, uint16_t vpid, uint64_t gva)
{
	/** Declare the following local variable of type const struct invvpid_operand
	 *  - operand representing the "invvpid" descriptor for "invvpid",
	 *            initialized as { vpid, 0U, 0U, gva }.
	 */
	const struct invvpid_operand operand = { vpid, 0U, 0U, gva };

	/** Call asm_invvpid with following parameters, in order to check its return value.
	 *  - operand
	 *  - type
	 * If the return value is not equal to 0H.
	 */
	if (asm_invvpid(operand, type) != 0) {
		/** Logging the following information for debug purpose.
		 *  - __func__
		 *  - type
		 *  - vpid
		 */
		pr_dbg("%s, failed. type = %lu, vpid = %u", __func__, type, vpid);
	}
}

/**
 * @brief This is a wrap of "invept" inline assembly.
 *
 * @param[in] type The operation type parameter for "invept".
 * @param[in] desc The "invept" descriptor.
 *
 * @return The error code of "invept". Please refer to SDM 30.2 and 30.3 (INVEPT) Vol.3.
 *
 * @retval 1 If called with an invalid invept type.
 * @retval 2 If called with an invalid EPTP in INVEPT Descriptor when the INVEPT type
 *           is Single-context invalidation.
 * @retval 0 Otherwise.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline int32_t asm_invept(uint64_t type, struct invept_desc desc)
{
	/** Declare the following local variable of type int32_t
	 *  - error representing the error code of "invept", not initialized.
	 */
	int32_t error;
	/** Execute inline assembly ("invept")
	 *  with following parameters, in order to invalidates mappings in the
	 *  translation lookaside buffers (TLBs) and paging-structure caches
	 *  that were derived from extended page tables (EPT).
	 *  - Instruction template: "invept %1, %2\n" VMFAIL_INVALID_EPT_VPID
	 *  - Input operands:
	 *      1. Memory pointed to by desc holds a "invept" descriptor.
	 *      2. A general register holds the operation type parameter for "invept".
	 *  - Output operands:
	 *      1. error is stored to a general register.
	 *  - Clobbers: "memory"
	 */
	asm volatile("invept %1, %2\n" VMFAIL_INVALID_EPT_VPID : "=r"(error) : "m"(desc), "r"(type) : "memory");

	/** Return error */
	return error;
}

/**
 * @brief This function does "invept" based on a specific descriptor.
 *
 * It calls asm_invept to do "invept".
 *
 * @param[in] type The operation type parameter for "invvpid".
 * @param[in] desc The "invept" descriptor.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void local_invept(uint64_t type, struct invept_desc desc)
{
	/** Call asm_invept with following parameters, in order to check its return value.
	 *  - type
	 *  - desc
	 * If the return value is not equal to 0H.
	 */
	if (asm_invept(type, desc) != 0) {
		/** Logging the following information for debug purpose.
		 *  - __func__
		 *  - type
		 *  - desc.eptp
		 */
		pr_dbg("%s, failed. type = %lu, eptp = 0x%lx", __func__, type, desc.eptp);
	}
}

/**
 * @brief The function invalidates all mappings (linear mappings, guest-physical
 *        mappings, and combined mappings) in the TLBs and paging-structure caches
 *        that are tagged with all VPIDs (Virtual Processor IDs).
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void flush_vpid_global(void)
{
	/** Call local_invvpid with following parameters, in order to invalidates all mappings
	 *  (linear mappings, guest-physical mappings, and combined mappings) in the TLBs and
	 *  paging-structure caches that are tagged with all VPIDs (Virtual Processor IDs).
	 *  - VMX_VPID_TYPE_ALL_CONTEXT
	 *  - 0H
	 *  - 0H
	 */
	local_invvpid(VMX_VPID_TYPE_ALL_CONTEXT, 0U, 0UL);
}

/**
 * @brief The function invalidates mappings in the translation lookaside buffers (TLBs) and paging-structure caches
 *        that were derived from extended page tables (EPT).
 *
 * @param[in] eptp A pointer to the extended page tables (EPT).
 *
 * @return None
 *
 * @pre When this function is called, the VCPU structure in the calling context doesn't equals to NULL.
 * @pre When this function is called, pcpuid_from_vcpu(vcpu) == get_pcpu_id() shall be met in the calling context.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void invept(const void *eptp)
{
	/** Declare the following local variable of type struct invept_desc desc
	 *  - desc representing the "invept" descriptor for "invept",
	 *         initialized as { 0 }.
	 */
	struct invept_desc desc = { 0 };

	/** Call hva2hpa with following parameters to get the HPA of eptp
	 *  from its HVA.
	 *  - eptp
	 *  Set the bits (3 ~ 5) to be 011b, bits (0 ~ 2) to be 110b.
	 */
	desc.eptp = hva2hpa(eptp) | (3UL << 3U) | 6UL;
	/** Call local_invept with following parameters, in order to
	 *  do a single-context invalidation.
	 *  - INVEPT_TYPE_SINGLE_CONTEXT
	 *  - desc
	 */
	local_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
}

/**
 * @brief This function gets the HPA of the sanitized page.
 *
 * @return The HPA of the sanitized page.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t get_sanitized_page(void)
{
	/** Call hva2hpa with following parameters, in order to get the HPA of
	 *  the sanitized page from its HVA.
	 *  - sanitized_page
	 *  Return the hva2hpa(sanitized_page).
	 */
	return hva2hpa(sanitized_page);
}

/**
 * @brief This function sets the PTE entry in the sanitized page.
 *
 * @param[in] ptep A pointer to a PTE in the sanitized page.
 * @param[in] mem_ops The handle of operations associated with the PTE.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void sanitize_pte_entry(uint64_t *ptep, const struct memory_ops *mem_ops)
{
	/** Call set_pgentry with following parameters, in order to
	 *  set a PTE entry in sanitized page.
	 *  - ptep
	 *  - get_sanitized_page()
	 *  - mem_ops
	 */
	set_pgentry(ptep, get_sanitized_page(), mem_ops);
}

/**
 * @brief This function sets all PTE entries in the sanitized page.
 *
 * @param[in] pt_page The pointer to the sanitized page.
 * @param[in] mem_ops The handle of operations associated with the PTEs in the sanitized page.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 * @mode HV_SUBMODE_INIT_ROOT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void sanitize_pte(uint64_t *pt_page, const struct memory_ops *mem_ops)
{
	/** Declare the following local variable of type uint64_t
	 *  - i representing the ith index of PTE in the sanitized page, not initialized.
	 */
	uint64_t i;

	/** For each i ranging from 0H to PTRS_PER_PTE - 1 */
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		/** Call sanitize_pte_entry with following parameters, in order to
		 *  set all PTE entries in the sanitized page.
		 *  - pt_page + i
		 *  - mem_ops
		 */
		sanitize_pte_entry(pt_page + i, mem_ops);
	}
}

/**
 * @brief This function is used to enable paging.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void enable_paging(void)
{
	/** Declare the following local variable of type uint64_t
	 *  - tmp64 representing a temporary IA32_EFER, initialized as 0H.
	 */
	uint64_t tmp64 = 0UL;

	/** Call msr_read with following parameters, in order to
	 *  read the current value in physical IA32_EFER, and set
	 *  tmp64 to its return value.
	 *  - MSR_IA32_EFER
	 */
	tmp64 = msr_read(MSR_IA32_EFER);
	/** Enable MSR IA32_EFER.NXE bit in tmp64 to prevent
	 *  instruction fetching from pages with XD bit set.
	 */
	tmp64 |= MSR_IA32_EFER_NXE_BIT;
	/** Call msr_write with following parameters, in order to
	 *  write tmp64 back to the MSR IA32_EFER.
	 *  - MSR_IA32_EFER
	 *  - tmp64
	 */
	msr_write(MSR_IA32_EFER, tmp64);

	/** Call CPU_CR_READ with following parameters, in order to get cr0 value
	 *  and save it into tmp64
	 *  - cr0
	 *  - &tmp64
	 */
	CPU_CR_READ(cr0, &tmp64);
	/** Call CPU_CR_WRITE with following parameters, in order to enable Write
	 *  Protect, inhibiting writing to read-only pages by saving tmp64 | CR0_WP
	 *  into host cr0.
	 *  - cr0
	 *  - tmp64 | CR0_WP
	 */
	CPU_CR_WRITE(cr0, tmp64 | CR0_WP);

	/** Call CPU_CR_WRITE with following parameters, in order to write ppt_mmu_pml4_addr into
	 *  host cr3. HPA->HVA is 1:1 mapping at this moment, simply treat ppt_mmu_pml4_addr as
	 *  HPA.
	 *  - cr3
	 *  - ppt_mmu_pml4_addr
	 */
	CPU_CR_WRITE(cr3, ppt_mmu_pml4_addr);
}

/**
 * @brief This function enables the Supervisor Mode Execution Prevention in CR4.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void enable_smep(void)
{
	/** Declare the following local variable of type uint64_t
	 *  - val64 representing a temporary variable to store CR4 value,
	 *    initialized as 0H.
	 */
	uint64_t val64 = 0UL;

	/** Call CPU_CR_READ with following parameters, in order to
	 *  Read CR4 value and store it into val64.
	 *  - cr4
	 *  - &val64
	 */
	CPU_CR_READ(cr4, &val64);
	/** Call CPU_CR_WRITE with following parameters, in order to write val64 | CR4_SMEP
	 *  into cr4 to enable CR4.SMEP.
	 *  - cr4
	 *  - val64 | CR4_SMEP
	 */
	CPU_CR_WRITE(cr4, val64 | CR4_SMEP);
}

/**
 * @brief This function enables the Supervisor Mode Access Prevention in CR4.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void enable_smap(void)
{
	/** Declare the following local variable of type uint64_t
	 *  - val64 representing a temporary variable to store CR4 value,
	 *    initialized as 0H.
	 */
	uint64_t val64 = 0UL;

	/** Call CPU_CR_READ with following parameters, in order to read CR4 value
	 *  and store it into val64.
	 *  - cr4
	 *  - &val64
	 */
	CPU_CR_READ(cr4, &val64);
	/** Call CPU_CR_WRITE with following parameters, in order to enable CR4.SMAP
	 *  by writing (val64 | CR4_SMAP) into cr4.
	 *  - cr4
	 *  - val64 | CR4_SMAP
	 */
	CPU_CR_WRITE(cr4, val64 | CR4_SMAP);
}

/**
 * @brief This function is used to update an memory region to be owned by hypervisor.
 *
 * The region specified by \a base and \a size will be added into hypervisor's page table.
 *
 * @param[in] base The start virtual address of specified memory region.
 * @param[in] size The size in byte of specified memory region.
 *
 * @return None
 *
 * @pre 0H < (base + size + PDE_SIZE -1)&PDE_MASK <= (get_e820_mem_info()->mem_top + PDE_SIZE -1) & PDE_MASK
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
void hv_access_memory_region_update(uint64_t base, uint64_t size)
{
	/** Declare the following local variable of type uint64_t
	 *  - base_aligned representing a 2-Mbyte aligned base, not initialized.
	 */
	uint64_t base_aligned;
	/** Declare the following local variable of type uint64_t
	 *  - size_aligned representing an aligned size, not initialized.
	 */
	uint64_t size_aligned;
	/** Declare the following local variable of type uint64_t
	 *  - region_end representing the end address of a memory region to be added
	 *               into hypervisor's page table, initialized as (base + size).
	 */
	uint64_t region_end = base + size;

	/** Call round_pde_down with following parameters, in order to round down base
	 *  to 2-Mbyte aligned. And Set base_aligned to be its return value.
	 *  - base
	 */
	base_aligned = round_pde_down(base);
	/** Call round_pde_up with following parameters, in order to set size_aligned to be
	 *  its return value.
	 *  - region_end - base_aligned
	 */
	size_aligned = round_pde_up(region_end - base_aligned);

	/** Call mmu_modify_or_del with following parameters, in order to clear the
	 *  user/supervisor bit in the paging structure entries of the specified
	 *  memory region which allows the hypervisor to access to the region with
	 *  SMAP activated.
	 *  - (uint64_t *)ppt_mmu_pml4_addr
	 *  - base_aligned
	 *  - size_aligned
	 *  - 0UL
	 *  - PAGE_USER
	 *  - &ppt_mem_ops
	 *  - MR_MODIFY
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, base_aligned, size_aligned, 0UL, PAGE_USER,
		&ppt_mem_ops, MR_MODIFY);
}

/**
 * @brief This function is used to do MMU page tables initialization
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
void init_paging(void)
{
	/** Declare the following local variable of type uint64_t
	 *  - hv_hpa representing the start HPA of the hypervisor code section, not initialized.
	 *  - text_end representing the end HPA of the hypervisor code section, not initialized.
	 *  - size representing the size of the hypervisor code section, not initialized.
	 */
	uint64_t hv_hpa, text_end, size;
	/** Declare the following local variable of type uint32_t
	 *  - i representing the ith e820 entry index, not initialized.
	 */
	uint32_t i;
	/** Declare the following local variable of type uint64_t
	 *  - low32_max_ram representing the HVA right after the highest RAM region below 4G,
	 *                  initialized as 0H.
	 */
	uint64_t low32_max_ram = 0UL;
	/** Declare the following local variable of type uint64_t
	 *  - high64_max_ram representing the max HVA beyond 4G, not initialized.
	 */
	uint64_t high64_max_ram;
	/** Declare the following local variable of type uint64_t
	 *  - attr_uc representing the page attribute for mapping the whole HVA space, initialized as
	 *            (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_CACHE_UC | PAGE_NX).
	 */
	uint64_t attr_uc = (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_CACHE_UC | PAGE_NX);
	/** Declare the following local variable of type const struct e820_entry
	 *  - entry representing a e820 entry pointer, not initialized.
	 */
	const struct e820_entry *entry;
	/** Declare the following local variable of type uint32_t
	 *  - entries_count representing the number of e820 entries,
	 *                  initialized as get_e820_entries_count().
	 */
	uint32_t entries_count = get_e820_entries_count();
	/** Declare the following local variable of type uint32_t
	 *  - p_e820 representing the address of e820 table,
	 *           initialized as get_e820_entry().
	 */
	const struct e820_entry *p_e820 = get_e820_entry();
	/** Declare the following local variable of type const struct mem_range
	 *  - p_mem_range_info representing memory range information calculated from
	 *                     the e820 table, initialized as get_mem_range_info().
	 */
	const struct mem_range *p_mem_range_info = get_mem_range_info();

	/** Print a debug inforamtion: "HV MMU Initialization" */
	pr_dbg("HV MMU Initialization");

	/** Call round_pde_up with following parameters and assign its
	 *  the return value to high64_max_ram, in order to
	 *  align high64_max_ram to 2MB.
	 *  - p_mem_range_info->mem_top
	 */
	high64_max_ram = round_pde_up(p_mem_range_info->mem_top);
	/** If high64_max_ram is bigger than (CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)
	 *  or high64_max_ram is less than 4G.
	 */
	if ((high64_max_ram > (CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)) || (high64_max_ram < (1UL << 32U))) {
		/** Logging the following information for debug purpose.
		 *  - high64_max_ram
		 *  - CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE
		 */
		printf("ERROR!!! high64_max_ram: 0x%lx, top address space: 0x%lx\n", high64_max_ram,
			CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE);
		/** Generate a panic with following message
		 *  - "Please configure HV_ADDRESS_SPACE correctly!\n"
		 */
		panic("Please configure HV_ADDRESS_SPACE correctly!\n");
	}

	/** Get the base HVA of memory for hypervisor PML4 table */
	ppt_mmu_pml4_addr = ppt_mem_ops.get_pml4_page(ppt_mem_ops.info);

	/** Call mmu_add with following parameters, in order to map the memory region
	 *  (0 ~ high64_max_ram) to corresponding UC attribute.
	 *  - (uint64_t *)ppt_mmu_pml4_addr
	 *  - 0H
	 *  - 0H
	 *  - high64_max_ram - 0H
	 *  - attr_uc
	 *  - &ppt_mem_ops
	 */
	mmu_add((uint64_t *)ppt_mmu_pml4_addr, 0UL, 0UL, high64_max_ram - 0UL, attr_uc, &ppt_mem_ops);

	/** For each i ranging from 0H to entries_count */
	for (i = 0U; i < entries_count; i++) {
		/** Set entry to be p_e820 + i */
		entry = p_e820 + i;
		/** If entry->type equals to E820_TYPE_RAM */
		if (entry->type == E820_TYPE_RAM) {
			/** If entry->baseaddr is less than  (1UL << 32U) */
			if (entry->baseaddr < (1UL << 32U)) {
				/** Declare the following local variable of type uint64_t
				 *  - end representing the end of a RAM region,
				 *        initialized as entry->baseaddr + entry->length.
				 */
				uint64_t end = entry->baseaddr + entry->length;
				/** If end is less than (1UL << 32U) and end is bigger than low32_max_ram */
				if ((end < (1UL << 32U)) && (end > low32_max_ram)) {
					/** Set low32_max_ram to be end */
					low32_max_ram = end;
				}
			}
		}
	}

	/** Call mmu_modify_or_del with following parameters, in order to
	 *  in order to clear page-level write-through bit and page-level
	 *  cache disable bit in the paging structure entries of the specified
	 *  memory region, which will use write back caching.
	 *  - (uint64_t *)ppt_mmu_pml4_addr
	 *  - 0UL
	 *  - round_pde_up(low32_max_ram)
	 *  - PAGE_CACHE_WB
	 *  - PAGE_CACHE_MASK
	 *  - &ppt_mem_ops
	 *  - MR_MODIFY
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, 0UL, round_pde_up(low32_max_ram), PAGE_CACHE_WB,
		PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	/** Call mmu_modify_or_del with following parameters, in order to
	 *  clear page-level write-through bit and page-level cache disable bit
	 *  in the paging structure entries of the specified memory region, which
	 *  will use write back caching.
	 *  - (uint64_t *)ppt_mmu_pml4_addr
	 *  - (1UL << 32U)
	 *  - high64_max_ram - (1UL << 32U)
	 *  - PAGE_CACHE_WB
	 *  - PAGE_CACHE_MASK
	 *  - &ppt_mem_ops
	 *  - MR_MODIFY
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, (1UL << 32U), high64_max_ram - (1UL << 32U), PAGE_CACHE_WB,
		PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	/** Call get_hv_image_base in order to get the start HPA of the hypervisor code section
	 *  and assign the return value to the hv_hpa
	 */
	hv_hpa = get_hv_image_base();
	/** Call mmu_modify_or_del with following parameters, in order to
	 *  clear page-level write-through bit, page-level cache disable bit and the
	 *  user/supervisor in the paging structure entries of the specified memory
	 *  region, which will use write back caching and allow the hypervisor to access to the
	 *  region with SMAP activated.
	 *  - (uint64_t *)ppt_mmu_pml4_addr
	 *  - hv_hpa & PDE_MASK
	 *  - CONFIG_HV_RAM_SIZE + (((hv_hpa & (PDE_SIZE - 1UL)) != 0UL) ? PDE_SIZE : 0UL)
	 *  - PAGE_CACHE_WB
	 *  - PAGE_CACHE_MASK | PAGE_USER
	 *  - &ppt_mem_ops
	 *  - MR_MODIFY
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, hv_hpa & PDE_MASK,
		CONFIG_HV_RAM_SIZE + (((hv_hpa & (PDE_SIZE - 1UL)) != 0UL) ? PDE_SIZE : 0UL), PAGE_CACHE_WB,
		PAGE_CACHE_MASK | PAGE_USER, &ppt_mem_ops, MR_MODIFY);

	/** Set size to be ((uint64_t)&ld_text_end - hv_hpa) */
	size = ((uint64_t)&ld_text_end - hv_hpa);
	/** Set text_end to be hv_hpa + size */
	text_end = hv_hpa + size;

	/**
	 * remove 'NX' bit for pages that contain hv code section, as by default XD bit is set for
	 * all pages, including pages for guests.
	 *
	 * Call mmu_modify_or_del with following parameters, in order to
	 * clear execute-disable bit in the paging structure entries of the
	 * specified memory region, which makes instruction fetches in this
	 * region are allowed.
	 * - (uint64_t *)ppt_mmu_pml4_addr
	 * - round_pde_down(hv_hpa)
	 * - round_pde_up(text_end) - round_pde_down(hv_hpa)
	 * - 0UL
	 * - PAGE_NX
	 * - &ppt_mem_ops
	 * - MR_MODIFY
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, round_pde_down(hv_hpa),
		round_pde_up(text_end) - round_pde_down(hv_hpa), 0UL, PAGE_NX, &ppt_mem_ops, MR_MODIFY);

	/** Call enable_paging in order to enable paging */
	enable_paging();

	/** Call sanitize_pte with following parameters, in order to
	 *  set entries in sanitized page point to itself.
	 *  - (uint64_t *)sanitized_page
	 *  - &ppt_mem_ops
	 */
	sanitize_pte((uint64_t *)sanitized_page, &ppt_mem_ops);
}

/**
 * @brief This function is used to flush address space by clflushopt instruction.
 *
 * @param[in] addr The the linear address to be flushed.
 * @param[in] size The size in byte to be flushed.
 *
 * @return None
 *
 * @pre addr != NULL && size != 0
 * @pre addr shall be CACHE_LINE_SIZE aligned
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void flush_address_space(void *addr, uint64_t size)
{
	/** Declare the following local variable of type uint64_t
	 *  - n representing nth byte in size, initialized as 0H.
	 */
	uint64_t n = 0UL;

	/** While n is less than size */
	while (n < size) {
		/** Call clflushopt with following parameters, in order to
		 *  optimally flush the cache line of a given address.
		 *  - (char *)addr + n
		 */
		clflushopt((char *)addr + n);
		/** Set n to be (n + CACHE_LINE_SIZE) */
		n += CACHE_LINE_SIZE;
	}
}

/**
 * @}
 */
