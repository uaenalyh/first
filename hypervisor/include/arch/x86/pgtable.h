/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PGTABLE_H
#define PGTABLE_H

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the features related to the paging-structure entry and the address translation
 *        between host physical address and host virtual address.
 *
 * Following functionalities are provided in this file:
 * - Define the macros and functions to access the paging-structure entry.
 * - Define the functions to support the address translation between host physical address and host virtual address.
 * - Declare the function 'lookup_address' to look for the specified paging-structure entry.
 * - Declare the functions 'mmu_add' and 'mmu_modify_or_del' to establish, modify, or delete the mapping information.
 *
 */

#include <page.h>

/**
 * @brief Bit indicator for Present (P) Bit in a paging-structure entry.
 */
#define PAGE_PRESENT (1UL << 0U)
/**
 * @brief Bit indicator for Read/Write (R/W) Bit in a paging-structure entry.
 */
#define PAGE_RW      (1UL << 1U)
/**
 * @brief Bit indicator for User/Supervisor (U/S) Bit in a paging-structure entry.
 */
#define PAGE_USER    (1UL << 2U)
/**
 * @brief Bit indicator for Page-level Write-through (PWT) Bit in a paging-structure entry.
 */
#define PAGE_PWT     (1UL << 3U)
/**
 * @brief Bit indicator for Page-level Cache Disable (PCD) Bit in a paging-structure entry.
 */
#define PAGE_PCD     (1UL << 4U)
/**
 * @brief Bit indicator for Page Size (PS) Bit in a paging-structure entry.
 */
#define PAGE_PS      (1UL << 7U)
/**
 * @brief Bit indicator for Execute-Disable (XD) Bit in a paging-structure entry.
 */
#define PAGE_NX      (1UL << 63U)

/**
 * @brief Mask of bits in a paging-structure entry that contribute to the determination of memory type.
 *
 * As the hypervisor will always keep the PAT bit to 0,
 * PWT bit and PCD bit are used to determine the memory type, refer to Intel SDM Section 4.9.2 for detailed encodings.
 */
#define PAGE_CACHE_MASK (PAGE_PCD | PAGE_PWT)
/**
 * @brief Setting of PWT and PCD bits in a paging-structure entry to select the memory type - Write-back (WB).
 *
 * Both PWT bit and PCD bit shall be cleared in order to select the memory type - Write-back (WB).
 */
#define PAGE_CACHE_WB   0UL
/**
 * @brief Setting of PWT and PCD bits in a paging-structure entry to select the memory type - Strong Uncacheable (UC).
 *
 * Both PWT bit and PCD bit shall be set to 1 in order to select the memory type - Strong Uncacheable (UC).
 */
#define PAGE_CACHE_UC   (PAGE_PCD | PAGE_PWT)

/**
 * @brief Bit indicator for Read Access Bit in an EPT paging-structure entry.
 */
#define EPT_RD (1UL << 0U)

/**
 * @brief Bit indicator for Write Access Bit in an EPT paging-structure entry.
 */
#define EPT_WR (1UL << 1U)

/**
 * @brief Bit indicator for Execute Access Bit in an EPT paging-structure entry.
 */
#define EPT_EXE (1UL << 2U)

/**
 * @brief Mask of [bit 2:0] in an EPT paging-structure entry.
 *
 * It can be used to indicate that reads, writes, and instruction fetches are both allowed from the page referenced by
 * the EPT paging-structure entry.
 * It can also be used to check whether an EPT paging-structure entry is present or not.
 * An EPT paging-structure entry is present if any of [bit 2:0] is 1; otherwise, the entry is not present.
 */
#define EPT_RWX (EPT_RD | EPT_WR | EPT_EXE)

/**
 * @brief The number of shift bits to determine the memory type in an EPT paging-structure entry.
 *
 * EPT memory type is specified in [bit 5:3] of an EPT paging-structure entry.
 * [bit 5:3] is stated as Memory Type field.
 */
#define EPT_MT_SHIFT 3U

/**
 * @brief Setting of Memory Type field in an EPT paging-structure entry to select the memory type -
 *        Strong Uncacheable (UC).
 *
 * [bit 5:3] of an EPT paging-structure entry shall be set to 0 to select the memory type - Strong Uncacheable (UC).
 */
