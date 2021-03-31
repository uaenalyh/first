/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <page.h>
#include <mmu.h>
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the external APIs to establish, modify, delete, or look for the mapping information.
 *
 * This file implements the external APIs to establish, modify, delete, or look for the mapping information.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 *
 * Following external APIs are implemented to establish, modify, delete, or look for the mapping information:
 * - 'mmu_add' could be invoked to establish mappings on the paging structures.
 * - 'mmu_modify_or_del' could be invoked to modify or delete the mappings established by the specified
 * paging-structure entries.
 * - 'lookup_address' could be invoked to look for the mapping information.
 *
 * Following helper functions are defined to implement 'mmu_modify_or_del':
 * split_large_page, local_modify_or_del_pte, modify_or_del_pte, modify_or_del_pde, and modify_or_del_pdpte.
 *
 * Following helper functions are defined to implement 'mmu_add':
 * construct_pgentry, add_pte, add_pde, and add_pdpte.
 *
 */

/**
 * @brief The log level used for debug messages in this file.
 */
#define ACRN_DBG_MMU 6U

/**
 * @brief Split a large page into next level pages.
 *
 * Only the following cases are supported:
 * - Split a 1-GByte page into 512 2-MByte pages.
 * - Split a 2-MByte page into 512 4-KByte pages.
 *
 * It is supposed to be called internally by 'modify_or_del_pdpte' and 'modify_or_del_pde'.
 *
 * @param[inout] pte A pointer to the specified paging-structure entry. It points to either a PDPTE or a PDE.
 * @param[in] level The specified paging-structure level.
 * @param[in] vaddr The specified address to be translated, which is the input of the translation process.
 *                  For hypervisor's MMU, it is the host virtual address.
 *                  For each VM's EPT, it is the guest physical address.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pte != NULL
 * @pre mem_ops != NULL
 * @pre (level == IA32E_PDPT) || (level == IA32E_PD)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pte is different among parallel invocation.
 */
static void split_large_page(
	uint64_t *pte, enum page_table_level level, uint64_t vaddr, const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pbase representing a pointer to the next level paging structure (either a page directory or a page table),
	 *  not initialized. */
	uint64_t *pbase;
	/** Declare the following local variables of type uint64_t.
	 *  - paddr representing the physical address of the page mapped by the specified paging-structure entry
	 *  (either a PDPTE or a PDE), not initialized.
	 *  - paddrinc representing the size of the space controlled by the next level paging-structure entry
	 *  (either a PDE or a PTE), not initialized.
	 */
	uint64_t paddr, paddrinc;
	/** Declare the following local variables of type uint64_t.
	 *  - i representing the index to go through all entries located in the specified paging structure
	 *  (either a page directory or a page table), not initialized.
	 *  - ref_prot representing the property to be set to a paging-structure entry, not initialized.
	 */
	uint64_t i, ref_prot;

	/** Depending on the paging-structure level specified by \a level */
	switch (level) {
	/** \a level is IA32E_PDPT */
	case IA32E_PDPT:
		/** Set 'paddr' to '(*pte) & PDPTE_PADDR_MASK', which is the physical address of the 1-GByte page
		 *  mapped by \a pte */
		paddr = (*pte) & PDPTE_PADDR_MASK;
		/** Set 'paddrinc' to PDE_SIZE */
		paddrinc = PDE_SIZE;
		/** Set 'ref_prot' to '(*pte) & PDPTE_PROT_MASK', which is the properties of the PDPTE specified by
		 *  \a pte */
		ref_prot = (*pte) & PDPTE_PROT_MASK;
		/** Set 'pbase' to the return value of 'mem_ops->get_pd_page(mem_ops->info, vaddr)',
		 *  which points to the page directory to be used for \a vaddr */
		pbase = (uint64_t *)mem_ops->get_pd_page(mem_ops->info, vaddr);
		/** End of case */
		break;
	/** Otherwise */
	default: /* IA32E_PD */
		/** Set 'paddr' to '(*pte) & PDE_PADDR_MASK', which is the physical address of the 2-MByte page
		 *  mapped by \a pte */
		paddr = (*pte) & PDE_PADDR_MASK;
		/** Set 'paddrinc' to PTE_SIZE */
		paddrinc = PTE_SIZE;
		/** Set 'ref_prot' to '(*pte) & PDE_PROT_MASK', which is the properties of the PDE specified by
		 *  \a pte */
		ref_prot = (*pte) & PDE_PROT_MASK;
		/** Clear Page Size Bit (Bit 7) in 'ref_prot' */
		ref_prot &= ~PAGE_PS;
		/** Call mem_ops->recover_exe_right with the following parameters, in order to
		 *  recover the execute permission in 'ref_prot'.
		 *  - &ref_prot
		 */
		mem_ops->recover_exe_right(&ref_prot);
		/** Set 'pbase' to the return value of 'mem_ops->get_pt_page(mem_ops->info, vaddr)',
		 *  which points to the page table to be used for \a vaddr */
		pbase = (uint64_t *)mem_ops->get_pt_page(mem_ops->info, vaddr);
		/** End of case */
		break;
	}

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - paddr
	 *  - pbase
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%lx, pbase: 0x%lx\n", __func__, paddr, pbase);

	/** For each 'i' ranging from 0 to 'PTRS_PER_PTE - 1' [with a step of 1] */
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		/** Call set_pgentry with the following parameters, in order to set the content stored in the
		 *  paging-structure entry (either a PDE or a PTE) pointed by 'pbase + i' to 'paddr | ref_prot'.
		 *  - pbase + i
		 *  - paddr | ref_prot
		 *  - mem_ops
		 */
		set_pgentry(pbase + i, paddr | ref_prot, mem_ops);
		/** Increment 'paddr' by 'paddrinc' */
		paddr += paddrinc;
	}

	/** Set 'ref_prot' to the return value of 'mem_ops->get_default_access_right()' */
	ref_prot = mem_ops->get_default_access_right();
	/** Call set_pgentry with the following parameters, in order to set the content stored in the
	 *  paging-structure entry (either a PDPTE or a PDE) pointed by \a pte to 'hva2hpa((void *)pbase) | ref_prot'
	 *  so that this paging-structure entry would reference the next level paging-structure.
	 *  - pte
	 *  - hva2hpa((void *)pbase) | ref_prot
	 *  - mem_ops
	 */
	set_pgentry(pte, hva2hpa((void *)pbase) | ref_prot, mem_ops);
}

/**
 * @brief Modify or delete the mapping established by the specified paging-structure entry.
 *
 * Modification means that the properties of the specified paging-structure entry is to be updated.
 * Deletion means that the current mapping established by the specified paging-structure entry is to be removed.
 *
 * It is supposed to be called internally by 'modify_or_del_pte', 'modify_or_del_pde', and 'modify_or_del_pdpte'.
 *
 * @param[inout] pte A pointer to the specified paging-structure entry. It could point to a PDPTE, or a PDE, or a PTE.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] type The type of the requested operation on the specified paging-structure entry,
 *                 either modification or deletion.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pte != NULL
 * @pre mem_ops != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pte is different among parallel invocation.
 */
static inline void local_modify_or_del_pte(
	uint64_t *pte, uint64_t prot_set, uint64_t prot_clr, uint32_t type, const struct memory_ops *mem_ops)
{
	/** If \a type is equal to MR_MODIFY */
	if (type == MR_MODIFY) {
		/** Declare the following local variables of type uint64_t.
		 *  - new_pte representing the new content to be set to the paging-structure entry pointed by \a pte,
		 *  initialized as '*pte'. */
		uint64_t new_pte = *pte;
		/** Clear the bits specified by \a prot_clr in 'new_pte' */
		new_pte &= ~prot_clr;
		/** Set each bit specified by \a prot_set in 'new_pte' to 1 */
		new_pte |= prot_set;
		/** Call set_pgentry with the following parameters, in order to set the content stored in
		 *  the paging-structure entry pointed by \a pte to 'new_pte'.
		 *  - pte
		 *  - new_pte
		 *  - mem_ops
		 */
		set_pgentry(pte, new_pte, mem_ops);
	} else {
		/** Call sanitize_pte_entry with the following parameters, in order to remove the current mapping
		 *  established by \a pte (letting \a pte store the host physical address of a sanitized page
		 *  defined in 'hwmgmt.mmu' module).
		 *  - pte
		 *  - mem_ops
		 */
		sanitize_pte_entry(pte, mem_ops);
	}
}

