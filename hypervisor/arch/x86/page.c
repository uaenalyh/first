/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <pgtable.h>
#include <page.h>
#include <mmu.h>
#include <vtd.h>
#include <vm_configurations.h>
#include <security.h>
#include <vm.h>

/**
 * @defgroup hwmgmt_page hwmgmt.page
 * @ingroup hwmgmt
 * @brief Implementation of all external APIs to support the basic paging mechanism.
 *
 * This module implements all external APIs to support the utilization of paging structures and the address translation
 * between host physical address and host virtual address.
 *
 * Paging structures are used to translate an input address to an output address.
 * There are two types of paging structures supported in ACRN hypervisor:
 * 1. Paging structures of hypervisor are used to translate host virtual address to host physical address.
 * 'hwmgmt.mmu' module utilizes these paging structures to implement hypervisor's MMU (Memory Management Unit).
 * 2. Paging structures of each VM's EPT are used to translate guest physical address to host physical address.
 * 'vp-base.guest_mem' module utilizes these paging structures to support the extended page-table mechanism for
 * each VM.
 *
 * This module provides the external data structures, macros, global variables, along with the following
 * external functions to utilize the paging structures:
 * - 'mmu_add' could be invoked to establish mappings on the paging structures.
 * - 'mmu_modify_or_del' could be invoked to modify or delete the mappings established by the specified
 * paging-structure entries.
 * - 'lookup_address' could be invoked to look for the mapping information.
 * - 'set_pgentry' could be invoked to set up a paging-structure entry.
 * - 'init_ept_mem_ops' could be invoked to populate the information to be used for each VM's EPT operations.
 *
 * Two additional external functions are also provided in this module to support the address translation between
 * host physical address and host virtual address:
 * - 'hpa2hva' could be invoked to translate host physical address to host virtual address.
 * - 'hva2hpa' could be invoked to translate host virtual address to host physical address.
 *
 * Usage:
 * - 'hwmgmt.vtd' module, 'hwmgmt.cpu' module, 'hwmgmt.apic' module, and 'vp-base.vboot' module
 * depend on this module to do the address translation between host physical address and host virtual address.
 * - 'hwmgmt.mmu' module depends on this module to establish, modify, or delete the mappings between
 * host virtual address and host physical address. It also depends on this module to set up a paging-structure entry.
 * - 'vp-base.guest_mem' module depends on this module to establish, modify, or delete the mappings between
 * guest physical address and host physical address. It also depends on this module to look for the
 * paging-structure entry that contains the mapping information for the specified guest physical address, and to
 * do the address translation between host physical address and host virtual address.
 * - 'vp-base.vm' module depends on this module to populate the information to be used for each VM's EPT operations.
 *
 * Dependency:
 * - This module depends on 'debug' module to log some information in debug phase.
 * - This module depends on 'lib.util' module to set the contents in the specified memory region to all 0s.
 * - This module depends on 'hwmgmt.vtd' module to flush cache lines that contain the specified memory region.
 * - This module depends on 'hwmgmt.mmu' module to sanitize the paging-structure entries.
 * - This module depends on 'hwmgmt.security' module to determine whether the physical platform is vulnerable to the
 * page size change MCE issue or not.
 *
 * @{
 */