#define EPT_UNCACHED (0UL << EPT_MT_SHIFT)

/**
 * @brief Setting of Memory Type field in an EPT paging-structure entry to select the memory type - Write-back (WB).
 *
 * [bit 5:3] of an EPT paging-structure entry shall be set to 6 to select the memory type - Write-back (WB).
 */
#define EPT_WB (6UL << EPT_MT_SHIFT)

/**
 * @brief Mask of Memory Type field in an EPT paging-structure entry.
 *
 * Memory type is specified in [bit 5:3] of an EPT paging-structure entry.
 */
#define EPT_MT_MASK (7UL << EPT_MT_SHIFT)

/**
 * @brief Bit indicator for Snoop Bit in a second-level paging-structure entry used for VT-d.
 *
 * In ACRN hypervisor, EPT is used as the second-level translation table for VT-d.
 */
#define EPT_SNOOP_CTRL (1UL << 11U)

/**
 * @brief Mask of [bit 63:52] in an EPT paging-structure entry.
 *
 * [bit 63:52] of an EPT paging-structure entry is ignored according to Intel SDM Section 28.2.2.
 */
#define EPT_PFN_HIGH_MASK 0xFFF0000000000000UL

/**
 * @brief Number of shift bits to identify the PML4E from the input address of the translation process.
 *
 * A PML4E is identified using [bit 47:39] of the input address.
 */
#define PML4E_SHIFT    39U
/**
 * @brief Number of entries contained in a PML4 table.
 */
#define PTRS_PER_PML4E 512UL
/**
 * @brief Size of the space controlled by a PML4E, which is 512-GByte.
 */
#define PML4E_SIZE     (1UL << PML4E_SHIFT)
/**
 * @brief Mask to clear low 39-bits in an input address in order to identify the corresponding PML4E.
 */
#define PML4E_MASK     (~(PML4E_SIZE - 1UL))

/**
 * @brief Number of shift bits to identify the PDPTE from the input address of the translation process.
 *
 * A PDPTE is identified using [bit 47:30] of the input address.
 */
#define PDPTE_SHIFT    30U
/**
 * @brief Number of entries contained in a page-directory-pointer table.
 */
#define PTRS_PER_PDPTE 512UL
/**
 * @brief Size of the space controlled by a PDPTE, which is 1-GByte.
 */
#define PDPTE_SIZE     (1UL << PDPTE_SHIFT)
/**
 * @brief Mask to clear low 30-bits in an input address in order to identify the corresponding PDPTE.
 */
#define PDPTE_MASK     (~(PDPTE_SIZE - 1UL))

/**
 * @brief Number of shift bits to identify the PDE from the input address of the translation process.
 *
 * A PDE is identified using [bit 47:21] of the input address.
 */
#define PDE_SHIFT    21U
/**
 * @brief Number of entries contained in a page directory.
 */
#define PTRS_PER_PDE 512UL
/**
 * @brief Size of the space controlled by a PDE, which is 2-MByte.
 */
#define PDE_SIZE     (1UL << PDE_SHIFT)
/**
 * @brief Mask to clear low 21-bits in an input address in order to identify the corresponding PDE.
 */
#define PDE_MASK     (~(PDE_SIZE - 1UL))

/**
 * @brief Number of shift bits to identify the PTE from the input address of the translation process.
 *
 * A PTE is identified using [bit 47:12] of the input address.
 */
#define PTE_SHIFT    12U
/**
 * @brief Number of entries contained in a page table.
 */
#define PTRS_PER_PTE 512UL
/**
 * @brief Size of the space controlled by a PTE, which is 4-KByte.
 */
#define PTE_SIZE     (1UL << PTE_SHIFT)

/**
 * @brief The physical-address width supported by the processor on the physical platform.
 *
 * According to SDM, MAXPHYADDR is determined by CPUID.80000008H:EAX[7:0]. It is 39 on KBL NUC.
 */
#define MAXPHYADDR 39UL

/**
 * @brief Mask to clear [bit 63:M] of a paging-structure entry, where M is an abbreviation for MAXPHYADDR.
 */
#define MAXPHYADDR_MASK ((1UL << MAXPHYADDR) - 1UL)