/**
 * @brief Construct the specified paging-structure entry to reference next level paging structure.
 *
 * It is supposed to be called internally by 'add_pde', 'add_pdpte', and 'mmu_add'.
 *
 * @param[inout] pde A pointer to the specified paging-structure entry.
 *                   It could point to a PML4E, or a PDPTE, or a PDE.
 * @param[inout] pd_page A pointer to the specified paging structure that is to be referenced.
 *                       It could point to a page-directory-pointer table, or a page directory, or a page table.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pde != NULL
 * @pre mem_ops != NULL
 * @pre pd_page != NULL
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
static inline void construct_pgentry(uint64_t *pde, void *pd_page, uint64_t prot, const struct memory_ops *mem_ops)
{
	/** Call sanitize_pte with the following parameters, in order to sanitize the paging structure specified by
	 *  \a pd_page (letting each entry inside store the host physical address of a sanitized page
	 *  defined in 'hwmgmt.mmu' module).
	 *  - (uint64_t *)pd_page
	 *  - mem_ops
	 */
	sanitize_pte((uint64_t *)pd_page, mem_ops);

	/** Call set_pgentry with the following parameters, in order to set the content stored in
	 *  the paging-structure entry pointed by \a pde to 'hva2hpa(pd_page) | prot'.
	 *  - pde
	 *  - hva2hpa(pd_page) | prot
	 *  - mem_ops
	 */
	set_pgentry(pde, hva2hpa(pd_page) | prot, mem_ops);
}

/*
 * In PT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
/**
 * @brief Modify or delete the mappings established by the PTEs associated with the specified input address space.
 *
 * The PTEs are located in the page table referenced by the PDE pointed by \a pde.
 * The input address space is specified by [vaddr_start, vaddr_end).
 *
 * Modification means that the properties of the specified paging-structure entry is to be updated.
 * Deletion means that the current mapping established by the specified paging-structure entry is to be removed.
 *
 * It is supposed to be called internally by 'modify_or_del_pde'.
 *
 * @param[in] pde A pointer to the specified PDE that references a page table.
 * @param[in] vaddr_start The specified input address determining the start of the input address space whose mapping
 *                        information is to be updated.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space whose mapping
 *                      information is to be updated. This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 * @param[in] type The type of the requested operation on the specified paging-structure entry,
 *                 either modification or deletion.
 *
 * @return None
 *
 * @pre pde != NULL
 * @pre mem_ops != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void modify_or_del_pte(const uint64_t *pde, uint64_t vaddr_start, uint64_t vaddr_end, uint64_t prot_set,
	uint64_t prot_clr, const struct memory_ops *mem_ops, uint32_t type)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pt_page representing a pointer to the page table referenced by the PDE pointed by \a pde,
	 *  initialized as the return value of 'pde_page_vaddr(*pde)'. */
	uint64_t *pt_page = pde_page_vaddr(*pde);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be handled in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PTEs associated with the specified input address space,
	 *  initialized as the return value of 'pte_index(vaddr)'. */
	uint64_t index = pte_index(vaddr);

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - vaddr
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PTE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PTE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pte representing the pointer to the 'index'-th PTE located in the page table pointed by 'pt_page',
		 *  initialized as 'pt_page + index'. */
		uint64_t *pte = pt_page + index;

		/** If the return value of 'mem_ops->pgentry_present(*pte)' is 0,
		 *  indicating that the PTE pointed by 'pte' is not present */
		if (mem_ops->pgentry_present(*pte) == 0UL) {
			/** If following two conditions are both satisfied:
			 *  1. \a type is equal to MR_MODIFY.
			 *  2. 'vaddr' is equal to or larger than MEM_1M.
			 */
			if ((type == MR_MODIFY) && (vaddr >= MEM_1M)) {
				/** Logging the following information with a log level of 4.
				 *  - __func__
				 *  - vaddr
				 */
				pr_warn("%s, vaddr: 0x%lx pte is not present.\n", __func__, vaddr);
			}
		} else {
			/** Call local_modify_or_del_pte with the following parameters, in order to
			 *  modify or delete the mapping established by the PTE pointed by 'pte'.
			 *  - pte
			 *  - prot_set
			 *  - prot_clr
			 *  - type
			 *  - mem_ops
			 */
			local_modify_or_del_pte(pte, prot_set, prot_clr, type, mem_ops);
		}

		/** Increment 'vaddr' by PTE_SIZE */
		vaddr += PTE_SIZE;
		/** If 'vaddr' is equal to or larger than \a vaddr_end, indicating that all PTEs
		 *  associated with the specified input address space has been handled. */
		if (vaddr >= vaddr_end) {
			/** Terminate the loop */
			break;
		}
	}
}

