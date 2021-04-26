/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <guest_memory.h>
#include <vm.h>
#include <mmu.h>
#include <ept.h>
#include <logmsg.h>

/**
 * @defgroup vp-base_guest-mem vp-base.guest-mem
 * @ingroup vp-base
 * @brief This module implements all the operations related to guest memory access and address translation.

 * The implemented operations contain copy between guest and host and guest memory address translation
 * which is done by EPT.
 *
 * Usage:
 * - 'vp-base.vcr' depends on this module to load PDPTRs.
 * - 'vp-base.vm' depends on this module to do following things:
 *   - Build RSDP, XSDT and MADT tables.
 *   - Copy kernel image and boot arguments.
 *   - Get host virtual addresses of kernel and zero-page.
 *   - Clean up EPT when shutting down a VM.
 *   - Map physical memory used by pre-launched VMs.
 * - 'vp-base.vcpu' depends on this module to initialize GDT register.
 * - 'vp-dm.vperipheral' depends on this module to map and unmap physical BAR for passthrough PCI devices.
 * - 'vp-dm.io_req' depends on this module to add executable access right to the specific memory region.
 * - 'vp-base.hv_main' depends on this module to flush page cache when wbinvd VMExit happens.
 *
 * Dependency:
 * - This module depends on 'lib.util' to operate on the host memory.
 * - This module depends on 'vp-base.vm' to get virtual CPU structure pointer.
 * - This module depends on 'hwmgmt.cpu' to do following things:
 *   - Access guest's memory protected by SMAP.
 * - This module depends on 'hwmgmt.page' to do following things:
 *   - Get physical page table entry.
 *   - Translate host physical address to host virtual address.
 *   - Add, modify and delete page table entries.
 * - This module depends on 'vp-base.virq' to notify vCPU to flush its TLB.
 * - This module depends on 'hwmgmt.mmu' to flush cache for a given memory region.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external functions related to guest memory copy and memory address
 * translation from guest to host. It also defines some helper functions to implement the features
 * that are commonly used in this file.
 *
 * Helper functions include: local_gpa2hpa, local_copy_gpa, copy_gpa.
 */

/**
 * @brief This helper function is for translating from guest physical address to host physical address
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[in] gpa The specified guest physical address
 * @param[out] size The pointer that returns the page size of the page in which the \a gpa is.
 *
 * @return The host physical address mapping to the \p gpa or INVALID_HPA if \a gpa is unmapping.
 *
 * @pre vm != NULL

 * @post *size == PTE_SIZE || *size == PDE_SIZE || *size == PDPTE_SIZE
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size)
{
	/** Declare the following local variables of type uint64_t.
	 *  - hpa representing the returned host physical address, initialized as INVALID_HPA.
	 *  - pg_size representing the detected page size, initialized as 0. */
	uint64_t hpa = INVALID_HPA, pg_size = 0UL;
	/** Declare the following local variables of type const uint64_t *.
	 *  - pgentry representing the pointer to the found page table entry, not initialized. */
	const uint64_t *pgentry;
	/** Declare the following local variables of type const void *.
	 *  - eptp representing the pointer to the EPT of the given VM, not initialized. */
	void *eptp;

	/** Call get_ept_entry with the following parameters, in order to
	 *  get the corresponding EPT according to \a vm and set its return value to 'eptp'.
	 *  - \a vm.
	 */
	eptp = get_ept_entry(vm);
	/** Call lookup_address with the following parameters, in order to
	 *  get the corresponding EPT entry and page size according to \a gpa
	 *  and set its return value to 'pgentry'.
	 *  - eptp.
	 *  - \a gpa.
	 *  - &pg_size.
	 *  - &vm->arch_vm.ept_mem_ops
	 */
	pgentry = lookup_address((uint64_t *)eptp, gpa, &pg_size, &vm->arch_vm.ept_mem_ops);
	/** If the corresponding EPT entry is found */
	if (pgentry != NULL) {
		/** Set 'hpa' to the result of a bitwise-OR of the page frame number specified in 'pgentry'
		 *  and the offset within the page frame in 'gpa'. */
		hpa = (((*pgentry & (~EPT_PFN_HIGH_MASK)) & (~(pg_size - 1UL))) | (gpa & (pg_size - 1UL)));
	}

	/** If specified \a size is not NULL and the host physical address is found, */
	if ((size != NULL) && (hpa != INVALID_HPA)) {
		/** Set the 32-bit integer pointed to by \a size to 'pg_size' which indicates the detected page size. */
		*size = (uint32_t)pg_size;
	}

	/** Return the corresponding host physical address */
	return hpa;
}