/**
 * @file
 * @brief This file provides the information to be used for paging operations.
 *
 * This file provides the information to be used for hypervisor's MMU operations and for each VM's EPT operations,
 * including the information of paging structures, the callback functions to access these paging structures, the
 * callback function to flush caches, the flag to indicate whether the large pages (1-GByte or 2-MByte) are allowed
 * to be used, and the callback function to tweak and recover execute permission.
 *
 * The information to be used for hypervisor's MMU operations is provided with a global variable 'ppt_mem_ops'.
 * 'hwmgmt.mmu' module could use this variable to access these information.
 *
 * The information to be used for each VM's EPT operations is provided with an external function 'init_ept_mem_ops'.
 * This function could be invoked to populate these information into each VM's dedicated data structure.
 *
 * Following helper functions and variables are defined to implement 'ppt_mem_ops':
 * ppt_pml4_pages, ppt_pdpt_pages, ppt_pd_pages, ppt_pages_info, ppt_get_pml4_page, ppt_get_pdpt_page, ppt_get_pd_page,
 * ppt_get_default_access_right, ppt_pgentry_present, and ppt_clflush_pagewalk.
 *
 * Following helper functions and variables are defined to implement 'init_ept_mem_ops':
 * uos_nworld_pml4_pages, uos_nworld_pdpt_pages, uos_nworld_pd_pages, uos_nworld_pt_pages, ept_pages_info,
 * ept_get_pml4_page, ept_get_pdpt_page, ept_get_pd_page, ept_get_pt_page, ept_get_default_access_right,
 * ept_pgentry_present, ept_clflush_pagewalk, ept_tweak_exe_right, and ept_recover_exe_right.
 *
 * Following helper functions are defined for those cases when no operation is to be conducted:
 * nop_tweak_exe_right and nop_recover_exe_right.
 *
 */

/**
 * @brief An array that contains all PML4 tables to be used by the hypervisor.
 */
static struct page ppt_pml4_pages[PML4_PAGE_NUM];
/**
 * @brief An array that contains all page-directory-pointer tables to be used by the hypervisor.
 */
static struct page ppt_pdpt_pages[PDPT_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];
/**
 * @brief An array that contains all page directories to be used by the hypervisor.
 */
static struct page ppt_pd_pages[PD_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];

/**
 * @brief An array that contains all paging structures to be used by the hypervisor.
 */
static union pgtable_pages_info ppt_pages_info = {
	.ppt = {
		.pml4_base = ppt_pml4_pages,
		.pdpt_base = ppt_pdpt_pages,
		.pd_base = ppt_pd_pages,
	}
};

/**
 * @brief Get the default access right of the paging-structure entry to be used by the hypervisor.
 *
 * It is supposed to be called when hypervisor attempts to set up a paging-structure entry to be used by the hypervisor
 * with the default access right.
 *
 * @return The default access right of the paging-structure entry to be used by the hypervisor.
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
static inline uint64_t ppt_get_default_access_right(void)
{
	/** Return '(PAGE_PRESENT | PAGE_RW | PAGE_USER)', which is the default access right to be set to
	 *  a paging-structure entry used by the hypervisor. */
	return (PAGE_PRESENT | PAGE_RW | PAGE_USER);
}

/**
 * @brief Callback function to flush the cache line that contains the paging-structure entry used by the hypervisor.
 *
 * There is no operation to be conducted in this function.
 *
 * The callback function is designed to enable cache flushes upon changes to DMAR tables.
 * This does not apply to the paging structures used by the hypervisor.
 *
 * @param[in] entry The specified paging-structure entry.
 *                  Though this argument is not used in this function, it is kept here since this function would
 *                  be used as a callback function and the other instances with same type need this argument.
 *
 * @return None
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
static inline void ppt_clflush_pagewalk(const void *entry __attribute__((unused)))
{
}

/**
 * @brief Check whether the specified paging-structure entry used by the hypervisor is present or not.
 *
 * According to SDM, a paging-structure entry is present if its Present Bit (Bit 0) is 1;
 * otherwise, the entry is not present.
 *
 * It is supposed to be called when hypervisor checks the presence of a paging-structure entry used by the hypervisor.
 *
 * @param[in] pte The specified paging-structure entry used by the hypervisor.
 *
 * @return A value indicating whether the specified paging-structure entry is present or not.
 *
 * @retval non-zero The specified paging-structure entry is present.
 * @retval 0 The specified paging-structure entry is not present.
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
static inline uint64_t ppt_pgentry_present(uint64_t pte)
{
	/** Return 'pte & PAGE_PRESENT', which indicates whether \a pte is present or not. */
	return pte & PAGE_PRESENT;
}