/*
 * In PD level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
/**
 * @brief Modify or delete the mappings established by the PDEs associated with the specified input address space.
 *
 * The PDEs are located in the page directory referenced by the PDPTE pointed by \a pdpte.
 * The input address space is specified by [vaddr_start, vaddr_end).
 *
 * Modification means that the properties of the specified paging-structure entry is to be updated.
 * Deletion means that the current mapping established by the specified paging-structure entry is to be removed.
 *
 * It is supposed to be called internally by 'modify_or_del_pdpte'.
 *
 * @param[in] pdpte A pointer to the specified PDPTE that references a page directory.
 * @param[in] vaddr_start The specified input address determining the start of the input address space whose mapping
 *                        information is to be updated.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space whose mapping
 *                      information is to be updated. This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 * @param[in] type The type of the requested operation on the specified paging-structure entry,
 *                 either modification or deletion.
 *
 * @return None
 *
 * @pre pdpte != NULL
 * @pre mem_ops != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void modify_or_del_pde(const uint64_t *pdpte, uint64_t vaddr_start, uint64_t vaddr_end, uint64_t prot_set,
	uint64_t prot_clr, const struct memory_ops *mem_ops, uint32_t type)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pd_page representing a pointer to the page directory referenced by the PDPTE pointed by \a pdpte,
	 *  initialized as the return value of 'pdpte_page_vaddr(*pdpte)'. */
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be handled in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PDEs associated with the specified input address space,
	 *  initialized as the return value of 'pde_index(vaddr)'. */
	uint64_t index = pde_index(vaddr);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be handled
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - vaddr
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PDE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PDE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pde representing the pointer to the 'index'-th PDE located in the page directory pointed by
		 *  'pd_page', initialized as 'pd_page + index'. */
		uint64_t *pde = pd_page + index;
		/** Declare the following local variables of type uint64_t.
		 *  - vaddr_next representing the next input address (aligned with PDE_SIZE) to be handled,
		 *  initialized as '(vaddr & PDE_MASK) + PDE_SIZE'. */
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		/** If the return value of 'mem_ops->pgentry_present(*pde)' is 0,
		 *  indicating that the PDE pointed by 'pde' is not present */
		if (mem_ops->pgentry_present(*pde) == 0UL) {
			/** If \a type is equal to MR_MODIFY */
			if (type == MR_MODIFY) {
				/** Logging the following information with a log level of 4.
				 *  - __func__
				 *  - vaddr
				 */
				pr_warn("%s, addr: 0x%lx pde is not present.\n", __func__, vaddr);
			}
		} else {
			/** If the return value of 'pde_large(*pde)' is not 0,
			 *  indicating that the PDE pointed by 'pde' maps a 2-MByte page. */
			if (pde_large(*pde) != 0UL) {
				/** If any one of the following conditions is satisfied:
				 *  1. 'vaddr_next' is larger than \a vaddr_end.
				 *  2. The return value of 'mem_aligned_check(vaddr, PDE_SIZE)' is false, indicating
				 *  that 'vaddr' is not aligned with PDE_SIZE.
				 */
				if ((vaddr_next > vaddr_end) || (!mem_aligned_check(vaddr, PDE_SIZE))) {
					/** Call split_large_page with the following parameters, in order to
					 *  split the 2-MByte page mapped by 'pde' into 4-KByte pages.
					 *  - pde
					 *  - IA32E_PD
					 *  - vaddr
					 *  - mem_ops
					 */
					split_large_page(pde, IA32E_PD, vaddr, mem_ops);
				} else {
					/** Call local_modify_or_del_pte with the following parameters, in order to
					 *  modify or delete the mapping established by the PDE pointed by 'pde'.
					 *  - pde
					 *  - prot_set
					 *  - prot_clr
					 *  - type
					 *  - mem_ops
					 */
					local_modify_or_del_pte(pde, prot_set, prot_clr, type, mem_ops);
					/** If 'vaddr_next' is smaller than \a vaddr_end */
					if (vaddr_next < vaddr_end) {
						/** Set 'vaddr' to 'vaddr_next' */
						vaddr = vaddr_next;
						/** Continue to next iteration */
						continue;
					}
					/** Terminate the loop as all PDEs associated with the specified input
					 *  address space has been handled. */
					break; /* done */
				}
			}

			/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than \a vaddr_end;
			 *  otherwise, set 'vaddr_end_each_iter' to \a vaddr_end */
			vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
			/** Call modify_or_del_pte with the following parameters, in order to modify or delete the
			 *  mappings established by the PTEs (locating in the page table referenced by 'pde')
			 *  associated with the input address space specified by [vaddr, vaddr_end_each_iter).
			 *  - pde
			 *  - vaddr
			 *  - vaddr_end_each_iter
			 *  - prot_set
			 *  - prot_clr
			 *  - mem_ops
			 *  - type
			 */
			modify_or_del_pte(pde, vaddr, vaddr_end_each_iter, prot_set, prot_clr, mem_ops, type);
		}
		/** If 'vaddr_next' is equal to or larger than \a vaddr_end, indicating that all PDEs
		 *  associated with the specified input address space has been handled. */
		if (vaddr_next >= vaddr_end) {
			/** Terminate the loop */
			break; /* done */
		}
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/*
 * In PDPT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
/**
 * @brief Modify or delete the mappings established by the PDPTEs associated with the specified input address space.
 *
 * The PDPTEs are located in the page-directory-pointer table referenced by the PML4E pointed by \a pml4e.
 * The input address space is specified by [vaddr_start, vaddr_end).
 *
 * Modification means that the properties of the specified paging-structure entry is requested to be updated.
 * Deletion means that the current mapping established by the specified paging-structure entry is to be removed.
 *
 * It is supposed to be called internally by 'mmu_modify_or_del'.
 *
 * @param[in] pml4e A pointer to the specified PML4E that references a page-directory-pointer table.
 * @param[in] vaddr_start The specified input address determining the start of the input address space whose mapping
 *                        information is to be updated.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space whose mapping
 *                      information is to be updated. This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 * @param[in] type The type of the requested operation on the specified paging-structure entry,
 *                 either modification or deletion.
 *
 * @return None
 *
 * @pre pml4e != NULL
 * @pre mem_ops != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void modify_or_del_pdpte(const uint64_t *pml4e, uint64_t vaddr_start, uint64_t vaddr_end, uint64_t prot_set,
	uint64_t prot_clr, const struct memory_ops *mem_ops, uint32_t type)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pdpt_page representing a pointer to the page-directory-pointer table referenced by the PML4E pointed
	 *  by \a pml4e, initialized as the return value of 'pml4e_page_vaddr(*pml4e)'. */
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be handled in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PDPTEs associated with the specified input address space,
	 *  initialized as the return value of 'pdpte_index(vaddr)'. */
	uint64_t index = pdpte_index(vaddr);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be handled
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - vaddr
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PDPTE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PDPTE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pdpte representing the pointer to the 'index'-th PDPTE located in the
		 *  page-directory-pointer table pointed by 'pdpt_page', initialized as 'pdpt_page + index'. */
		uint64_t *pdpte = pdpt_page + index;
		/** Declare the following local variables of type uint64_t.
		 *  - vaddr_next representing the next input address (aligned with PDPTE_SIZE) to be handled,
		 *  initialized as '(vaddr & PDPTE_MASK) + PDPTE_SIZE'. */
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		/** If the return value of 'mem_ops->pgentry_present(*pdpte)' is 0,
		 *  indicating that the PDPTE pointed by 'pdpte' is not present */
		if (mem_ops->pgentry_present(*pdpte) == 0UL) {
			/** If \a type is equal to MR_MODIFY */
			if (type == MR_MODIFY) {
				/** Logging the following information with a log level of 4.
				 *  - __func__
				 *  - vaddr
				 */
				pr_warn("%s, vaddr: 0x%lx pdpte is not present.\n", __func__, vaddr);
			}
		} else {
			/** If the return value of 'pdpte_large(*pdpte)' is not 0,
			 *  indicating that the PDPTE pointed by 'pdpte' maps a 1-GByte page. */
			if (pdpte_large(*pdpte) != 0UL) {
				/** If any one of the following conditions is satisfied:
				 *  1. 'vaddr_next' is larger than \a vaddr_end.
				 *  2. The return value of 'mem_aligned_check(vaddr, PDPTE_SIZE)' is false, indicating
				 *  that 'vaddr' is not aligned with PDPTE_SIZE.
				 */
				if ((vaddr_next > vaddr_end) || (!mem_aligned_check(vaddr, PDPTE_SIZE))) {
					/** Call split_large_page with the following parameters, in order to
					 *  split the 1-GByte page mapped by 'pdpte' into 2-MByte pages.
					 *  - pdpte
					 *  - IA32E_PDPT
					 *  - vaddr
					 *  - mem_ops
					 */
					split_large_page(pdpte, IA32E_PDPT, vaddr, mem_ops);
				} else {
					/** Call local_modify_or_del_pte with the following parameters, in order to
					 *  modify or delete the mapping established by the PDPTE pointed by 'pdpte'.
					 *  - pdpte
					 *  - prot_set
					 *  - prot_clr
					 *  - type
					 *  - mem_ops
					 */
					local_modify_or_del_pte(pdpte, prot_set, prot_clr, type, mem_ops);
					/** If 'vaddr_next' is smaller than \a vaddr_end */
					if (vaddr_next < vaddr_end) {
						/** Set 'vaddr' to 'vaddr_next' */
						vaddr = vaddr_next;
						/** Continue to next iteration */
						continue;
					}
					/** Terminate the loop as all PDPTEs associated with the specified input
					 *  address space has been handled. */
					break; /* done */
				}
			}

			/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than \a vaddr_end;
			 *  otherwise, set 'vaddr_end_each_iter' to \a vaddr_end */
			vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
			/** Call modify_or_del_pde with the following parameters, in order to modify or delete the
			 *  mappings established by the PDEs (locating in the page directory referenced by 'pdpte')
			 *  associated with the input address space specified by [vaddr, vaddr_end_each_iter).
			 *  - pdpte
			 *  - vaddr
			 *  - vaddr_end_each_iter
			 *  - prot_set
			 *  - prot_clr
			 *  - mem_ops
			 *  - type
			 */
			modify_or_del_pde(pdpte, vaddr, vaddr_end_each_iter, prot_set, prot_clr, mem_ops, type);
		}
		/** If 'vaddr_next' is equal to or larger than \a vaddr_end, indicating that all PDPTEs
		 *  associated with the specified input address space has been handled. */
		if (vaddr_next >= vaddr_end) {
			/** Terminate the loop */
			break; /* done */
		}
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/*
 * type: MR_MODIFY
 * modify [vaddr, vaddr + size ) memory type or page access right.
 * prot_clr - memory type or page access right want to be clear
 * prot_set - memory type or page access right want to be set
 * @pre: the prot_set and prot_clr should set before call this function.
 * If you just want to modify access rights, you can just set the prot_clr
 * to what you want to set, prot_clr to what you want to clear. But if you
 * want to modify the MT, you should set the prot_set to what MT you want
 * to set, prot_clr to the MT mask.
 * type: MR_DEL
 * delete [vaddr_base, vaddr_base + size ) memory region page table mapping.
 */