/**
 * @brief Mask to determine the physical address of the page-directory-pointer table referenced by a PML4E.
 *
 * According to SDM, for a PML4E that references a page-directory-pointer table,
 * physical address of 4-KByte aligned page-directory-pointer table referenced by this entry are specified in
 * [bit M-1:12], where M is an abbreviation for MAXPHYADDR.
 */
#define PML4E_PFN_MASK (MAXPHYADDR_MASK & PAGE_MASK)

/**
 * @brief Mask to determine the physical address of the page directory referenced by a PDPTE.
 *
 * According to SDM, for a PDPTE that references a page directory,
 * physical address of 4-KByte aligned page directory referenced by this entry are specified in
 * [bit M-1:12], where M is an abbreviation for MAXPHYADDR.
 */
#define PDPTE_PFN_MASK (MAXPHYADDR_MASK & PAGE_MASK)

/**
 * @brief Mask to determine the physical address of the page table referenced by a PDE.
 *
 * According to SDM, for a PDE that references a page table,
 * physical address of 4-KByte aligned page table referenced by this entry are specified in
 * [bit M-1:12], where M is an abbreviation for MAXPHYADDR.
 */
#define PDE_PFN_MASK (MAXPHYADDR_MASK & PAGE_MASK)

/**
 * @brief Mask to determine the physical address of the 1-GByte page mapped by a PDPTE.
 *
 * According to SDM, for a PDPTE that maps a 1-GByte page,
 * physical address of the 1-GByte page mapped by this entry are specified in [bit M-1:30],
 * where M is an abbreviation for MAXPHYADDR.
 */
#define PDPTE_PADDR_MASK (MAXPHYADDR_MASK & PDPTE_MASK)

/**
 * @brief Mask to determine the physical address of the 2-MByte page mapped by a PDE.
 *
 * According to SDM, for a PDE that maps a 2-MByte page,
 * physical address of the 2-MByte page mapped by this entry are specified in [bit M-1:21],
 * where M is an abbreviation for MAXPHYADDR.
 */
#define PDE_PADDR_MASK (MAXPHYADDR_MASK & PDE_MASK)

/**
 * @brief Mask to determine the properties of a PDPTE.
 *
 * According to SDM, for a PDPTE that maps a 1-GByte page,
 * properties of this entry are specified in [bit 63:59] and [bit 11:0].
 */
#define PDPTE_PROT_MASK 0xF800000000000FFFUL

/**
 * @brief Mask to determine the properties of a PDE.
 *
 * According to SDM, for a PDE that maps a 2-MByte page,
 * properties of this entry are specified in [bit 63:59] and [bit 11:0].
 */
#define PDE_PROT_MASK  0xF800000000000FFFUL

/**
 * @brief Get the host virtual address corresponding to the given host physical address.
 *
 * It is supposed to be called when hypervisor attempts to get the host virtual address corresponding to the given
 * host physical address.
 *
 * @param[in] hpa The given host physical address.
 *
 * @return The host virtual address corresponding to the given host physical address.
 *
 * @pre 0 < hpa < get_mem_range_info()->mem_top
 * @pre The host physical address and host virtual address shall be one to one mapping.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called after enable_paging() has been called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void *hpa2hva(uint64_t hpa)
{
	/** Return '(void *)hpa', which is the host virtual address corresponding to \a hpa. */
	return (void *)hpa;
}

/**
 * @brief Get the host physical address corresponding to the given host virtual address.
 *
 * It is supposed to be called when hypervisor attempts to get the host physical address corresponding to the given
 * host virtual address.
 *
 * @param[in] hva The given host virtual address.
 *
 * @return The host physical address corresponding to the given host virtual address.
 *
 * @pre 0 < hva < get_mem_range_info()->mem_top
 * @pre The host physical address and host virtual address shall be one to one mapping.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called after enable_paging() has been called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t hva2hpa(const void *hva)
{
	/** Return '(uint64_t)hva', which is the host physical address corresponding to \a hva. */
	return (uint64_t)hva;
}

/**
 * @brief Get the index of the PML4E associated with the specified input address.
 *
 * A PML4E is identified using [bit 47:39] of the input address.
 * The value of [bit 47:39] is stated as the index of this PML4E.
 *
 * It is supposed to be called internally by 'pml4e_offset'.
 *
 * @param[in] address The specified input address.
 *
 * @return The index of the PML4E associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pml4e_index(uint64_t address)
{
	/** Return '(address >> PML4E_SHIFT) & (PTRS_PER_PML4E - 1UL)', which is bits 47:39 of \a address */
	return (address >> PML4E_SHIFT) & (PTRS_PER_PML4E - 1UL);
}