/**
 * @brief This helper function is for page-sized copying between host memory and guest memory.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[inout] h_ptr The virtial address in the host memory region.
 * @param[in] gpa The guest physical memory address.
 * @param[in] size The size of the copied region.
 * @param[in] fix_pg_size The specified page size, if set as 0, page size will be detected according to the page table.
 * @param[in] cp_from_vm Indicates whether copy operation is from guest memory or into.
 *
 * @return The actual copy size in byte.
 *
 * @pre vm != NULL
 * @pre h_ptr != NULL
 * @pre size != 0
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint32_t local_copy_gpa(
	struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size, uint32_t fix_pg_size, bool cp_from_vm)
{
	/** Declare the following local variable of type uint64_t
	 *  - 'hpa' representing host physical memory address translated from \a gpa, not initialized. */
	uint64_t hpa;
	/** Declare the following local variables of type uint32_t
	 *  - 'offset_in_pg' representing offset of \a gpa within the page frame, not initialized.
	 *  - 'len' representing the actual copy size in byte, not initialized.
	 *  - 'pg_size' representing the actual page size, either specified by \a fix_pg_size or detected,
	 *     not initialized.
	 */
	uint32_t offset_in_pg, len, pg_size;
	/** Declare the following local variable of type void *
	 *  - 'g_ptr' representing host virtual memory address paired with \a gpa, not initialized. */
	void *g_ptr;

	/** Call local_copy_gpa with the following parameters, in order to
	 *  get corresponding host physical address according to \a gpa.
	 *  Also get actual page size of that page entry and set its return value to 'hpa'.
	 *  - \a vm
	 *  - \a gpa
	 *  - &pg_size
	 */
	hpa = local_gpa2hpa(vm, gpa, &pg_size);
	/** If the corresponding host physical address is invalid */
	if (hpa == INVALID_HPA) {
		/** Print an error message to show vm ID and \a gpa */
		pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping", __func__, vm->vm_id, gpa);
		/** Set the actual copy size to 0 */
		len = 0U;
	/** If the corresponding host physical address is valid */
	} else {
		/** If page size is specified by \a fix_pg_size which is not 0 */
		if (fix_pg_size != 0U) {
			/** Override the detected page size with \a fix_pg_size */
			pg_size = fix_pg_size;
		}
		/** Calculate the \a gpa's offset within a page */
		offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
		/** Set 'len' to the smaller value between size and ('pg_size' - 'offset_in_pg'),
		 *  in order to reduce the copy size to fit within a page if the specified copy
		 *  region crosses the page boundary.
		 */
		len = (size > (pg_size - offset_in_pg)) ? (pg_size - offset_in_pg) : size;

		/** Call hpa2hva with the following parameters, in order to
		 *  get corresponding host virtual address and set its return value to 'g_ptr'.
		 *  - hpa
		 */
		g_ptr = hpa2hva(hpa);

		/** Call stac to allow explicit supervisor-mode accesses to user-mode pages */
		stac();
		/** If 'cp_from_vm' is true, meanning we want to copy from guest memory into host memory */
		if (cp_from_vm) {
			/** Call memcpy_s with following parameters in order to
			 *  copy memory content from guest memory into host memory.
			 *  - \a h_ptr
			 *  - len
			 *  - g_ptr
			 *  - len
			 */
			(void)memcpy_s(h_ptr, len, g_ptr, len);
		/** If we want to copy from host memory into guest memory */
		} else {
			/** Call memcpy_s with following parameters in order to
			 *  copy memory content from host memory into guest memory.
			 *  - \a h_ptr
			 *  - len
			 *  - g_ptr
			 *  - len
			 */
			(void)memcpy_s(g_ptr, len, h_ptr, len);
		}
		/** Call clac to disallow explicit supervisor-mode accesses to user-mode pages */
		clac();
	}

	/** Return the actual copy size in byte. */
	return len;
}