/**
 * @brief Modify or delete the mappings associated with the specified input address space.
 *
 * The input address space is specified by [vaddr_base, vaddr_base + size).
 *
 * Modification means that the properties of the specified paging-structure entry is requested to be updated.
 * Deletion means that the current mapping established by the specified paging-structure entry is to be removed.
 *
 * It is supposed to be called by 'init_paging' and 'hv_access_memory_region_update' from 'hwmgmt.mmu' module,
 * and 'ept_modify_mr' and 'ept_del_mr' from 'vp-base.guest_mem' module.
 *
 * @param[inout] pml4_page A pointer to the specified PML4 table.
 * @param[in] vaddr_base The specified input address determining the start of the input address space whose mapping
 *                       information is to be updated.
 *                       For hypervisor's MMU, it is the host virtual address.
 *                       For each VM's EPT, it is the guest physical address.
 * @param[in] size The size of the specified input address space whose mapping information is to be updated.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by \a prot_clr are cleared before each bit specified by \a prot_set is set to 1.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 * @param[in] type The type of the requested operation on the specified paging-structure entry,
 *                 either modification or deletion.
 *
 * @return None
 *
 * @pre pml4_page != NULL
 * @pre pml4_page & 0FFFH == 0H
 * @pre mem_ops != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 * @pre For x86 hypervisor, the following conditions shall be met if "type == MR_MODIFY".
 *      - ( prot_set & ~(PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PS |
 *      PAGE_GLOBAL | PAGE_PAT_LARGE | PAGE_NX) == 0)
 *      - ( prot_clr & ~(PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PS |
 *      PAGE_GLOBAL | PAGE_PAT_LARGE | PAGE_NX) == 0)
 * @pre For the VM EPT mappings, the following conditions shall be met if "type == MR_MODIFY".
 *      - (prot_set & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK) == 0)
 *      - (prot_set & EPT_MT_MASK) == EPT_UNCACHED || (prot_set & EPT_MT_MASK) == EPT_WB
 *      - (prot_clr & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK) == 0)
 *      - (prot_clr & EPT_MT_MASK) == EPT_UNCACHED || (prot_clr & EPT_MT_MASK) == EPT_WB
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
void mmu_modify_or_del(uint64_t *pml4_page, uint64_t vaddr_base, uint64_t size, uint64_t prot_set, uint64_t prot_clr,
	const struct memory_ops *mem_ops, uint32_t type)
{
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be handled in
	 *  each iteration, not initialized.
	 *  - vaddr_next representing the next input address (aligned with PML4E_SIZE) to be mapped, not initialized.
	 *  - vaddr_end representing the input address determining the end of the input address space whose mapping
	 *  information is to be established, not initialized.
	 */
	uint64_t vaddr, vaddr_next, vaddr_end;
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pml4e representing the pointer to a PML4E, not initialized. */
	uint64_t *pml4e;
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be handled
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Set 'vaddr' to the return value of 'round_page_down(vaddr_base)', which is the
	 *  round-down 4-KByte aligned value corresponding to \a vaddr_base */
	vaddr = round_page_down(vaddr_base);
	/** Set 'vaddr_end' to 'round_page_up(vaddr_base + size)', which is the round-up 4-KByte aligned
	 *  value corresponding to the end of the region */
	vaddr_end = round_page_up(vaddr_base + size);

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - vaddr
	 *  - size
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: 0x%lx, size: 0x%lx\n", __func__, vaddr, size);

	/** Until 'vaddr' is equal to or larger than 'vaddr_end' */
	while (vaddr < vaddr_end) {
		/** Set 'vaddr_next' to '(vaddr & PML4E_MASK) + PML4E_SIZE' */
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		/** Set 'pml4e' to the return value of 'pml4e_offset(pml4_page, vaddr)', which points to the PML4E
		 *  (locating in the PML4 table pointed by \a pml4_page) associated with 'vaddr' */
		pml4e = pml4e_offset(pml4_page, vaddr);
		/** If following two conditions are both satisfied:
		 *  1. The return value of 'mem_ops->pgentry_present(*pml4e)' is 0, indicating that the PML4E pointed
		 *  by 'pml4e' is not present.
		 *  2. \a type is equal to MR_MODIFY.
		 */
		if ((mem_ops->pgentry_present(*pml4e) == 0UL) && (type == MR_MODIFY)) {
			/** Assert */
			ASSERT(false, "invalid op, pml4e not present");
		} else {
			/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than \a vaddr_end;
			 *  otherwise, set 'vaddr_end_each_iter' to \a vaddr_end */
			vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
			/** Call modify_or_del_pdpte with the following parameters, in order to modify or delete the
			 *  mappings established by the PDPTEs (locating in the page-directory-pointer table referenced
			 *  by 'pml4e') associated with the input address space specified by
			 *  [vaddr, vaddr_end_each_iter).
			 *  - pml4e
			 *  - vaddr
			 *  - vaddr_end_each_iter
			 *  - prot_set
			 *  - prot_clr
			 *  - mem_ops
			 *  - type
			 */
			modify_or_del_pdpte(pml4e, vaddr, vaddr_end_each_iter, prot_set, prot_clr, mem_ops, type);
		}
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/**
 * @brief Establish mappings on the PTEs associated with the specified input address space.
 *
 * This function maps the input address space specified by [vaddr_start, vaddr_end) to the physical memory region
 * starting from \a paddr_start.
 * The PTEs are located in the page table referenced by the PDE pointed by \a pde.
 *
 * It is supposed to be called internally by 'add_pde'.
 *
 * @param[in] pde A pointer to the specified PDE that references a page table.
 * @param[in] paddr_start The specified physical address determining the start of the physical memory region.
 *                        It is the host physical address.
 * @param[in] vaddr_start The specified input address determining the start of the input address space.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space.
 *                      This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pde != NULL
 * @pre mem_ops != NULL
 * @pre mem_aligned_check(paddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void add_pte(const uint64_t *pde, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end, uint64_t prot,
	const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pt_page representing a pointer to the page table referenced by the PDE pointed by \a pde,
	 *  initialized as the return value of 'pde_page_vaddr(*pde)'. */
	uint64_t *pt_page = pde_page_vaddr(*pde);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be mapped in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - paddr representing the host physical address determining the start of the physical memory region
	 *  to be mapped to in each iteration, initialized as \a paddr_start. */
	uint64_t paddr = paddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PTEs associated with the specified input address space,
	 *  initialized as the return value of 'pte_index(vaddr)'. */
	uint64_t index = pte_index(vaddr);

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - paddr
	 *  - vaddr_start
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n", __func__, paddr, vaddr_start, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PTE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PTE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pte representing the pointer to the 'index'-th PTE located in the page table pointed by 'pt_page',
		 *  initialized as 'pt_page + index'. */
		uint64_t *pte = pt_page + index;

		/** If the return value of 'mem_ops->pgentry_present(*pte)' is not 0,
		 *  indicating that the PTE pointed by 'pte' is present */
		if (mem_ops->pgentry_present(*pte) != 0UL) {
			/** Logging the following information with a log level of 1.
			 *  - __func__
			 *  - vaddr
			 */
			pr_fatal("%s, pte 0x%lx is already present!\n", __func__, vaddr);
		} else {
			/** Call set_pgentry with the following parameters, in order to set the content stored in
			 *  the PTE pointed by 'pte' to 'paddr | prot'.
			 *  - pte
			 *  - paddr | prot
			 *  - mem_ops
			 */
			set_pgentry(pte, paddr | prot, mem_ops);
			/** Increment 'paddr' by PTE_SIZE */
			paddr += PTE_SIZE;
			/** Increment 'vaddr' by PTE_SIZE */
			vaddr += PTE_SIZE;

			/** If 'vaddr' is equal to or larger than \a vaddr_end, indicating that all PTEs
			 *  associated with the specified input address space has been mapped. */
			if (vaddr >= vaddr_end) {
				/** Terminate the loop */
				break; /* done */
			}
		}
	}
}