/**
 * @brief Get the index of the PDPTE associated with the specified input address.
 *
 * A PDPTE is identified using [bit 47:30] of the input address.
 * [bit 47:39] is used to identify the page-directory-pointer table where the PDPTE is located.
 * [bit 38:30] is used to determine the exact PDPTE. The value of [bit 38:30] is stated as the index of this PDPTE.
 *
 * It is supposed to be called internally by 'pdpte_offset', 'modify_or_del_pdpte', and 'add_pdpte'.
 *
 * @param[in] address The specified input address.
 *
 * @return The index of the PDPTE associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pdpte_index(uint64_t address)
{
	/** Return '(address >> PDPTE_SHIFT) & (PTRS_PER_PDPTE - 1UL)', which is [bit 38:30] of \a address */
	return (address >> PDPTE_SHIFT) & (PTRS_PER_PDPTE - 1UL);
}

/**
 * @brief Get the index of the PDE associated with the specified input address.
 *
 * A PDE is identified using [bit 47:21] of the input address.
 * [bit 47:30] is used to identify the page directory where the PDE is located.
 * [bit 29:21] is used to determine the exact PDE. The value of [bit 29:21] is stated as the index of this PDE.
 *
 * It is supposed to be called internally by 'pde_offset', 'modify_or_del_pde', and 'add_pde'.
 *
 * @param[in] address The specified input address.
 *
 * @return The index of the PDE associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pde_index(uint64_t address)
{
	/** Return '(address >> PDE_SHIFT) & (PTRS_PER_PDE - 1UL)', which is [bit 29:21] of \a address */
	return (address >> PDE_SHIFT) & (PTRS_PER_PDE - 1UL);
}

/**
 * @brief Get the index of the PTE associated with the specified input address.
 *
 * A PTE is identified using [bit 47:12] of the input address.
 * [bit 47:21] is used to identify the page table where the PTE is located.
 * [bit 20:12] is used to determine the exact PTE. The value of [bit 20:12] is stated as the index of this PTE.
 *
 * It is supposed to be called internally by 'pte_offset', 'modify_or_del_pte', and 'add_pte'.
 *
 * @param[in] address The specified input address.
 *
 * @return The index of the PTE associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pte_index(uint64_t address)
{
	/** Return '(address >> PTE_SHIFT) & (PTRS_PER_PTE - 1UL)', which is [bit 20:12] of \a address */
	return (address >> PTE_SHIFT) & (PTRS_PER_PTE - 1UL);
}

/**
 * @brief Get the page-directory-pointer table referenced by the specified PML4E.
 *
 * Physical address of the page-directory-pointer table referenced by the specified PML4E is identified
 * using [bit M-1:12] of the specified PML4E, while M is an abbreviation for MAXPHYADDR.
 *
 * It is supposed to be called internally by 'pdpte_offset', 'modify_or_del_pdpte', and 'add_pdpte'.
 *
 * @param[in] pml4e The specified PML4E that references a page-directory-pointer table.
 *
 * @return A pointer to the page-directory-pointer table referenced by the specified PML4E.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pml4e_page_vaddr(uint64_t pml4e)
{
	/** Return the return value of 'hpa2hva(pml4e & PML4E_PFN_MASK)', which points to
	 *  the page-directory-pointer table referenced by \a pml4e. */
	return hpa2hva(pml4e & PML4E_PFN_MASK);
}

/**
 * @brief Get the page directory referenced by the specified PDPTE.
 *
 * Physical address of the page directory referenced by the specified PDPTE is identified
 * using [bit M-1:12] of the specified PDPTE, while M is an abbreviation for MAXPHYADDR.
 *
 * It is supposed to be called internally by 'pde_offset', 'modify_or_del_pde', and 'add_pde'.
 *
 * @param[in] pdpte The specified PDPTE that references a page directory.
 *
 * @return A pointer to the page directory referenced by the specified PDPTE.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pdpte_page_vaddr(uint64_t pdpte)
{
	/** Return the return value of 'hpa2hva(pdpte & PDPTE_PFN_MASK)', which points to
	 *  the page directory referenced by \a pdpte. */
	return hpa2hva(pdpte & PDPTE_PFN_MASK);
}

