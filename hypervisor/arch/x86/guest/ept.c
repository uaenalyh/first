/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <acrn_hv_defs.h>
#include <vm.h>
#include <mmu.h>
#include <ept.h>
#include <logmsg.h>
#include <trace.h>

/**
 * @addtogroup vp-base_guest_mem
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs related to EPT operations.
 *
 * This file implements all external functions to add, modify and delete EPT entries.
 * In addition, it defines a function to walk through EPT and a helper function used
 * by another file in vp-base.guest_mem.
 *
 * Helper function includes: get_ept_entry.
 */

/**
 * @brief A flag which indicates the default log level for EPT is 6.
 */
#define ACRN_DBG_EPT 6U

/**
 * @brief A helper function to retrieve the corresponding \a vm's EPT structure.
 *
 * @param[in] vm Pointer to the VM whose address of EPT is returned.
 *
 * @return The corresponding VM's normal world EPT structure.
 *
 * @pre vm != NULL
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
void *get_ept_entry(struct acrn_vm *vm)
{
	/** Return the VM's normal world EPT structure */
	return vm->arch_vm.nworld_eptp;
}

/**
 * @brief Clean up a VM's EPT.
 *
 * @param[in] vm Pointer to the VM whose EPT is cleared.
 *
 * @return None
 *
 * @pre vm != NULL
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
void destroy_ept(struct acrn_vm *vm)
{
	/** If VM's normal world EPT is valid */
	if (vm->arch_vm.nworld_eptp != NULL) {
		/** Call memset with following parameters in order to set PAGE_SIZE bytes
		 *  starting from 'vm->arch_vm.nworld_eptp' to 0.
		 *  - vm->arch_vm.nworld_eptp
		 *  - 0
		 *  - PAGE_SIZE
		 */
		(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
	}
}