/**
 * @brief Establish mappings on the PDEs associated with the specified input address space.
 *
 * This function maps the input address space specified by [vaddr_start, vaddr_end) to the physical memory region
 * starting from \a paddr_start.
 * The PDEs are located in the page directory referenced by the PDPTE pointed by \a pdpte.
 *
 * It is supposed to be called internally by 'add_pdpte'.
 *
 * @param[in] pdpte A pointer to the specified PDPTE that references a page directory.
 * @param[in] paddr_start The specified physical address determining the start of the physical memory region.
 *                        It is the host physical address.
 * @param[in] vaddr_start The specified input address determining the start of the input address space.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space.
 *                      This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pdpte != NULL
 * @pre mem_ops != NULL
 * @pre mem_aligned_check(paddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void add_pde(const uint64_t *pdpte, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end,
	uint64_t prot, const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pd_page representing a pointer to the page directory referenced by the PDPTE pointed by \a pdpte,
	 *  initialized as the return value of 'pdpte_page_vaddr(*pdpte)'. */
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be mapped in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - paddr representing the host physical address determining the start of the physical memory region
	 *  to be mapped to in each iteration, initialized as \a paddr_start. */
	uint64_t paddr = paddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PDEs associated with the specified input address space,
	 *  initialized as the return value of 'pde_index(vaddr)'. */
	uint64_t index = pde_index(vaddr);
	/** Declare the following local variables of type uint64_t.
	 *  - effective_prot representing the properties that eventually set to a paging-structure entry,
	 *  initialized as \a prot. */
	uint64_t effective_prot = prot;
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be mapped
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - paddr
	 *  - vaddr
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n", __func__, paddr, vaddr, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PDE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PDE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pde representing the pointer to the 'index'-th PDE located in the page directory pointed by
		 *  'pd_page', initialized as 'pd_page + index'. */
		uint64_t *pde = pd_page + index;
		/** Declare the following local variables of type uint64_t.
		 *  - vaddr_next representing the next input address (aligned with PDE_SIZE) to be mapped,
		 *  initialized as '(vaddr & PDE_MASK) + PDE_SIZE'. */
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		/** If the return value of 'pde_large(*pde)' is not 0,
		 *  indicating that the PDE pointed by 'pde' maps a 2-MByte page. */
		if (pde_large(*pde) != 0UL) {
			/** Logging the following information with a log level of 1.
			 *  - __func__
			 *  - vaddr
			 */
			pr_fatal("%s, pde 0x%lx already maps a 2-MByte page!\n", __func__, vaddr);
		} else {
			/** If the return value of 'mem_ops->pgentry_present(*pde)' is 0,
			 *  indicating that the PDE pointed by 'pde' is not present */
			if (mem_ops->pgentry_present(*pde) == 0UL) {
				/** If following four conditions are both satisfied, indicating that the PDE pointed
				 *  by 'pde' could be used to map a 2-MByte page:
				 *  1. 'mem_ops->large_page_enabled' is true, indicating that the large pages
				 *  (1-GByte or 2-MByte) are allowed to be used.
				 *  2. Return value of 'mem_aligned_check(paddr, PDE_SIZE)' is true, indicating that
				 *  that 'paddr' is aligned with PDE_SIZE.
				 *  3. Return value of 'mem_aligned_check(vaddr, PDE_SIZE)' is true, indicating that
				 *  that 'vaddr' is aligned with PDE_SIZE.
				 *  4. 'vaddr_next' is equal to or smaller than \a vaddr_end.
				 */
				if (mem_ops->large_page_enabled && mem_aligned_check(paddr, PDE_SIZE) &&
					mem_aligned_check(vaddr, PDE_SIZE) && (vaddr_next <= vaddr_end)) {
					/** Call mem_ops->tweak_exe_right with the following parameters, in order to
					 *  tweak the execute permission in 'effective_prot'.
					 *  - &effective_prot
					 */
					mem_ops->tweak_exe_right(&effective_prot);
					/** Call set_pgentry with the following parameters, in order to set the content
					 *  stored in the PDE pointed by 'pde' to 'paddr | (effective_prot | PAGE_PS)'
					 *  so that this PDE maps a 2-MByte page.
					 *  - pde
					 *  - paddr | (effective_prot | PAGE_PS)
					 *  - mem_ops
					 */
					set_pgentry(pde, paddr | (effective_prot | PAGE_PS), mem_ops);
					/** If 'vaddr_next' is smaller than \a vaddr_end */
					if (vaddr_next < vaddr_end) {
						/** Increment 'paddr' by 'vaddr_next - vaddr', where the increment
						 *  indicates the size of the space that has been mapped. */
						paddr += (vaddr_next - vaddr);
						/** Set 'vaddr' to 'vaddr_next' */
						vaddr = vaddr_next;
						/** Continue to next iteration */
						continue;
					}
					/** Terminate the loop as all PDEs associated with the specified input
					 *  address space has been mapped. */
					break; /* done */
				} else {
					/** Declare the following local variables of type 'void *'.
					 *  - pt_page representing a pointer to the page table associated with 'vaddr',
					 *  initialized as the return value of
					 *  'mem_ops->get_pt_page(mem_ops->info, vaddr)'. */
					void *pt_page = mem_ops->get_pt_page(mem_ops->info, vaddr);
					/** Call construct_pgentry with the following parameters, in order to
					 *  construct the PDE pointed by 'pde' to reference the page table pointed by
					 *  'pt_page'.
					 *  - pde
					 *  - pt_page
					 *  - mem_ops->get_default_access_right()
					 *  - mem_ops
					 */
					construct_pgentry(pde, pt_page, mem_ops->get_default_access_right(), mem_ops);
				}
			}

			/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than \a vaddr_end;
			 *  otherwise, set 'vaddr_end_each_iter' to \a vaddr_end */
			vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
			/** Call add_pte with the following parameters, in order to establish mappings on the PTEs
			 *  (locating in the page table referenced by 'pde') associated with the
			 *  input address space specified by [vaddr, vaddr_end_each_iter).
			 *  - pde
			 *  - paddr
			 *  - vaddr
			 *  - vaddr_end_each_iter
			 *  - effective_prot
			 *  - mem_ops
			 */
			add_pte(pde, paddr, vaddr, vaddr_end_each_iter, effective_prot, mem_ops);
		}
		/** If 'vaddr_next' is equal to or larger than \a vaddr_end, indicating that all PDEs
		 *  associated with the specified input address space has been mapped. */
		if (vaddr_next >= vaddr_end) {
			/** Terminate the loop */
			break; /* done */
		}
		/** Increment 'paddr' by 'vaddr_next - vaddr', where the increment indicates the size of the space
		 *  that has been mapped. */
		paddr += (vaddr_next - vaddr);
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/**
 * @brief Establish mappings on the PDPTEs associated with the specified input address space.
 *
 * This function maps the input address space specified by [vaddr_start, vaddr_end) to the physical memory region
 * starting from \a paddr_start.
 * The PDPTEs are located in the page-directory-pointer table referenced by the PML4E pointed by \a pml4e.
 *
 * It is supposed to be called internally by 'mmu_add'.
 *
 * @param[in] pml4e A pointer to the specified PML4E that references a page-directory-pointer table.
 * @param[in] paddr_start The specified physical address determining the start of the physical memory region.
 *                        It is the host physical address.
 * @param[in] vaddr_start The specified input address determining the start of the input address space.
 *                        For hypervisor's MMU, it is the host virtual address.
 *                        For each VM's EPT, it is the guest physical address.
 * @param[in] vaddr_end The specified input address determining the end of the input address space.
 *                      This address is exclusive.
 *                      For hypervisor's MMU, it is the host virtual address.
 *                      For each VM's EPT, it is the guest physical address.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pml4e != NULL
 * @pre mem_ops != NULL
 * @pre mem_aligned_check(paddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_start, PAGE_SIZE) == true
 * @pre mem_aligned_check(vaddr_end, PAGE_SIZE) == true
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
static void add_pdpte(const uint64_t *pml4e, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end,
	uint64_t prot, const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pdpt_page representing a pointer to the page-directory-pointer table referenced by the PML4E pointed
	 *  by \a pml4e, initialized as the return value of 'pml4e_page_vaddr(*pml4e)'. */
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be mapped in
	 *  each iteration, initialized as \a vaddr_start. */
	uint64_t vaddr = vaddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - paddr representing the host physical address determining the start of the physical memory region
	 *  to be mapped to in each iteration, initialized as \a paddr_start. */
	uint64_t paddr = paddr_start;
	/** Declare the following local variables of type uint64_t.
	 *  - index representing the index to go through the PDPTEs associated with the specified input address space,
	 *  initialized as the return value of 'pdpte_index(vaddr)'. */
	uint64_t index = pdpte_index(vaddr);
	/** Declare the following local variables of type uint64_t.
	 *  - effective_prot representing the properties that eventually set to a paging-structure entry,
	 *  initialized as \a prot. */
	uint64_t effective_prot = prot;
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be mapped
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - paddr
	 *  - vaddr
	 *  - vaddr_end
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n", __func__, paddr, vaddr, vaddr_end);
	/** For each 'index' ranging from the initial value of 'index' to 'PTRS_PER_PDPTE - 1' [with a step of 1] */
	for (; index < PTRS_PER_PDPTE; index++) {
		/** Declare the following local variables of type 'uint64_t *'.
		 *  - pdpte representing the pointer to the 'index'-th PDPTE located in the
		 *  page-directory-pointer table pointed by 'pdpt_page', initialized as 'pdpt_page + index'. */
		uint64_t *pdpte = pdpt_page + index;
		/** Declare the following local variables of type uint64_t.
		 *  - vaddr_next representing the next input address (aligned with PDPTE_SIZE) to be mapped,
		 *  initialized as '(vaddr & PDPTE_MASK) + PDPTE_SIZE'. */
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		/** If the return value of 'pde_large(*pdpte)' is not 0,
		 *  indicating that the PDPTE pointed by 'pdpte' maps a 1-GByte page. */
		if (pdpte_large(*pdpte) != 0UL) {
			/** Logging the following information with a log level of 1.
			 *  - __func__
			 *  - vaddr
			 */
			pr_fatal("%s, pdpte 0x%lx already maps a 1-GByte page!\n", __func__, vaddr);
		} else {
			/** If the return value of 'mem_ops->pgentry_present(*pdpte)' is 0,
			 *  indicating that the PDPTE pointed by 'pdpte' is not present */
			if (mem_ops->pgentry_present(*pdpte) == 0UL) {
				/** If following four conditions are both satisfied, indicating that the PDPTE pointed
				 *  by 'pdpte' could be used to map a 1-GByte page:
				 *  1. 'mem_ops->large_page_enabled' is true, indicating that the large pages
				 *  (1-GByte or 2-MByte) are allowed to be used.
				 *  2. Return value of 'mem_aligned_check(paddr, PDPTE_SIZE)' is true, indicating that
				 *  that 'paddr' is aligned with PDPTE_SIZE.
				 *  3. Return value of 'mem_aligned_check(vaddr, PDPTE_SIZE)' is true, indicating that
				 *  that 'vaddr' is aligned with PDPTE_SIZE.
				 *  4. 'vaddr_next' is equal to or smaller than \a vaddr_end.
				 */
				if (mem_ops->large_page_enabled && mem_aligned_check(paddr, PDPTE_SIZE) &&
					mem_aligned_check(vaddr, PDPTE_SIZE) && (vaddr_next <= vaddr_end)) {
					/** Call mem_ops->tweak_exe_right with the following parameters, in order to
					 *  tweak the execute permission in 'effective_prot'.
					 *  - &effective_prot
					 */
					mem_ops->tweak_exe_right(&effective_prot);
					/** Call set_pgentry with the following parameters, in order to set the content
					 *  stored in the PDPTE pointed by 'pdpte' to
					 *  'paddr | (effective_prot | PAGE_PS)' so that this PDPTE maps a 1-GByte page.
					 *  - pdpte
					 *  - paddr | (effective_prot | PAGE_PS)
					 *  - mem_ops
					 */
					set_pgentry(pdpte, paddr | (effective_prot | PAGE_PS), mem_ops);
					/** If 'vaddr_next' is smaller than \a vaddr_end */
					if (vaddr_next < vaddr_end) {
						/** Increment 'paddr' by 'vaddr_next - vaddr', where the increment
						 *  indicates the size of the space that has been mapped. */
						paddr += (vaddr_next - vaddr);
						/** Set 'vaddr' to 'vaddr_next' */
						vaddr = vaddr_next;
						/** Continue to next iteration */
						continue;
					}
					/** Terminate the loop as all PDPTEs associated with the specified input
					 *  address space has been mapped. */
					break; /* done */
				} else {
					/** Declare the following local variables of type 'void *'.
					 *  - pd_page representing a pointer to the page directory associated with
					 *  'vaddr', initialized as the return value of
					 *  'mem_ops->get_pd_page(mem_ops->info, vaddr)'. */
					void *pd_page = mem_ops->get_pd_page(mem_ops->info, vaddr);
					/** Call construct_pgentry with the following parameters, in order to
					 *  construct the PDPTE pointed by 'pdpte' to reference the page directory
					 *  pointed by 'pd_page'.
					 *  - pdpte
					 *  - pd_page
					 *  - mem_ops->get_default_access_right()
					 *  - mem_ops
					 */
					construct_pgentry(pdpte, pd_page, mem_ops->get_default_access_right(), mem_ops);
				}
			}

			/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than \a vaddr_end;
			 *  otherwise, set 'vaddr_end_each_iter' to \a vaddr_end */
			vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
			/** Call add_pde with the following parameters, in order to establish mappings on the PDEs
			 *  (locating in the page directory referenced by 'pdpte') associated with the
			 *  input address space specified by [vaddr, vaddr_end_each_iter).
			 *  - pdpte
			 *  - paddr
			 *  - vaddr
			 *  - vaddr_end_each_iter
			 *  - effective_prot
			 *  - mem_ops
			 */
			add_pde(pdpte, paddr, vaddr, vaddr_end_each_iter, effective_prot, mem_ops);
		}
		/** If 'vaddr_next' is equal to or larger than \a vaddr_end, indicating that all PDEs
		 *  associated with the specified input address space has been mapped. */
		if (vaddr_next >= vaddr_end) {
			/** Terminate the loop */
			break; /* done */
		}
		/** Increment 'paddr' by 'vaddr_next - vaddr', where the increment indicates the size of the space
		 *  that has been mapped. */
		paddr += (vaddr_next - vaddr);
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/**
 * @brief Establish mappings on the paging-structure entries associated with the specified input address space.
 *
 * This function maps the input address space specified by [vaddr_base, vaddr_base + size) to the physical memory region
 * starting from \a paddr_base.
 *
 * It is supposed to be called by 'init_paging' from 'hwmgmt.mmu' module and
 * 'ept_add_mr' from 'vp-base.guest_mem' module.
 *
 * @param[inout] pml4_page A pointer to the specified PML4 table.
 * @param[in] paddr_base The specified physical address determining the start of the physical memory region.
 *                       It is the host physical address.
 * @param[in] vaddr_base The specified input address determining the start of the input address space.
 *                       For hypervisor's MMU, it is the host virtual address.
 *                       For each VM's EPT, it is the guest physical address.
 * @param[in] size The size of the specified input address space.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pml4e != NULL
 * @pre mem_ops != NULL
 * @pre 0 < vaddr_size < get_mem_range_info()->mem_top
 * @pre Any subrange within [vaddr_base, vaddr_base + size) shall be unmapped before calling mmu_add.
 * @pre For x86 hypervisor mapping, the following condition shall be met.
 *      - prot & ~(PAGE_PRESENT| PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PS |
 *      PAGE_GLOBAL | PAGE_PAT_LARGE | PAGE_NX) == 0
 * @pre For VM EPT mapping, the following conditions shall be met.
 *      - prot & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK | EPT_SNOOP_CTRL) == 0
 *      - (prot & EPT_MT_MASK) == EPT_UNCACHED || (prot & EPT_MT_MASK) == EPT_WB
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
void mmu_add(uint64_t *pml4_page, uint64_t paddr_base, uint64_t vaddr_base, uint64_t size, uint64_t prot,
	const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr representing the address determining the start of the input address space to be mapped in
	 *  each iteration, not initialized.
	 *  - vaddr_next representing the next input address (aligned with PML4E_SIZE) to be mapped, not initialized.
	 *  - vaddr_end representing the input address determining the end of the input address space whose mapping
	 *  information is to be established, not initialized.
	 */
	uint64_t vaddr, vaddr_next, vaddr_end;
	/** Declare the following local variables of type uint64_t.
	 *  - paddr representing the host physical address determining the start of the physical memory region
	 *  to be mapped to in each iteration, not initialized. */
	uint64_t paddr;
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pml4e representing the pointer to a PML4E, not initialized. */
	uint64_t *pml4e;
	/** Declare the following local variables of type uint64_t.
	 *  - vaddr_end_each_iter representing the address determining the end of the input address space to be mapped
	 *  in each iteration, not initialized. */
	uint64_t vaddr_end_each_iter;

	/** Logging the following information with a log level of ACRN_DBG_MMU.
	 *  - __func__
	 *  - paddr_base
	 *  - vaddr_base
	 *  - size
	 */
	dev_dbg(ACRN_DBG_MMU, "%s, paddr 0x%lx, vaddr 0x%lx, size 0x%lx\n", __func__, paddr_base, vaddr_base, size);

	/** Set 'vaddr' to the return value of 'round_page_down(vaddr_base)', which is the
	 *  round-down 4-KByte aligned value corresponding to \a vaddr_base */
	vaddr = round_page_down(vaddr_base);
	/** Set 'paddr' to the return value of 'round_page_down(paddr_base)', which is the
	 *  round-down 4-KByte aligned value corresponding to \a paddr_base */
	paddr = round_page_down(paddr_base);
	/** Set 'vaddr_end' to 'round_page_up(vaddr_base + size)', which is the round-up 4-KByte aligned
	 *  value corresponding to the end of the region */
	vaddr_end = round_page_up(vaddr_base + size);

	/** Until 'vaddr' is equal to or larger than 'vaddr_end' */
	while (vaddr < vaddr_end) {
		/** Set 'vaddr_next' to '(vaddr & PML4E_MASK) + PML4E_SIZE' */
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		/** Set 'pml4e' to the return value of 'pml4e_offset(pml4_page, vaddr)', which points to the PML4E
		 *  (locating in the PML4 table pointed by \a pml4_page) associated with 'vaddr' */
		pml4e = pml4e_offset(pml4_page, vaddr);
		/** If the return value of 'mem_ops->pgentry_present(*pml4e)' is 0, indicating that the PML4E pointed
		 *  by 'pml4e' is not present. */
		if (mem_ops->pgentry_present(*pml4e) == 0UL) {
			/** Declare the following local variables of type 'void *'.
			 *  - pdpt_page representing a pointer to the page-directory-pointer table associated with
			 *  'vaddr', initialized as the return value of 'mem_ops->get_pdpt_page(mem_ops->info, vaddr)'.
			 */
			void *pdpt_page = mem_ops->get_pdpt_page(mem_ops->info, vaddr);
			/** Call construct_pgentry with the following parameters, in order to construct the PML4E
			 *  pointed by 'pml4e' to reference the page-directory-pointer table pointed by 'pdpt_page'.
			 *  - pml4e
			 *  - pdpt_page
			 *  - mem_ops->get_default_access_right()
			 *  - mem_ops
			 */
			construct_pgentry(pml4e, pdpt_page, mem_ops->get_default_access_right(), mem_ops);
		}

		/** Set 'vaddr_end_each_iter' to 'vaddr_next' if 'vaddr_next' is smaller than 'vaddr_end';
		 *  otherwise, set 'vaddr_end_each_iter' to 'vaddr_end' */
		vaddr_end_each_iter = (vaddr_next < vaddr_end) ? vaddr_next : vaddr_end;
		/** Call add_pdpte with the following parameters, in order to establish mappings on the PDPTEs
		 *  (locating in the page-directory-pointer table referenced by 'pml4e') associated with the
		 *  input address space specified by [vaddr, vaddr_end_each_iter).
		 *  - pml4e
		 *  - paddr
		 *  - vaddr
		 *  - vaddr_end_each_iter
		 *  - prot
		 *  - mem_ops
		 */
		add_pdpte(pml4e, paddr, vaddr, vaddr_end_each_iter, prot, mem_ops);

		/** Increment 'paddr' by 'vaddr_next - vaddr', where the increment indicates the size of the space
		 *  that has been covered in the PML4E pointed by 'pml4e'. */
		paddr += (vaddr_next - vaddr);
		/** Set 'vaddr' to 'vaddr_next' */
		vaddr = vaddr_next;
	}
}