/**
 * @brief Get the page table referenced by the specified PDE.
 *
 * Physical address of the page table referenced by the specified PDE is identified
 * using [bit M-1:12] of the specified PDE, while M is an abbreviation for MAXPHYADDR.
 *
 * It is supposed to be called internally by 'pte_offset', 'modify_or_del_pte', and 'add_pte'.
 *
 * @param[in] pde The specified PDE that references a page table.
 *
 * @return A pointer to the page table referenced by the specified PDE.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pde_page_vaddr(uint64_t pde)
{
	/** Return the return value of 'hpa2hva(pde & PDE_PFN_MASK)', which points to
	 *  the page table referenced by \a pde. */
	return hpa2hva(pde & PDE_PFN_MASK);
}

/**
 * @brief Locate the PML4E (in the specified PML4 table) associated with the specified input address.
 *
 * In the specified PML4 table, the PML4E associated with the specified input address could be located
 * using [bit 47:39] of the input address.
 *
 * It is supposed to be called internally by 'mmu_modify_or_del', 'mmu_add', and 'lookup_address'.
 * It is also supposed to be called by 'walk_ept_table' from 'vp-base.guest_mem' module.
 *
 * @param[in] pml4_page A pointer to the specified PML4 table.
 * @param[in] addr The specified input address.
 *
 * @return A pointer to the PML4E (locating in the specified PML4 table) associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pml4e_offset(uint64_t *pml4_page, uint64_t addr)
{
	/** Return 'pml4_page + pml4e_index(addr)', which points to
	 *  the PML4E (locating in \a pml4_page) associated with \a addr. */
	return pml4_page + pml4e_index(addr);
}

/**
 * @brief Locate the PDPTE associated with the specified input address.
 *
 * The PDPTE associated with the specified input address could be located with the following information:
 * 1. The page-directory-pointer table (where the PDPTE is located) referenced by the specified PML4E.
 * 2. [bit 38:30] of the input address, which determines the exact PDPTE.
 *
 * It is supposed to be called internally by 'lookup_address'.
 * It is also supposed to be called by 'walk_ept_table' from 'vp-base.guest_mem' module.
 *
 * @param[in] pml4e The specified PML4E.
 * @param[in] addr The specified input address.
 *
 * @return A pointer to the PDPTE (locating in the page-directory-pointer table referenced by the specified PML4E)
 *         associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pdpte_offset(const uint64_t *pml4e, uint64_t addr)
{
	/** Return 'pml4e_page_vaddr(*pml4e) + pdpte_index(addr)', which points to
	 *  the PDPTE (locating in the page-directory-pointer table referenced by \a pml4e)
	 *  associated with \a addr. */
	return pml4e_page_vaddr(*pml4e) + pdpte_index(addr);
}

/**
 * @brief Locate the PDE associated with the specified input address.
 *
 * The PDE associated with the specified input address could be located with the following information:
 * 1. The page directory (where the PDE is located) referenced by the specified PDPTE.
 * 2. [bit 29:21] of the input address, which determines the exact PDE.
 *
 * It is supposed to be called internally by 'lookup_address'.
 * It is also supposed to be called by 'walk_ept_table' from 'vp-base.guest_mem' module.
 *
 * @param[in] pdpte The specified PDPTE.
 * @param[in] addr The specified input address.
 *
 * @return A pointer to the PDE (locating in the page directory referenced by the specified PDPTE)
 *         associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pde_offset(const uint64_t *pdpte, uint64_t addr)
{
	/** Return 'pdpte_page_vaddr(*pdpte) + pde_index(addr)', which points to
	 *  the PDE (locating in the page directory referenced by \a pdpte)
	 *  associated with \a addr. */
	return pdpte_page_vaddr(*pdpte) + pde_index(addr);
}