/**
 * @brief Get the specified PML4 table to be used by the hypervisor.
 *
 * It is supposed to be called when hypervisor sets up the PML4 table used by the hypervisor.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used by the hypervisor.
 *
 * @return A pointer to the specified PML4 table to be used by the hypervisor.
 *
 * @pre info != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline struct page *ppt_get_pml4_page(const union pgtable_pages_info *info)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pml4_page representing a pointer to the PML4 table to be used by the hypervisor,
	 *  initialized as 'info->ppt.pml4_base'. */
	struct page *pml4_page = info->ppt.pml4_base;

	/** Call memset with the following parameters, in order to set the contents stored in 'pml4_page' to all 0s,
	 *  and discard its return value.
	 *  - pml4_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	/** Return 'pml4_page' */
	return pml4_page;
}

/**
 * @brief Get the specified page-directory-pointer table to be used by the hypervisor.
 *
 * This page-directory-pointer table could be used to establish the mapping for the specified host virtual address.
 *
 * It is supposed to be called when hypervisor sets up the page-directory-pointer table to be used by the hypervisor.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used by the hypervisor.
 * @param[in] gpa The specified host virtual address that needs to be translated.
 *
 * @return A pointer to the specified page-directory-pointer table to be used to establish the mapping for the specified
 *         host virtual address.
 *
 * @pre info != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline struct page *ppt_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pdpt_page representing a pointer to the page-directory-pointer table to be used by the hypervisor
	 *  and to establish the mapping for \a gpa, initialized as 'info->ppt.pdpt_base + (gpa >> PML4E_SHIFT)'. */
	struct page *pdpt_page = info->ppt.pdpt_base + (gpa >> PML4E_SHIFT);

	/** Call memset with the following parameters, in order to set the contents stored in 'pdpt_page' to all 0s,
	 *  and discard its return value.
	 *  - pdpt_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	/** Return 'pdpt_page' */
	return pdpt_page;
}

/**
 * @brief Get the specified page directory to be used by the hypervisor.
 *
 * This page directory could be used to establish the mapping for the specified host virtual address.
 *
 * It is supposed to be called when hypervisor sets up the page directory to be used by the hypervisor.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used by the hypervisor.
 * @param[in] gpa The specified host virtual address that needs to be translated.
 *
 * @return A pointer to the specified page directory to be used to establish the mapping for the specified
 *         host virtual address.
 *
 * @pre info != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline struct page *ppt_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pd_page representing a pointer to the page directory to be used by the hypervisor
	 *  and to establish the mapping for \a gpa, initialized as 'info->ppt.pd_base + (gpa >> PDPTE_SHIFT)'. */
	struct page *pd_page = info->ppt.pd_base + (gpa >> PDPTE_SHIFT);

	/** Call memset with the following parameters, in order to set the contents stored in 'pd_page' to all 0s,
	 *  and discard its return value.
	 *  - pd_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pd_page, 0U, PAGE_SIZE);
	/** Return 'pd_page' */
	return pd_page;
}

/**
 * @brief Callback function to tweak the execute permission when MCE mitigation is not needed.
 *
 * There is no operation to be conducted in this function.
 *
 * The callback function is designed to mitigate the page size change MCE issue.
 * It only applies to EPT paging-structure entries when the physical platform that is vulnerable to
 * the page size change MCE issue.
 * For all the other cases, there is no operation needed to be conducted.
 *
 * @param[in] prot A pointer to the specified property of a paging-structure entry.
 *                 Though this argument is not used in this function, it is kept here since this function would
 *                 be used as a callback function and other instances with same type need this argument.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void nop_tweak_exe_right(uint64_t *prot __attribute__((unused)))
{
}

/**
 * @brief Callback function to recover the execute permission when MCE mitigation is not needed.
 *
 * There is no operation to be conducted in this function.
 *
 * The callback function is designed to mitigate the page size change MCE issue.
 * It only applies to EPT paging-structure entries when the physical platform that is vulnerable to
 * the page size change MCE issue.
 * For all the other cases, there is no operation needed to be conducted.
 *
 * @param[in] prot A pointer to the specified property of a paging-structure entry.
 *                 Though this argument is not used in this function, it is kept here since this function would
 *                 be used as a callback function and other instances with same type need this argument.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void nop_recover_exe_right(uint64_t *prot __attribute__((unused)))
{
}

/**
 * @brief A global variable providing the information to be used for hypervisor's MMU operations.
 */