/**
 * @brief Look for the paging-structure entry that contains the mapping information for the specified input address.
 *
 * This function looks for the paging-structure entry that contains the mapping information for the specified
 * input address of the translation process.
 *
 * It is supposed to be called by 'local_gpa2hpa' from 'vp-base.guest_mem' module.
 *
 * @param[in] pml4_page A pointer to the specified PML4 table.
 * @param[in] addr The specified input address whose mapping information is to be searched.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 * @param[out] pg_size A pointer to the size of the space controlled by the returned paging-structure entry.
 * @param[in] mem_ops A pointer to the data structure containing the information of the specified memory operations.
 *
 * @return A pointer to the paging-structure entry that contains the mapping information for the specified
 *         input address.
 *
 * @retval non-NULL There exists a paging-structure entry that contains the mapping information for the specified
 *                  input address.
 * @retval NULL There is no paging-structure entry that contains the mapping information for the specified
 *              input address.
 *
 * @pre pml4_page != NULL
 * @pre pg_size != NULL
 * @pre mem_ops != NULL
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
const uint64_t *lookup_address(uint64_t *pml4_page, uint64_t addr, uint64_t *pg_size, const struct memory_ops *mem_ops)
{
	/** Declare the following local variables of type 'const uint64_t *'.
	 *  - pret representing a pointer to the paging-structure entry that contains the mapping information for
	 *  the specified input address, initialized as NULL. */
	const uint64_t *pret = NULL;
	/** Declare the following local variables of type bool.
	 *  - present representing whether the specified paging-structure entry is present or not,
	 *  initialized as true. */
	bool present = true;
	/** Declare the following local variables of type 'uint64_t *'.
	 *  - pml4e representing a pointer to the PML4E associated with the specified input address, not initialized.
	 *  - pdpte representing a pointer to the PDPTE associated with the specified input address, not initialized.
	 *  - pde representing a pointer to the PDE associated with the specified input address, not initialized.
	 *  - pte representing a pointer to the PTE associated with the specified input address, not initialized.
	 */
	uint64_t *pml4e, *pdpte, *pde, *pte;

	/** Set 'pml4e' to the return value of 'pml4e_offset(pml4_page, addr)', which points to the PML4E
	 *  (locating in the PML4 table pointed by \a pml4_page) associated with \a addr */
	pml4e = pml4e_offset(pml4_page, addr);
	/** Set 'present' to false if the return value of 'mem_ops->pgentry_present(*pml4e)' is 0; otherwise,
	 *  set 'present' to true */
	present = (mem_ops->pgentry_present(*pml4e) != 0UL);

	/** If 'present' is true, indicating that the PML4E pointed by 'pml4e' is present */
	if (present) {
		/** Set 'pdpte' to the return value of 'pdpte_offset(pml4e, addr)', which points to the PDPTE
		 *  (locating in the page-directory-pointer table referenced by 'pml4e') associated with \a addr */
		pdpte = pdpte_offset(pml4e, addr);
		/** Set 'present' to false if the return value of 'mem_ops->pgentry_present(*pdpte)' is 0; otherwise,
		 *  set 'present' to true */
		present = (mem_ops->pgentry_present(*pdpte) != 0UL);
		/** If 'present' is true, indicating that the PDPTE pointed by 'pdpte' is present */
		if (present) {
			/** If the return value of 'pde_large(*pdpte)' is not 0,
			 *  indicating that the PDPTE pointed by 'pdpte' maps a 1-GByte page. */
			if (pdpte_large(*pdpte) != 0UL) {
				/** Set '*pg_size' to PDPTE_SIZE */
				*pg_size = PDPTE_SIZE;
				/** Set 'pret' to 'pdpte' */
				pret = pdpte;
			} else {
				/** Set 'pde' to the return value of 'pde_offset(pdpte, addr)', which points to the
				 *  PDE (locating in the page directory referenced by 'pdpte') associated with \a addr
				 */
				pde = pde_offset(pdpte, addr);
				/** Set 'present' to false if the return value of 'mem_ops->pgentry_present(*pde)'
				 *  is 0; otherwise, set 'present' to true */
				present = (mem_ops->pgentry_present(*pde) != 0UL);
				/** If 'present' is true, indicating that the PDE pointed by 'pde' is present */
				if (present) {
					/** If the return value of 'pde_large(*pde)' is not 0,
					 *  indicating that the PDE pointed by 'pde' maps a 2-MByte page. */
					if (pde_large(*pde) != 0UL) {
						/** Set '*pg_size' to PDE_SIZE */
						*pg_size = PDE_SIZE;
						/** Set 'pret' to 'pde' */
						pret = pde;
					} else {
						/** Set 'pte' to the return value of 'pte_offset(pde, addr)', which
						 *  points to the PTE (locating in the page table referenced by 'pde')
						 *  associated with \a addr */
						pte = pte_offset(pde, addr);
						/** Set 'present' to false if the return value of
						 *  'mem_ops->pgentry_present(*pte)' is 0; otherwise,
						 *  set 'present' to true */
						present = (mem_ops->pgentry_present(*pte) != 0UL);
						/** If 'present' is true, indicating that the PTE pointed by 'pte'
						 *  is present */
						if (present) {
							/** Set '*pg_size' to PTE_SIZE */
							*pg_size = PTE_SIZE;
							/** Set 'pret' to 'pte' */
							pret = pte;
						}
					}
				}
			}
		}
	}

	/** Return 'pret' */
	return pret;
}