/**
 * @brief Locate the PTE associated with the specified input address.
 *
 * The PTE associated with the specified input address could be located with the following information:
 * 1. The page table (where the PTE is located) referenced by the specified PDE.
 * 2. [bit 20:12] of the input address, which determines the exact PTE.
 *
 * It is supposed to be called internally by 'lookup_address'.
 * It is also supposed to be called by 'walk_ept_table' from 'vp-base.guest_mem' module.
 *
 * @param[in] pde The specified PDE.
 * @param[in] addr The specified input address.
 *
 * @return A pointer to the PTE (locating in the page table referenced by the specified PDE)
 *         associated with the specified input address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t *pte_offset(const uint64_t *pde, uint64_t addr)
{
	/** Return 'pde_page_vaddr(*pde) + pte_index(addr)', which points to
	 *  the PTE (locating in the page table referenced by \a pde)
	 *  associated with \a addr. */
	return pde_page_vaddr(*pde) + pte_index(addr);
}

/**
 * @brief Set up the specified paging-structure entry based on the given information.
 *
 * The paging-structure entry can be a PML4E, or a PDPTE, or a PDE, or a PTE.
 *
 * It is supposed to be called when hypervisor attempts to set up a paging-structure entry.
 *
 * @param[out] ptep A pointer to the specified paging-structure entry.
 * @param[in] pte The specified content to be set to the specified paging-structure entry.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre ptep != NULL
 * @pre mem_ops != NULL
 * @pre mem_ops->clflush_pagewalk != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void set_pgentry(uint64_t *ptep, uint64_t pte, const struct memory_ops *mem_ops)
{
	/** Set the content in the paging-structure entry pointed by \a ptep to \a pte */
	*ptep = pte;
	/** Call 'mem_ops->clflush_pagewalk' with the following parameters, in order to flush the cache line
	 *  that contains the paging-structure entry pointed by \a ptep (when it is an EPT paging-structure entry).
	 *  - ptep
	 */
	mem_ops->clflush_pagewalk(ptep);
}

/**
 * @brief Check whether the PS flag of the specified PDE is 1 or not.
 *
 * If the PS flag of the specified PDE is 1, it indicates that the specified PDE maps a 2-MByte page.
 * If the PS flag of the specified PDE is 0, it indicates that the specified PDE references a page table.
 *
 * It is supposed to be called when hypervisor attempts to check if the specified PDE maps a 2-MByte page or
 * references a page table.
 *
 * @param[in] pde The specified PDE.
 *
 * @return A value indicating whether the PS flag of the specified PDE is 1 or not.
 *
 * @retval non-zero The PS flag of the specified PDE is 1.
 * @retval 0 The PS flag of the specified PDE is 0.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pde_large(uint64_t pde)
{
	/** Return 'pde & PAGE_PS', which indicates whether the PS flag of \a pde is 1 or not. */
	return pde & PAGE_PS;
}

/**
 * @brief Check whether the PS flag of the specified PDPTE is 1 or not.
 *
 * If the PS flag of the specified PDPTE is 1, it indicates that the specified PDPTE maps a 1-GByte page.
 * If the PS flag of the specified PDPTE is 0, it indicates that the specified PDPTE references a page directory.
 *
 * It is supposed to be called when hypervisor attempts to check if the specified PDPTE maps a 1-GByte page or
 * references a page directory.
 *
 * @param[in] pdpte The specified PDPTE.
 *
 * @return A value indicating whether the PS flag of the specified PDPTE is 1 or not.
 *
 * @retval non-zero The PS flag of the specified PDPTE is 1.
 * @retval 0 The PS flag of the specified PDPTE is 0.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t pdpte_large(uint64_t pdpte)
{
	/** Return 'pdpte & PAGE_PS', which indicates whether the PS flag of \a pdpte is 1 or not. */
	return pdpte & PAGE_PS;
}

const uint64_t *lookup_address(uint64_t *pml4_page, uint64_t addr, uint64_t *pg_size, const struct memory_ops *mem_ops);

void mmu_add(uint64_t *pml4_page, uint64_t paddr_base, uint64_t vaddr_base, uint64_t size, uint64_t prot,
	const struct memory_ops *mem_ops);
void mmu_modify_or_del(uint64_t *pml4_page, uint64_t vaddr_base, uint64_t size, uint64_t prot_set, uint64_t prot_clr,
	const struct memory_ops *mem_ops, uint32_t type);

/**
 * @}
 */

#endif /* PGTABLE_H */