const struct memory_ops ppt_mem_ops = {
	.info = &ppt_pages_info,
	.large_page_enabled = true,
	.get_default_access_right = ppt_get_default_access_right,
	.pgentry_present = ppt_pgentry_present,
	.get_pml4_page = ppt_get_pml4_page,
	.get_pdpt_page = ppt_get_pdpt_page,
	.get_pd_page = ppt_get_pd_page,
	.clflush_pagewalk = ppt_clflush_pagewalk,
	.tweak_exe_right = nop_tweak_exe_right,
	.recover_exe_right = nop_recover_exe_right,
};

/**
 * @brief An array that contains all PML4 tables to be used for all VMs' EPT.
 *
 * uos_nworld_pml4_pages[vm_id] is to be used for the VM whose identifier is vm_id.
 */
static struct page uos_nworld_pml4_pages[CONFIG_MAX_VM_NUM][PML4_PAGE_NUM];
/**
 * @brief An array that contains all page-directory-pointer tables to be used for all VMs' EPT.
 *
 * uos_nworld_pdpt_pages[vm_id] is to be used for the VM whose identifier is vm_id.
 */
static struct page uos_nworld_pdpt_pages[CONFIG_MAX_VM_NUM][PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
/**
 * @brief An array that contains all page directories to be used for all VMs' EPT.
 *
 * uos_nworld_pd_pages[vm_id] is to be used for the VM whose identifier is vm_id.
 */
static struct page uos_nworld_pd_pages[CONFIG_MAX_VM_NUM][PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
/**
 * @brief An array that contains all page tables to be used for all VMs' EPT.
 *
 * uos_nworld_pt_pages[vm_id] is to be used for the VM whose identifier is vm_id.
 */
static struct page uos_nworld_pt_pages[CONFIG_MAX_VM_NUM][PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];

/**
 * @brief An array that contains all paging structures to be used for all VMs' EPT.
 *
 * ept_pages_info[vm_id] is to be used for the VM whose identifier is vm_id.
 */
static union pgtable_pages_info ept_pages_info[CONFIG_MAX_VM_NUM];

/**
 * @brief Get the default access right of an EPT paging-structure entry.
 *
 * It is supposed to be called when hypervisor attempts to set up an EPT paging-structure entry
 * with the default access right.
 *
 * @return The default access right of an EPT paging-structure entry.
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
static inline uint64_t ept_get_default_access_right(void)
{
	/** Return EPT_RWX, which is the default access right to be set to an EPT paging-structure entry. */
	return EPT_RWX;
}

/**
 * @brief Check whether the specified EPT paging-structure entry is present or not.
 *
 * According to SDM, an EPT paging-structure entry is present if any of bits 2:0 is 1;
 * otherwise, the entry is not present.
 *
 * It is supposed to be called when hypervisor checks the presence of an EPT paging-structure entry.
 *
 * @param[in] pte The specified EPT paging-structure entry to be checked.
 *
 * @return A value indicating whether the specified paging-structure entry is present or not.
 *
 * @retval non-zero The specified paging-structure entry is present.
 * @retval 0 The specified paging-structure entry is not present.
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
static inline uint64_t ept_pgentry_present(uint64_t pte)
{
	/** Return 'pte & EPT_RWX', which indicates whether \a pte is present or not. */
	return pte & EPT_RWX;
}