/**
 * @brief Walk through all the entries for a given page table.
 *
 * This function walks through all the entries for a given page table structure specified by \a pml4_page and apply the
 * action \a cb on each entry that identifies a page frame (rather than a next-level page table).
 *
 * @param[in] pml4_page Pointer to the PML4 page of the paging structure to be visited
 * @param[in] mem_ops A collection of function pointers to paging structure specific operations
 * @param[in] cb The callback for each entry that identifies a page frame
 *
 * @return None
 *
 * @pre pml4_page != NULL
 * @pre mem_ops != NULL
 * @pre cb != NULL
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
void walk_page_table(uint64_t *pml4_page, const struct memory_ops *mem_ops, pge_handler cb)
{
	/** Declare the following local variables of type uint64_t *.
	 *  - pml4e representing the PML4 Table entry, not initialized.
	 *  - pdpte representing the Page-Directory-Pointer Table entry, not initialized.
	 *  - pde representing the Page Directory Table entry, not initialized.
	 *  - pte representing the Page Table entry, not initialized.
	 */
	uint64_t *pml4e, *pdpte, *pde, *pte;
	/** Declare the following local variables of type uint64_t.
	 *  - i representing the PML4 Table entry index, not initialized.
	 *  - j representing the Page-Directory-Pointer Table entry index, not initialized.
	 *  - k representing the Page Directory Table entry index, not initialized.
	 *  - m representing the Page Table entry index, not initialized.
	 */
	uint64_t i, j, k, m;

	/** For each i ranging from 0 to (PTRS_PER_PML4E - 1), in order to loop through all the PML4 Table entries. */
	for (i = 0UL; i < PTRS_PER_PML4E; i++) {
		/** Call pml4e_offset with following parameters to get PML4 table entry and
		 *  set its value to 'pml4e'.
		 *  - pml4_page
		 *  - i << PML4E_SHIFT
		 */
		pml4e = pml4e_offset(pml4_page, i << PML4E_SHIFT);
		/** If a call to mem_ops->pgentry_present with *pml4e being the parameter returns zero, indicating the
		 *  table entry is not present */
		if (mem_ops->pgentry_present(*pml4e) == 0UL) {
			/** Continue this loop */
			continue;
		}
		/** For each j ranging from 0 to (PTRS_PER_PDPTE - 1), in order to loop through all
		 *  the Page-directory-pointer table entries.
		 */
		for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
			/**  Call pdpte_offset with following parameters to get Page-Directory-Pointer Table entry
			 *   and set return its value to 'pdpte'.
			 *  - pml4e
			 *  - j << PDPTE_SHIFT
			 */
			pdpte = pdpte_offset(pml4e, j << PDPTE_SHIFT);

			/** If a call to mem_ops->pgentry_present with *pdpte being the parameter returns zero,
			 *  indicating the table entry is not present */
			if (mem_ops->pgentry_present(*pdpte) == 0UL) {
				/** Continue this loop */
				continue;
			}

			/** If a call to pdpte_large with *pdpte being the parameter returns a non-zero value,
			 *  indicating the table entry identifies a 1GB page frame */
			if (pdpte_large(*pdpte) != 0UL) {
				/** Call \a cb with following parameters to perform an action for this page entry.
				 *  - pdpte
				 *  - PDPTE_SIZE
				 */
				cb(pdpte, PDPTE_SIZE);
				/** Continue this loop */
				continue;
			}
			/** For each k ranging from 0 to (PTRS_PER_PDE - 1), in order to loop
			 *  through all the Page Directory entries.
			 */
			for (k = 0UL; k < PTRS_PER_PDE; k++) {
				/** Call pde_offset with following parameters to get Page
				 *  directory entry and set its return value to 'pde'.
				 *  - pdpte
				 *  - k << PDE_SHIFT
				 */
				pde = pde_offset(pdpte, k << PDE_SHIFT);

				/** If a call to mem_ops->pgentry_present with *pde being the parameter returns zero,
				 *  indicating the table entry is not present */
				if (mem_ops->pgentry_present(*pde) == 0UL) {
					/** Continue this loop */
					continue;
				}

				/** If a call to pde_large with *pde being the parameter returns a non-zero value,
				 *  indicating the table entry identifies a 2MB page frame */
				if (pde_large(*pde) != 0UL) {
					/** Call \a cb with following parameters to perform an
					 *  action for this page entry.
					 *  - pde
					 *  - PDE_SIZE
					 */
					cb(pde, PDE_SIZE);
					/** Continue this loop */
					continue;
				}
				/** For each m ranging from 0 to (PTRS_PER_PTE - 1), in order to
				 *  loop through all the Page Table entries.
				 */
				for (m = 0UL; m < PTRS_PER_PTE; m++) {
					/** Call pte_offset with following parameters to get
					 *  Page Table entry and set its return value to 'pte'.
					 *  - pde
					 *  - m << PTE_SHIFT
					 */
					pte = pte_offset(pde, m << PTE_SHIFT);

					/** If a call to mem_ops->pgentry_present with *pte being the parameter returns
					 *  a non-zero value, indicating the table entry is present */
					if (mem_ops->pgentry_present(*pte) != 0UL) {
						/** Call \a cb with following parameters to perform an action
						 *  for this page entry.
						 *  - pte
						 *  - PTE_SIZE
						 */
						cb(pte, PTE_SIZE);
					}
				}
			}
		}
	}
}

/**
 * @}
 */
