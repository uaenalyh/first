/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief This file defines macros and data structures related to the high-level paging structures information.
 *
 * Following functionalities are provided in this file:
 * - Define the macros related to the 4-KByte page.
 * - Define the macros to calculate the number of requested paging structures.
 * - Define the data structures to store the information of paging structures.
 * - Define the data structures to store the information to be used for MMU operations and EPT operations.
 * - Declare the global variable 'ppt_mem_ops' to provide the information to be used for hypervisor's MMU operations.
 * - Declare the function 'init_ept_mem_ops' to provide the information to be used for each VM's EPT operations.
 */

/**
 * @brief The number of shift bits to determine the alignment of the pages in hypervisor.
 *
 * In ACRN hypervisor, all pages are aligned with 4-KByte and low 12-bits of their address are all 0s.
 */
#define PAGE_SHIFT 12U
/**
 * @brief Size of the 4-KByte page.
 */
#define PAGE_SIZE  (1U << PAGE_SHIFT)
/**
 * @brief The mask to clear low 12-bits of the address of a 4-KByte aligned page.
 */
#define PAGE_MASK  0xFFFFFFFFFFFFF000UL

/**
 * @brief Size of the low MMIO address space (2-GByte).
 */
#define PLATFORM_LO_MMIO_SIZE 0x80000000UL

/**
 * @brief The number of PML4 tables per paging structure.
 */
#define PML4_PAGE_NUM 1UL

/**
 * @brief Calculate the number of page-directory-pointer tables that is requested to control the memory region with
 *        the specified size.
 *
 * A page-directory-pointer table can be referenced by a PML4E and each PML4E controls access to a 512-GByte region.
 *
 * It is supposed to be called when hypervisor allocates the page-directory-pointer tables for hypervisor and all VMs.
 *
 * @param[in] size The specified size of the memory region.
 *
 * @return The number of page-directory-pointer tables.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define PDPT_PAGE_NUM(size) (((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)

/**
 * @brief Calculate the number of page directories that is requested to control the memory region with the specified
 *        size.
 *
 * A page directory can be referenced by a PDPTE and each PDPTE controls access to a 1-GByte region.
 *
 * It is supposed to be called when hypervisor allocates the page directories for hypervisor and all VMs.
 *
 * @param[in] size The specified size of the memory region.
 *
 * @return The number of page directories.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define PD_PAGE_NUM(size)   (((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)

/**
 * @brief Calculate the number of page tables that is requested to control the memory region with the specified size.
 *
 * A page table can be referenced by a PDE and each PDE controls access to a 2-MByte region.
 *
 * It is supposed to be called when hypervisor allocates the page tables for hypervisor and all VMs.
 *
 * @param[in] size The specified size of the memory region.
 *
 * @return The number of page tables.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define PT_PAGE_NUM(size)   (((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)

/*
 * The size of the guest physical address space, covered by the EPT page table of a VM.
 * With the assumptions:
 * - The GPA of DRAM & MMIO are contiguous.
 * - Guest OS won't re-program device MMIO bars to the address not covered by
 *   this EPT_ADDRESS_SPACE.
 */
/**
 * @brief Calculate the size of the memory region that is controlled by EPT for each VM.
 *
 * For each VM, EPT controls the following memory regions: RAM and the low MMIO address space.
 *
 * It is supposed to be called when hypervisor allocates the paging structures for all VMs.
 *
 * @param[in] size The specified size of RAM.
 *
 * @return The size of the memory region that is controlled by EPT.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define EPT_ADDRESS_SPACE(size) ((size) + PLATFORM_LO_MMIO_SIZE)

/**
 * @brief Data structure to illustrate a 4-KByte memory region with an alignment of 4-KByte.
 *
 * Data structure to illustrate a 4-KByte memory region with an alignment of 4-KByte, calling it a 4-KByte page.
 *
 * It can be used to support the memory management in hypervisor and the extended page-table mechanism for VMs.
 * It can also be used when hypervisor accesses the 4-KByte aligned memory region whose size is a multiple of 4-KByte.
 *
 * @consistency N/A
 * @alignment 4096
 *
 * @remark N/A
 */
struct page {
	/**
	 * @brief A 4-KByte page in the memory.
	 */
	uint8_t contents[PAGE_SIZE];
} __aligned(PAGE_SIZE);