/**
 * @brief Flush the cache line that contains the specified EPT paging-structure entry.
 *
 * The cache of those EPT paging-structure entries needs to be flushed because they are shared between EPT and VT-d.
 * As page-walk coherency is not supported on the VT-d remapping hardware in current physical platform, the cache
 * needs to be flushed to let the modification on those paging structures be visible to VT-d remapping hardware.
 *
 * It is supposed to be called when the paging structures used for EPT is modified.
 *
 * @param[in] entry A pointer to the specified EPT paging-structure entry.
 *
 * @return None
 *
 * @pre entry != NULL
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
static inline void ept_clflush_pagewalk(const void *entry)
{
	/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
	 *  the EPT paging-structure entry pointed by \a entry.
	 *  - entry
	 *  - sizeof(uint64_t)
	 */
	iommu_flush_cache(entry, sizeof(uint64_t));
}

/**
 * @brief Get the specified PML4 table to be used for the specified EPT.
 *
 * It is supposed to be called when hypervisor sets up the PML4 table for the specified EPT.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used for the specified EPT.
 *
 * @return A pointer to the PML4 table to be used for the specified EPT.
 *
 * @pre info != NULL
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
static inline struct page *ept_get_pml4_page(const union pgtable_pages_info *info)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pml4_page representing a pointer to the PML4 table to be used for the specified EPT,
	 *  initialized as 'info->ept.nworld_pml4_base'. */
	struct page *pml4_page = info->ept.nworld_pml4_base;

	/** Call memset with the following parameters, in order to set the contents stored in 'pml4_page' to all 0s,
	 *  and discard its return value.
	 *  - pml4_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	/** Return 'pml4_page' */
	return pml4_page;
}

/**
 * @brief Get the specified page-directory-pointer table to be used for the specified EPT.
 *
 * This page-directory-pointer table could be used to establish the mapping for the specified guest physical address.
 *
 * It is supposed to be called when hypervisor sets up the page-directory-pointer table for the specified EPT and
 * the specified guest physical address.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used for the specified EPT.
 * @param[in] gpa The specified guest physical address that needs to be translated.
 *
 * @return A pointer to the EPT page-directory-pointer table to be used to establish the mapping for the specified
 *         guest physical address.
 *
 * @pre info != NULL
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
static inline struct page *ept_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pdpt_page representing a pointer to the page-directory-pointer table to be used for the specified EPT and
	 * to establish the mapping for \a gpa, initialized as 'info->ept.nworld_pdpt_base + (gpa >> PML4E_SHIFT)'. */
	struct page *pdpt_page = info->ept.nworld_pdpt_base + (gpa >> PML4E_SHIFT);

	/** Call memset with the following parameters, in order to set the contents stored in 'pdpt_page' to all 0s,
	 *  and discard its return value.
	 *  - pdpt_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	/** Return 'pdpt_page' */
	return pdpt_page;
}

/**
 * @brief Get the specified page directory to be used for the specified EPT.
 *
 * This page directory could be used to establish the mapping for the specified guest physical address.
 *
 * It is supposed to be called when hypervisor sets up the page directory for the specified EPT and
 * the specified guest physical address.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used for the specified EPT.
 * @param[in] gpa The specified guest physical address that needs to be translated.
 *
 * @return A pointer to the page directory to be used to establish the mapping for the specified guest physical address.
 *
 * @pre info != NULL
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
static inline struct page *ept_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pd_page representing a pointer to the page directory to be used for the specified EPT and
	 *  to establish the mapping for \a gpa, initialized as 'info->ept.nworld_pd_base + (gpa >> PDPTE_SHIFT)'. */
	struct page *pd_page = info->ept.nworld_pd_base + (gpa >> PDPTE_SHIFT);

	/** Call memset with the following parameters, in order to set the contents stored in 'pd_page' to all 0s,
	 *  and discard its return value.
	 *  - pd_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pd_page, 0U, PAGE_SIZE);
	/** Return 'pd_page' */
	return pd_page;
}