/**
 * @brief This helper function is for arbitrary sized copying between host memory and guest memory.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[inout] h_ptr_arg The virtial address in the host memory region.
 * @param[in] gpa_arg The guest physical memory address.
 * @param[in] size_arg The size of the copied region.
 * @param[in] cp_from_vm Indicates whether copy operation is from guest memory or vice versa.
 *
 * @return 0 if no error happens otherwise return -EINVAL.
 *
 * @pre vm != NULL
 * @pre h_ptr_arg != NULL
 * @pre size_arg != 0
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline int32_t copy_gpa(
	struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg, uint32_t size_arg, bool cp_from_vm)
{
	/** Declare the following local variable of type void *
	 *  - 'h_ptr' representing the start of the next host virtual memory address to operate on,
	 *     initialized as value of \a h_ptr_arg. */
	void *h_ptr = h_ptr_arg;
	/** Declare the following local variable of type uint64_t
	 *  - 'gpa' representing the start of the next guest physical memory address to operate on,
	 *    initialized as value of \a gpa_arg. */
	uint64_t gpa = gpa_arg;
	/** Declare the following local variable of type uint32_t
	 *  - 'size' representing the remaining size of copied region, initialized as value of \a size_arg.
	 *  - 'len' representing the actual copy size in byte for each iteration, not initialized. */
	uint32_t size = size_arg, len;
	/** Declare the following local variable of type int32_t
	 *  - 'err' representing return value, initialized as 0 . */
	int32_t err = 0;

	/** Loop until the remaining copy size reaches 0 */
	while (size > 0U) {
		/** Call local_copy_gpa with the following parameters, in order to
		 *  copy at most one page data and set its return value to 'len'.
		 *  - \a vm
		 *  - h_ptr
		 *  - gpa
		 *  - size
		 *  - 0
		 *  - \a cp_from_vm
		 */
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		/** If copy size is 0 which indicates an error happened */
		if (len == 0U) {
			/** Set return value as -EINVAL */
			err = -EINVAL;
			/** Break out the loop */
			break;
		}
		/** If no error happened, advance the start of the guest physical memory address
		 *  to operate on by 'len' bytes.
		 */
		gpa += len;
		/** Advance the start of the host virtual memory address to operate on by 'len' bytes. */
		h_ptr += len;
		/** Deduct 'len' bytes from the remaining copy size. */
		size -= len;
	}

	/** Return the return value */
	return err;
}

/**
 * @brief Copy data from guest memory space to host memory space.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[out] h_ptr The pointer that points the start host memory address which data is stored in.
 * @param[in] gpa The start guest memory address which data will be copied from.
 * @param[in] size The size (bytes) of copied region.
 *
 * @return 0 if no error happens otherwise return -EINVAL.
 *
 * @pre vm != NULL
 * @pre hptr != NULL
 * @pre size != 0

 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	/** Call copy_gpa with the following parameters, in order to
	 *  copy data from guest memory to host memory.
	 *  - \a vm
	 *  - \a h_ptr
	 *  - \a gpa
	 *  - \a size
	 *  - true
	 */
	return copy_gpa(vm, h_ptr, gpa, size, true);
}
/**
 * @brief Copy data from host memory space to guest memory space.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[in] h_ptr The pointer that points the start host memory address which data is copied from.
 * @param[in] gpa The start guest memory address which data will be stored into.
 * @param[in] size The size (bytes) of copied region.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre hptr != NULL
 * @pre size != 0

 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	/** Call copy_gpa with the following parameters, in order to
	 *  copy data from host memory to guest memory, and discard its return value.
	 *  - \a vm
	 *  - \a h_ptr
	 *  - \a gpa
	 *  - \a size
	 *  - false
	 */
	(void)copy_gpa(vm, h_ptr, gpa, size, false);
}

/**
 * @brief Translate guest physical memory address to host virtual memory address.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[in] x The guest physical memory address.
 *
 * @return The host virtual address that can be used to access the physical memory.
 *
 * @pre vm != NULL

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{

	/** Declare the following local variables of type uint64_t.
	 *  - hpa representing the returned host physical address,
	 *  initialized as return value of gpa2hpa with vm and x as parameter. */
	uint64_t hpa = gpa2hpa(vm, x);
	/** Return NULL if hpa is INVALID_HPA,
	 *  else return the return value of hpa2hva with hpa as parameter. */
	return (hpa == INVALID_HPA) ? NULL : hpa2hva(hpa);
}

/**
 * @brief Translate guest physical memory address to host physical memory address.
 *
 * @param[in] vm The pointer to the virtual machine structure which the guest memory belongs to.
 * @param[in] gpa The guest physical memory address.
 *
 * @return The host physical address mapping to the \a gpa or INVALID_HPA if it's unmapping.
 *
 * @pre vm != NULL

 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa)
{
	/** Call local_gpa2hpa with the following parameters, in order to
	 *  get corresponding host physical memory address and return its return value.
	 *  - \a vm
	 *  - \a gpa
	 *  - NULL
	 */
	return local_gpa2hpa(vm, gpa, NULL);
}

/**
 * @}
 */