/**
 * @brief Add EPT entries for guest memory mapping.
 *
 * This function will create one or more entries from the VM's EPT,
 * so that the guest memory region could map to a host physical memory
 * region in order to manipulate that memory.
 *
 * @param[in] vm Pointer to the VM to which the guest memory mapping is added.
 * @param[in] pml4_page The host virtual address of the EPT.
 * @param[in] hpa The specified start host physical address that the guest memory region will be mapped to.
 * @param[in] gpa The specified start physical address of the guest memory region.
 * @param[in] size The size of guest memory region.
 * @param[in] prot_orig The specified memory access right and memory type.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre pml4_page != NULL
 * @pre size > 0
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
void ept_add_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the loop counter as vCPU index, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing an online vCPU of the given VM, not initialized. */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variables of type uint64_t.
	 *  - prot representing the memory access right and memory type, initialized as \a prot_orig. */
	uint64_t prot = prot_orig;

	/** Logging the following information with a log level of ACRN_DBG_EPT.
	 * - __func__
	 * - vm->vm_id
	 * - hpa
	 * - gpa
	 * - size
	 * - prot
	 */
	dev_dbg(ACRN_DBG_EPT, "%s, vm[%d] hpa: 0x%016lx gpa: 0x%016lx size: 0x%016lx prot: 0x%016x\n", __func__,
		vm->vm_id, hpa, gpa, size, prot);

	/** If the desired page is cachable */
	if (((prot & EPT_MT_MASK) != EPT_UNCACHED)) {
		/** Set SNP bit to force snooping of PCIe devices */
		prot |= EPT_SNOOP_CTRL;
	}
	/** Call mmu_add with following parameters in order to add table entry in the EPT.
	 *  - pml4_page
	 *  - hpa
	 *  - gpa
	 *  - size
	 *  - prot
	 *  - &vm->arch_vm.ept_mem_ops
	 */
	mmu_add(pml4_page, hpa, gpa, size, prot, &vm->arch_vm.ept_mem_ops);

	/** For each vcpu in the online vCPUs of vm, using i as the loop counter. */
	foreach_vcpu (i, vm, vcpu) {
		/** Call vcpu_make_request with following parameters in order to notify the vCPU to flush its TLB.
		 *  - vcpu
		 *  - ACRN_REQUEST_EPT_FLUSH
		 */
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

/**
 * @brief Modify the access rights and memory types of existing EPT entries.
 *
 * This function will modify memory access right in the VM's EPT in order to
 * change its access right or memory type.
 *
 * @param[in] vm Pointer to the VM to which the guest memory mapping is modified.
 * @param[in] pml4_page The host virtual address of the EPT.
 * @param[in] gpa The specified start physical address of the guest memory region.
 * @param[in] size The size of guest memory region.
 * @param[in] prot_set The memory access right and memory type to set.
 * @param[in] prot_clr The memory access right and memory type to clear.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre pml4_page != NULL
 * @pre size > 0
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
void ept_modify_mr(
	struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size, uint64_t prot_set, uint64_t prot_clr)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing an online vCPU of the given VM, not initialized. */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the loop counter as vCPU index, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type uint64_t.
	 *  - local_prot representing the memory access right and memory type, initialized as \a prot_set. */
	uint64_t local_prot = prot_set;

	/** Logging the following information with a log level of ACRN_DBG_EPT.
	 * - __func__
	 * - vm->vm_id
	 * - gpa
	 * - size
	 */
	dev_dbg(ACRN_DBG_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	/** If the desired page is cachable */
	if (((local_prot & EPT_MT_MASK) != EPT_UNCACHED)) {
		/** Set SNP bit to force snooping of PCIe devices */
		local_prot |= EPT_SNOOP_CTRL;
	}

	/** Call mmu_modify_or_del with following parameters in order to change guest memory
	 *  region's access rights and type.
	 *  - pml4_page
	 *  - gpa
	 *  - size
	 *  - local_prot
	 *  - prot_clr
	 *  - &vm->arch_vm.ept_mem_ops
	 *  - MR_MODIFY
	 */
	mmu_modify_or_del(pml4_page, gpa, size, local_prot, prot_clr, &(vm->arch_vm.ept_mem_ops), MR_MODIFY);

	/** For each vcpu in the online vCPUs of vm, using i as the loop counter. */
	foreach_vcpu (i, vm, vcpu) {
		/** Call vcpu_make_request with following parameters in order to notify the vCPU to flush its TLB.
		 *  - vcpu
		 *  - ACRN_REQUEST_EPT_FLUSH
		 */
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

/**
 * @brief Delete EPT entries from guest memory mapping.
 *
 * This function will delete one or more entries from the VM's EPT,
 * so that the guest memory region will no longer map a physical memory region.
 *
 * @param[in] vm Pointer to the VM to which the guest memory mapping is deleted.
 * @param[in] pml4_page The host virtual address of the EPT.
 * @param[in] gpa The specified start physical address of the guest memory region.
 * @param[in] size The size of guest memory region.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre pml4_page != NULL
 * @pre size > 0
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
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
void ept_del_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing an online vCPU of the given VM, not initialized. */
	struct acrn_vcpu *vcpu;
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the loop counter as vCPU index, not initialized. */
	uint16_t i;

	/** Logging the following information with a log level of ACRN_DBG_EPT.
	 * - __func__
	 * - vm->vm_id
	 * - gpa
	 * - size
	 */
	dev_dbg(ACRN_DBG_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	/** Call mmu_modify_or_del with following parameters in order to delete guest memory region's mapping.
	 *  - pml4_page
	 *  - gpa
	 *  - size
	 *  - 0
	 *  - 0
	 *  - &vm->arch_vm.ept_mem_ops
	 *  - MR_DEL
	 */
	mmu_modify_or_del(pml4_page, gpa, size, 0UL, 0UL, &vm->arch_vm.ept_mem_ops, MR_DEL);

	/** For each vcpu in the online vCPUs of vm, using i as the loop counter. */
	foreach_vcpu (i, vm, vcpu) {
		/** Call vcpu_make_request with following parameters in order to notify the vCPU to flush its TLB.
		 *  - vcpu
		 *  - ACRN_REQUEST_EPT_FLUSH
		 */
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

/**
 * @brief Flush cache for a given guest memory page.
 *
 * This function is a callback to flush cache for each guest memory region.
 *
 * @param[in] pge The pointer that points to a leaf page entry.
 * @param[in] size The size of guest memory region.
 *
 * @return None
 *
 * @pre pge != NULL
 * @pre size > 0
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
void ept_flush_leaf_page(uint64_t *pge, uint64_t size)
{
	/** Declare the following local variables of type uint64_t.
	 *  - hpa representing the corresponding start host physical memory address
	 *    of the guest memory region, initialized as INVALID_HPA. */
	uint64_t hpa = INVALID_HPA;
	/** Declare the following local variables of type void *.
	 *  - hva representing the corresponding start host virtual memory address
	 *    of the guest memory region, initialized as NULL. */
	void *hva = NULL;

	/** If the guest memory region specified by \a pge is cachable. */
	if ((*pge & EPT_MT_MASK) != EPT_UNCACHED) {
		/** Set hpa to the content of the page table entry with the access right bits cleared. */
		hpa = (*pge & (~(size - 1UL)));
		/** Call hpa2hva with following parameters to get corresponding host
		 *  physical address and set its return value to 'hva'.
		 *  - hpa
		 */
		hva = hpa2hva(hpa);
		/** Call stac to allow explicit supervisor-mode accesses to user-mode pages */
		stac();
		/** Call flush_address_space with following parameters to flush cache for the given memory region.
		 *  - hva
		 *  - size
		 */
		flush_address_space(hva, size);
		/** Call clac to disallow explicit supervisor-mode accesses to user-mode pages */
		clac();
	}
}

/**
 * @brief Walk through all the EPT entries for a given VM.
 *
 * This function walks through all the EPT entry for a given VM specified by \a vm and an action will be
 * performed on each of them specified by \a cb.
 *
 * @param[in] vm Pointer to the VM whose EPT is walked through.
 * @param[in] cb The callback for each EPT entry.
 *
 * @return None
 *
 * @pre vm != NULL
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
void walk_ept_table(struct acrn_vm *vm, pge_handler cb)
{
	/** Declare the following local variables of type const struct memory_ops *.
	 *  - mem_ops representing the operations for the EPT, initialized as &vm->arch_vm.ept_mem_ops. */
	const struct memory_ops *mem_ops = &vm->arch_vm.ept_mem_ops;
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

	/** For each i ranging from 0 to PTRS_PER_PML4E, in order to loop through all the PML4 Table entries. */
	for (i = 0UL; i < PTRS_PER_PML4E; i++) {
		/** Call get_ept_entry with following parameters to get EPT structure.
		 *  - vm
		 *  Then call pml4e_offset with following parameters to get PML4 table entry and
		 *  set its value to 'pml4e'.
		 *  - The EPT structure returned by get_ept_entry
		 *  - i << PML4E_SHIFT
		 */
		pml4e = pml4e_offset((uint64_t *)get_ept_entry(vm), i << PML4E_SHIFT);
		/** Call mem_ops->pgentry_present with following parameters to check whether the
		 *  corresponding Page-Directory-Pointer Table is present.
		 *  - *pml4e
		 *  If the Page-Directory-Pointer table isn't present.
		 */
		if (mem_ops->pgentry_present(*pml4e) == 0UL) {
			/** Continue this loop */
			continue;
		}
		/** For each j ranging from 0 to PTRS_PER_PDPTE, in order to loop through all
		 *  the Page-directory-pointer table entries.
		 */
		for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
			/**  Call pdpte_offset with following parameters to get Page-Directory-Pointer Table entry
			 *   and set return its value to 'pdpte'.
			 *  - pml4e
			 *  - j << PDPTE_SHIFT
			 */
			pdpte = pdpte_offset(pml4e, j << PDPTE_SHIFT);
			/** Call mem_ops->pgentry_present with following parameters to check whether the
			 *  corresponding table entry is present.
			 *  - *pdpte
			 *  If the table entry isn't present.
			 */
			if (mem_ops->pgentry_present(*pdpte) == 0UL) {
				/** Continue this loop */
				continue;
			}
			/** Call pdpte_large with following parameters to check whether the
			 *  corresponding table entry is a 1GB page entry.
			 *  - *pdpte
			 *  If it is a 1GB page entry.
			 */
			if (pdpte_large(*pdpte) != 0UL) {
				/** Call \a cb with following parameters to perform an action for this page entry.
				 *  - pdpte
				 *  - PDPTE_SIZE
				 */
				cb(pdpte, PDPTE_SIZE);
				/** Continue this loop */
				continue;
			}
			/** For each k ranging from 0 to PTRS_PER_PDE, in order to loop
			 *  through all the Page Directory entries.
			 */
			for (k = 0UL; k < PTRS_PER_PDE; k++) {
				/** Call pde_offset with following parameters to get Page
				 *  directory entry and set its return value to 'pde'.
				 *  - pdpte
				 *  - k << PDE_SHIFT
				 */
				pde = pde_offset(pdpte, k << PDE_SHIFT);
				/** Call mem_ops->pgentry_present with following parameters to check whether the
				 *  corresponding Page directory entry is present.
				 *  - *pde
				 *  If the table entry isn't present.
				 */
				if (mem_ops->pgentry_present(*pde) == 0UL) {
					/** Continue this loop */
					continue;
				}
				/** Call pde_large with following parameters to check whether the
				 *  corresponding entry is a 2MB page entry.
				 *  - *pde
				 *  If it is a 2MB page entry.
				 */
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
				/** For each m ranging from 0 to PTRS_PER_PTE, in order to
				 *  loop through all the Page Table entries.
				 */
				for (m = 0UL; m < PTRS_PER_PTE; m++) {
					/** Call pte_offset with following parameters to get
					 *  Page Table entry and set its return value to 'pte'.
					 *  - pde
					 *  - m << PTE_SHIFT
					 */
					pte = pte_offset(pde, m << PTE_SHIFT);
					/** Call mem_ops->pgentry_present with following parameters to check whether the
					 *  corresponding Page Table entry is present.
					 *  - *pte
					 *  If the table entry isn't present.
					 */
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