/**
 * @brief Get the specified page table to be used for the specified EPT.
 *
 * This page table could be used to establish the mapping for the specified guest physical address.
 *
 * It is supposed to be called when hypervisor sets up the page table for the specified EPT and
 * the specified guest physical address.
 *
 * @param[inout] info A pointer to the data structure that contains the information of the paging structures
 *                    used for the specified EPT.
 * @param[in] gpa The specified guest physical address that needs to be translated.
 *
 * @return A pointer to the page table to be used to establish the mapping for the specified guest physical address.
 *
 * @pre info != NULL
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
static inline struct page *ept_get_pt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	/** Declare the following local variables of type 'struct page *'.
	 *  - pt_page representing a pointer to the page table to be used for the specified EPT and
	 *  to establish the mapping for \a gpa, initialized as 'info->ept.nworld_pt_base + (gpa >> PDE_SHIFT)'. */
	struct page *pt_page = info->ept.nworld_pt_base + (gpa >> PDE_SHIFT);

	/** Call memset with the following parameters, in order to set the contents stored in 'pt_page' to all 0s,
	 *  and discard its return value.
	 *  - pt_page
	 *  - 0
	 *  - PAGE_SIZE
	 */
	(void)memset(pt_page, 0U, PAGE_SIZE);
	/** Return 'pt_page' */
	return pt_page;
}

/* The function is used to disable execute right for (2MB / 1GB)large pages in EPT */
/**
 * @brief Tweak the execute permission in an EPT paging-structure entry.
 *
 * This function clears the execute access control of an EPT paging-structure entry.
 * When the execute access control is 0, it indicates that instruction fetches are not allowed from the memory region
 * controlled by this entry.
 *
 * The callback function is designed to mitigate the page size change MCE issue.
 * It only applies to EPT paging-structure entries when the physical platform that is vulnerable to
 * the page size change MCE issue.
 * Execute access control is cleared for EPT large pages (1-GByte or 2-MByte) by default.
 * If an instruction fetch from the guest software triggers an EPT violation VM exit, hypervisor would
 * split the large page into 4-KByte pages and set the execute access control to 1 in the PTEs for those 4-KByte pages.
 *
 * It is supposed to be called when hypervisor attempts to disallow the execute access from the memory region
 * controlled by an EPT paging-structure entry.
 *
 * @param[inout] prot A pointer to the specified property of an EPT paging-structure entry.
 *
 * @return None
 *
 * @pre prot != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void ept_tweak_exe_right(uint64_t *prot)
{
	/** Clear Execute Access Bit (Bit 2) of the value pointed to by \a prot */
	*prot &= ~EPT_EXE;
}

/* The function is used to recover the execute right when large pages are breaking into 4KB pages
 * Hypervisor doesn't control execute right for guest memory, recovers execute right by default.
 */
/**
 * @brief Recover the execute permission in an EPT paging-structure entry.
 *
 * This function sets the execute access control of an EPT paging-structure entry to 1.
 * When the execute access control is 1, it indicates that instruction fetches are allowed from the memory region
 * controlled by this entry.
 *
 * The callback function is designed to mitigate the page size change MCE issue.
 * It only applies to EPT paging-structure entries when the physical platform that is vulnerable to
 * the page size change MCE issue.
 * Execute access control is cleared for EPT large pages (1-GByte or 2-MByte) by default.
 * If an instruction fetch from the guest software triggers an EPT violation VM exit, hypervisor would
 * split the large page into 4-KByte pages and set the execute access control to 1 in the PTEs for those 4-KByte pages.
 *
 * It is supposed to be called when hypervisor attempts to allow the execute access from the memory region
 * controlled by an EPT paging-structure entry.
 *
 * @param[inout] prot A pointer to the specified property of an EPT paging-structure entry.
 *
 * @return None
 *
 * @pre prot != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void ept_recover_exe_right(uint64_t *prot)
{
	/** Set Execute Access Bit (Bit 2) of the value pointed to by \a prot to 1 */
	*prot |= EPT_EXE;
}

/**
 * @brief Populate the specified data structure with the information to be used for the specified VM's EPT operations.
 *
 * It is supposed to be called only by 'create_vm' from 'vp-base.vm' module.
 *
 * @param[out] mem_ops A pointer to the data structure which is to be populated.
 * @param[in] vm_id The identifier of the specified VM whose paging structures information would be used.
 * @param[in] enforce_4k_ipage A boolean value indicating whether the hypervisor wants to use only 4K page mappings
 *                             in the EPT of the VM. It is true if the hypervisor wants to use only 4K page mappings
 *                             in the EPT of the VM; otherwise, it is false.
 *
 * @return None
 *
 * @pre mem_ops != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a mem_ops is different among parallel invocation
 */