/**
 * @brief Data structure that contains the information of paging structures.
 *
 * It is used to support the memory management in hypervisor and the extended page-table mechanism for VMs.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
union pgtable_pages_info {
	/**
	 * @brief Data structure that contains the information of primary paging structures.
	 *
	 * These primary paging structures are used to support the memory management in hypervisor.
	 *
	 * @consistency N/A
	 * @alignment N/A
	 *
	 * @remark N/A
	 */
	struct {
		/**
		 * @brief A pointer to the PML4 tables to be used by the hypervisor.
		 */
		struct page *pml4_base;
		/**
		 * @brief A pointer to the page-directory-pointer tables to be used by the hypervisor.
		 */
		struct page *pdpt_base;
		/**
		 * @brief A pointer to the page directories to be used by the hypervisor.
		 */
		struct page *pd_base;
		/**
		 * @brief A pointer to the page tables to be used by the hypervisor.
		 */
		struct page *pt_base;
	} ppt;

	/**
	 * @brief Data structure that contains the information of EPT paging structures.
	 *
	 * These EPT paging structures are used to support the extended page-table mechanism for VMs.
	 *
	 * @consistency N/A
	 * @alignment N/A
	 *
	 * @remark N/A
	 */
	struct {
		/**
		 * @brief A pointer to the PML4 tables to be used for EPT.
		 */
		struct page *nworld_pml4_base;
		/**
		 * @brief A pointer to the page-directory-pointer tables to be used for EPT.
		 */
		struct page *nworld_pdpt_base;
		/**
		 * @brief A pointer to the page directories to be used for EPT.
		 */
		struct page *nworld_pd_base;
		/**
		 * @brief A pointer to the page tables to be used for EPT.
		 */
		struct page *nworld_pt_base;
	} ept;
};

/**
 * @brief Data structure that contains the information to be used for paging operations.
 *
 * It includes the following information:
 * the information of paging structures, the callback functions to access these paging structures, the callback
 * function to flush caches, the flag to indicate whether the large pages (1-GByte or 2-MByte) are allowed to be used,
 * and the callback function to tweak and recover execute permission.
 *
 * There is one dedicated instance to support the memory management in hypervisor.
 * Hypervisor also allocates the unique instance for each VM to support EPT.
 *
 * It is supposed to be used when paging operations are conducted.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct memory_ops {
	/**
	 * @brief A pointer to the data structure that contains the information of paging structures.
	 */
	union pgtable_pages_info *info;

	/**
	 * @brief A boolean value indicating whether the large pages (1-GByte or 2-MByte) are allowed to be used.
	 */
	bool large_page_enabled;

	/**
	 * @brief A function pointer to get the default access right of the paging-structure entry.
	 */
	uint64_t (*get_default_access_right)(void);

	/**
	 * @brief A function pointer to check whether the specified paging-structure entry is present or not.
	 */
	uint64_t (*pgentry_present)(uint64_t pte);

	/**
	 * @brief A function pointer to get the to-be-used PML4 table based on the given information.
	 */
	struct page *(*get_pml4_page)(const union pgtable_pages_info *info);

	/**
	 * @brief A function pointer to get the to-be-used page-directory-pointer table based on the given information.
	 */
	struct page *(*get_pdpt_page)(const union pgtable_pages_info *info, uint64_t gpa);

	/**
	 * @brief A function pointer to get the to-be-used page directory based on the given information.
	 */
	struct page *(*get_pd_page)(const union pgtable_pages_info *info, uint64_t gpa);

	/**
	 * @brief A function pointer to get the to-be-used page table based on the given information.
	 */
	struct page *(*get_pt_page)(const union pgtable_pages_info *info, uint64_t gpa);

	/**
	 * @brief A function pointer to flush the cache line that contains the specified paging-structure entry
	 *        (when it is applicable).
	 *
	 * Operations only apply to EPT paging-structure entries.
	 * There is no operation needed to be conducted for the paging-structure entries used by the hypervisor.
	 */
	void (*clflush_pagewalk)(const void *entry);

	/**
	 * @brief A function pointer to tweak the execute permission.
	 *
	 * When the physical platform is vulnerable to the page size change MCE issue, execute access control is
	 * cleared on the EPT paging-structure entry that map a 1-GByte page or a 2-MByte page.
	 *
	 * There is no operation needed to be conducted for all the other cases.
	 */
	void (*tweak_exe_right)(uint64_t *prot);

	/**
	 * @brief A function pointer to recover the execute permission.
	 *
	 * When the physical platform is vulnerable to the page size change MCE issue, execute access control is
	 * set on the EPT PTE that is split from a large page due to EPT violation VM exit caused by the
	 * instruction fetch from guest software.
	 *
	 * There is no operation needed to be conducted for all the other cases.
	 */
	void (*recover_exe_right)(uint64_t *prot);
};

extern const struct memory_ops ppt_mem_ops;
void init_ept_mem_ops(struct memory_ops *mem_ops, uint16_t vm_id, bool enforce_4k_ipage);

/**
 * @}
 */

#endif /* PAGE_H */