void init_ept_mem_ops(struct memory_ops *mem_ops, uint16_t vm_id, bool enforce_4k_ipage)
{
	/** Set 'ept_pages_info[vm_id].ept.nworld_pml4_base' to 'uos_nworld_pml4_pages[vm_id]' */
	ept_pages_info[vm_id].ept.nworld_pml4_base = uos_nworld_pml4_pages[vm_id];
	/** Set 'ept_pages_info[vm_id].ept.nworld_pdpt_base' to 'uos_nworld_pdpt_pages[vm_id]' */
	ept_pages_info[vm_id].ept.nworld_pdpt_base = uos_nworld_pdpt_pages[vm_id];
	/** Set 'ept_pages_info[vm_id].ept.nworld_pd_base' to 'uos_nworld_pd_pages[vm_id]' */
	ept_pages_info[vm_id].ept.nworld_pd_base = uos_nworld_pd_pages[vm_id];
	/** Set 'ept_pages_info[vm_id].ept.nworld_pt_base' to 'uos_nworld_pt_pages[vm_id]' */
	ept_pages_info[vm_id].ept.nworld_pt_base = uos_nworld_pt_pages[vm_id];

	/** Set 'mem_ops->info' to '&ept_pages_info[vm_id]' */
	mem_ops->info = &ept_pages_info[vm_id];
	/** Set 'mem_ops->get_default_access_right' to 'ept_get_default_access_right' */
	mem_ops->get_default_access_right = ept_get_default_access_right;
	/** Set 'mem_ops->pgentry_present' to 'ept_pgentry_present' */
	mem_ops->pgentry_present = ept_pgentry_present;
	/** Set 'mem_ops->get_pml4_page' to 'ept_get_pml4_page' */
	mem_ops->get_pml4_page = ept_get_pml4_page;
	/** Set 'mem_ops->get_pdpt_page' to 'ept_get_pdpt_page' */
	mem_ops->get_pdpt_page = ept_get_pdpt_page;
	/** Set 'mem_ops->get_pd_page' to 'ept_get_pd_page' */
	mem_ops->get_pd_page = ept_get_pd_page;
	/** Set 'mem_ops->get_pt_page' to 'ept_get_pt_page' */
	mem_ops->get_pt_page = ept_get_pt_page;
	/** Set 'mem_ops->clflush_pagewalk' to 'ept_clflush_pagewalk' */
	mem_ops->clflush_pagewalk = ept_clflush_pagewalk;
	/** Set 'mem_ops->large_page_enabled' to true */
	mem_ops->large_page_enabled = true;

	/* Mitigation for issue "Machine Check Error on Page Size Change" */
	/** If the return value of 'is_ept_force_4k_ipage()' is true,
	 *  indicating that the physical platform is vulnerable to the page size change MCE issue. */
	if (is_ept_force_4k_ipage()) {
		/** Set 'mem_ops->tweak_exe_right' to 'ept_tweak_exe_right' */
		mem_ops->tweak_exe_right = ept_tweak_exe_right;
		/** Set 'mem_ops->recover_exe_right' to 'ept_recover_exe_right' */
		mem_ops->recover_exe_right = ept_recover_exe_right;

		/** If \a enforce_4k_ipage is true */
		if (enforce_4k_ipage) {
			/** Set 'mem_ops->large_page_enabled' to false */
			mem_ops->large_page_enabled = false;
		}
	} else {
		/** Set 'mem_ops->tweak_exe_right' to 'nop_tweak_exe_right' */
		mem_ops->tweak_exe_right = nop_tweak_exe_right;
		/** Set 'mem_ops->recover_exe_right' to 'nop_recover_exe_right' */
		mem_ops->recover_exe_right = nop_recover_exe_right;
	}
}

/**
 * @}
 */
