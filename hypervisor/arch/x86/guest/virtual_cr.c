/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains vmcs operations which is vcpu related
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <irq.h>
#include <mmu.h>
#include <vcpu.h>
#include <vm.h>
#include <vmx.h>
#include <vtd.h>
#include <vmexit.h>
#include <pgtable.h>
#include <trace.h>
#include <logmsg.h>
#include <virq.h>

/**
 * @defgroup vp-base_vcr vp-base.vcr
 * @ingroup vp-base
 * @brief The defination and implementation of virtual control registers (VCRs) related functions and APIs
 *
 * The guest's control registers (VCRs) are emulated by the hypervisor. This module provides
 * defination and implementation of virtual control registers (VCRs) related functions and
 * APIs.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by the vp-base.vcr module.
 *
 * This file provides implementation of public APIs for VCRs interception configuration, public APIs
 * for getting or setting VCRs inside the hypervisor, VM-Exit entry function caused by set to
 * CR0/CR4 as well as private helper functions used by these public APIs.
 *
 * Public APIs:
 *   - init_cr0_cr4_host_mask: public API for VCRs interception configuration.
 *   - vcpu_get_cr0: public API for get the value of virtual CR0.
 *   - vcpu_set_cr0: public API for set the value of virtual CR0.
 *   - vcpu_set_cr2: public API for set the value of virtual CR2.
 *   - vcpu_get_cr4: public API for get the value of virutal CR4.
 *   - vcpu_set_cr4: public API for set the value of virutal CR4.
 *   - cr_access_vmexit_handler: VM-Exit entry function when setting to virtual CR0/CR4.
 *
 * Private helper functions:
 *   - is_cr0_write_valid: helper function for checking the validity of CR0 write operation.
 *   - is_cr4_write_valid: helper function for checking the validity of CR4 write operation.
 *   - vmx_write_cr0: helper function for virtual CR0 setting, the public API vcpu_set_cr0
 *                    actually call this function to do the set operation.
 *   - vmx_write_cr4: helper function for virtual CR4 setting, the public API vcpu_set_cr4
 *                    actually call this function to do the set operation.
 *   - load_pdptrs: helper function for reloading PDPTRs.
 */

/**
 * @brief CR0 bits hypervisor wants to trap to track the status change.
 *
 * The hypervisor will trap CR0.PE, CR.PG, CR0.WP, CR0.CD, CR0.NW to track the status change.
 * This macro marks these bits.
 */
#define CR0_TRAP_MASK (CR0_PE | CR0_PG | CR0_WP | CR0_CD | CR0_NW)
/**
 * @brief These CR0 bits are reserved according to the SDM and shall not be changed by the guests.
 *
 * The CR0 bits are reserved if they don't belongs to any bit of CR0.PG, CR0.CD, CR0.NW, CR0.AM,
 * CRO.WP, CRO.NE, CRO.ET, CRO.TS CR0.EM. CRO.MP or CR0.PE. This macro marks these bits.
 */
#define CR0_RESERVED_MASK \
	~(CR0_PG | CR0_CD | CR0_NW | CR0_AM | CR0_WP | CR0_NE | CR0_ET | CR0_TS | CR0_EM | CR0_MP | CR0_PE)

/**
 * @brief CR4 bits hypervisor wants to trap to track status change.
 *
 * The hypervisor will trap CR4.PSE, CR4.PAE, CR4.VMXE, CR4.PCIDE, CR4.SMEP, CR4.SMAP, CR4.PKE, CR4.SMXE, CR4.DE,
 * CR4.MCE, CR4_PCE, CR4_VME, CR4_PVI to track the status change. This macro marks these bits.
 */
#define CR4_TRAP_MASK \
	(CR4_PSE | CR4_PAE | CR4_VMXE | CR4_PCIDE | CR4_SMEP | CR4_SMAP | CR4_PKE | CR4_SMXE | \
	 CR4_DE | CR4_MCE | CR4_PCE | CR4_VME | CR4_PVI)
/**
 * @brief These CR4 bits are reserved according to the SDM and shall not be changed by the guests.
 *
 * The CR4 bits are reserved if they don't belongs to any bit of CR4.VME, CR4.PVI, CR4.TSD,CR4.DE
 * CR4.PSE, CR4.PAE, CR4.MCE, CR4.PGE, CR4.PCE, CR4.OSFXSR, CR4.PCIDE, CR4.OSXSAVE, CR4.SMEP,
 * CR4.FSGSBASE, CR4.VMXE, CR4.OSXMMEXCPT, CR4.SMAP, CR4.PKE, CR4.SMXE or CR4.UMIP. This macro marks
 * these bits.
 */
#define CR4_RESERVED_MASK                                                                                            \
	~(CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_PAE | CR4_MCE | CR4_PGE | CR4_PCE | CR4_OSFXSR |      \
		CR4_PCIDE | CR4_OSXSAVE | CR4_SMEP | CR4_FSGSBASE | CR4_VMXE | CR4_OSXMMEXCPT | CR4_SMAP | CR4_PKE | \
		CR4_SMXE | CR4_UMIP)

/**
 * @brief PAE PDPTE bits 1 ~ 2, 5 ~ 8 are always reserved. This macro marks these bits.
 */
#define PAE_PDPTE_FIXED_RESVD_BITS 0x00000000000001E6UL

/**
 * @brief the mask for CR0 bits which shall be always set to 1.
 *
 * Generally speaking, the reference value is hinted by the value of MSR_IA32_VMX_CR0_FIXED0.
 * Currently, the actual value is initialized as (value of MSR_IA32_VMX_CR0_FIXED0) & (~(CR0_PE | CR0_PG)).
 */
static uint64_t cr0_always_on_mask;
/**
 * @brief the mask for CR0 bits which shall be always set to 0.
 *
 * Generally speaking, the reference value is hinted by the value of MSR_IA32_VMX_CR0_FIXED1.
 * The value is initialized as ~(value of MSR_IA32_VMX_CR0_FIXED1).
 */
static uint64_t cr0_always_off_mask;
/**
 * @brief the mask for CR4 bits which shall be always set to 1.
 *
 * Generally speaking, the reference value is hinted by the value of MSR_IA32_VMX_CR4_FIXED0.
 * The value is initialized as (value of MSR_IA32_VMX_CR4_FIXED0).
 */
static uint64_t cr4_always_on_mask;
/**
 * @brief the mask for CR4 bits which shall be always set to 0.
 *
 * Generally speaking, the reference value is hinted by the value of MSR_IA32_VMX_CR4_FIXED1.
 * The value is is initialized as ~(value of MSR_IA32_VMX_CR4_FIXED1).
 */
static uint64_t cr4_always_off_mask;

/**
 * @brief Helper function for reloading PDPTRs.
 *
 * This function checks the validity of the address area pointed by the guest vCPU's cr3 and
 * reload the PDPTRs for the guests if there are no errors.
 *
 * @param[inout] vcpu pointer to vcpu data structure whose virtual PDPTRs are to be reloaded.
 *
 * @return An error code indicating if the operation succeeds or not.
 *
 * @retval 0 No error happened
 * @retval -EFAULT Loading guest PDPTR fails
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
static int32_t load_pdptrs(const struct acrn_vcpu *vcpu)
{
	/** Declare the following local variable of type uint64_t
	 *  - guest_cr3 representing the guest cr3 of the given vcpu, initialized as exec_vmread(VMX_GUEST_CR3) */
	uint64_t guest_cr3 = exec_vmread(VMX_GUEST_CR3);
	/** Declare the following local variable of type 'cpuinfo_x86*'
	 *  - cpu_info representing the physical cpu information, initialized as get_pcpu_info() */
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	/** Declare the following local variable of type int32_t
	 *  - ret representing the return value of this function, initialized as 0 */
	int32_t ret = 0;
	/** Declare the following local variable of type 'uint64_t[]'
	 *  - pdpte[4] representing the four PDPTEs of the given vcpu, not initialized */
	uint64_t pdpte[4];
	/** Declare the following local variable of type uint64_t
	 *  - rsvd_bits_mask representing the mask of PDPTE reserved bits, 1~2, 5~8, maxphyaddr ~ 63,
	 *  not initialized */
	uint64_t rsvd_bits_mask;
	/** Declare the following local variable of type uint8_t
	 *  - maxphyaddr representing the max width of physical addresses, not initialized */
	uint8_t maxphyaddr;
	/** Declare the following local variable of type int32_t
	 *  - i representing the index of PDPTE, not initialized */
	int32_t i;

	/** Copy the full PDPT at the PDPT address in guest CR3 of the given vCPU to pdpte,
	 *  and check if the copy fails. */
	if (copy_from_gpa(vcpu->vm, pdpte, get_pae_pdpt_addr(guest_cr3), sizeof(pdpte)) != 0) {
		/** Set ret to -EFAULT to record the error */
		ret = -EFAULT;
	/** If no copy error happens */
	} else {
		/** Set maxphyaddr to be cpu_info->phys_bits */
		maxphyaddr = cpu_info->phys_bits;
		/** Set rsvd_bits_mask to be ((1UL << (63U - maxphyaddr + 1U)) - 1UL) << maxphyaddr, in order to set bit
		 *  63:maxphyaddr in rsvd_bits_mask to 1 where 63 is the index of the most significant bit in 64-bit
		 *  physical addresses. */
		rsvd_bits_mask = ((1UL << (63U - maxphyaddr + 1U)) - 1UL) << maxphyaddr;
		/** Set rsvd_bits_mask to be (rsvd_bits_mask | PAE_PDPTE_FIXED_RESVD_BITS) */
		rsvd_bits_mask |= PAE_PDPTE_FIXED_RESVD_BITS;
		/** loop for checking each pdpte, i ranging from 0 to 3 */
		for (i = 0; i < 4; i++) {
			/** Check if the ith PDPTE in pdpte sets both the P flag (bit 0) and any reserved bits */
			if (((pdpte[i] & PAGE_PRESENT) != 0UL) && ((pdpte[i] & rsvd_bits_mask) != 0UL)) {
				/** Set ret to -EFAULT to record the error */
				ret = -EFAULT;
				/** go out the loop due to error */
				break;
			}
		}
	}

	/** If no error */
	if (ret == 0) {
		/** write checked pdpte 0 to VMCS */
		exec_vmwrite64(VMX_GUEST_PDPTE0_FULL, pdpte[0]);
		/** write checked pdpte 1 to VMCS */
		exec_vmwrite64(VMX_GUEST_PDPTE1_FULL, pdpte[1]);
		/** write checked pdpte 2 to VMCS */
		exec_vmwrite64(VMX_GUEST_PDPTE2_FULL, pdpte[2]);
		/** write checked pdpte 3 to VMCS */
		exec_vmwrite64(VMX_GUEST_PDPTE3_FULL, pdpte[3]);
	}

	/** Return the result */
	return ret;
}

/**
 * @brief This function checks if writing \a cr0 to the vCPU CR0 of \a vcpu is valid.
 *
 * @param[in] vcpu pointer to the vcpu whose guest CR0 is to be written.
 *
 * @param[in] cr0 the value to be written to CR0
 *
 * @return Whether the given value is valid when written to the given vCPU
 *
 * @retval true \a cr0 is a valid new value for guest CR0
 * @retval false \a cr0 is an invalid new value for guest CR0
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
static bool is_cr0_write_valid(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	/** Declare the following local variable of type bool
	 *  - ret representing the return value of this function, initialized as true */
	bool ret = true;

	/** If the given value has set any bit that must be 0 in guest CR0 */
	if ((cr0 & cr0_always_off_mask) != 0UL) {
		/** Set the value of ret to false */
		ret = false;
	/** If no always off bit is set */
	} else {
		/** If the given vCPU attempts to change guest CR0.PE from 1 to 0 */
		if (((vcpu_get_cr0(vcpu) & CR0_PE) != 0UL) && ((cr0 & CR0_PE) == 0UL)) {
			/** Set the ret to false */
			ret = false;
		/** The corresponding if condition is not meet */
		} else {
			/** If the vCPU has CR0.PG set and it is not in PAE mode but has IA32_EFER.LME set
			 *  We always require "unrestricted guest" control enabled. So CR0.PG = 1, CR4.PAE = 0
			 *  and IA32_EFER.LME = 1 is invalid. CR0.PE = 0 and CR0.PG = 1 is invalid. */
			if (((cr0 & CR0_PG) != 0UL) && (!is_pae(vcpu)) &&
				((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL)) {
				/** Set the ret to false */
				ret = false;
			/** The corresponding if condition is not meet */
			} else {
				/** If the PE bit is off but PG bit is on */
				if (((cr0 & CR0_PE) == 0UL) && ((cr0 & CR0_PG) != 0UL)) {
					/** Set the ret to false */
					ret = false;
				/** The corresponding if condition is not meet */
				} else {
					/** If the vCPU has CR0.NW set but CR0.CD not set, Loading CR0 register with
					 *  a set NW flag and a clear CD flag is invalid */
					if (((cr0 & CR0_CD) == 0UL) && ((cr0 & CR0_NW) != 0UL)) {
						/** Set the ret to false */
						ret = false;
					}
				}
			}
		}

	}

	/** Return the result */
	return ret;
}

/**
 * @brief This function does the write operation to CR0
 *
 * This a helper function for virtual CR0 setting, the public API vcpu_set_cr0
 * actually call this function to do the set operation.
 *
 * Handling of CR0:
 * Assume "unrestricted guest" feature is supported by vmx.
 * For mode switch, hv only needs to take care of enabling/disabling long mode,
 * thanks to "unrestricted guest" feature.
 *
 *   - PE (0)  Trapped to track cpu mode.
 *	     Set the value according to the value from guest.
 *   - MP (1)  Flexible to guest
 *   - EM (2)  Flexible to guest
 *   - TS (3)  Flexible to guest
 *   - ET (4)  Flexible to guest
 *   - NE (5)  must always be 1
 *   - WP (16) Trapped to get if it inhibits supervisor level procedures to
 *	     write into ro-pages.
 *   - AM (18) Flexible to guest
 *   - NW (29) Trapped to emulate cache disable situation
 *   - CD (30) Trapped to emulate cache disable situation
 *   - PG (31) Trapped to track cpu/paging mode.
 *	     Set the value according to the value from guest.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR0 is to be written.
 *
 * @param[in] cr0 the value to be written to CR0
 *
 * @param[in] is_init indicate if the guest CR0 write is for initializing
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
static void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t cr0, bool is_init)
{
	/** Declare the following local variable of type bool
	 *  - err_found representing if error found or not, initialized as false */
	bool err_found = false;

	/** Declare the following local variable of type uint64_t
	 * - cr0_vmx representing the vCPU CR0 value to be set, not initialized */
	uint64_t cr0_vmx;
	/** Declare the following local variable of type uint64_t
	 *  - cr0_mask representing the vCPU CR0 shadow value */
	uint64_t cr0_mask = cr0;

	/** Clear the reserved bits in cr0_mask, When loading a control register,
	*  reserved bit should always set to the value previously read. */
	cr0_mask &= ~CR0_RESERVED_MASK;

	/** If is_init is false. */
	if (is_init == false) {
		/** If the write to CR0 is invalid */
		if (!is_cr0_write_valid(vcpu, cr0)) {
			/** Print a debug message */
			pr_dbg("Invalid cr0 write operation from guest");
			/** Call vcpu_inject_gp to inject \# GP(0) to \a vcpu */
			vcpu_inject_gp(vcpu, 0U);
			/** Set the err_found to be true */
			err_found = true;
		/** If write to CR0 is valid */
		} else {
			/** Declare the following local variable of type uint32_t
			 * - entry_ctrls representing the vCPU VMX_ENTRY_CONTROLS value in VMCS, not initialized */
			uint32_t entry_ctrls;
			/** Declare the following local variable of type bool
			 *  - old_paging_enabled representing the old value of guest paging state,
			 *    initialized as is_paging_enabled() */
			bool old_paging_enabled = is_paging_enabled(vcpu);
			/** Declare the following local variable of type uint64_t
			 *  - cr0_changed_bits representing the vCPU CR0 changed bits, initialized as
			 *    vcpu_get_cr0(vcpu) ^ cr0 */
			uint64_t cr0_changed_bits = vcpu_get_cr0(vcpu) ^ cr0;
			/** Declare the following local variable of type uint32_t
			 *  - cs_attr representing the vCPU CS attribute */
			uint32_t cs_attr;
			/** Declare the following local variable of type uint32_t
			 *  - tr_attr representing the vCPU TR attribute */
			uint32_t tr_attr;


			/** If the given vCPU attempts to change guest CR0.PG from 0 to 1 */
			if (!old_paging_enabled && ((cr0_mask & CR0_PG) != 0UL)) {
				/** If \a vcpu has long mode enabled */
				if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
					/** Print a debug message */
					pr_dbg("VMM: Enable long mode");

					/** Call exec_vmread32 with following parameters, in order to read
					 *  VMX_GUEST_CS_ATTR from VMCS and assign it to cs_attr.
					 *  - VMX_GUEST_CS_ATTR
					 */
					cs_attr = exec_vmread32(VMX_GUEST_CS_ATTR);
					/** Call exec_vmread32 with following parameters, in order to read
					 *  VMX_GUEST_TR_ATTR from VMCS and assign it to tr_attr.
					 *  - VMX_GUEST_TR_ATTR
					 */
					tr_attr = exec_vmread32(VMX_GUEST_TR_ATTR);
					/** If guest CS.L is 1H  or the segment type of guest TR is 16 bit busy TSS */
					if (((cs_attr & 0x2000U) != 0U) || ((tr_attr & 0xfU) == 3U)) {
						/** Record the error found by setting the err_found to be true */
						err_found = true;
						/** Call vcpu_inject_gp with following parameters, in order to
						 *  inject \# GP(0) to \a vcpu.
						 *  - vcpu
						 *  - 0H
						 */
						vcpu_inject_gp(vcpu, 0U);
					/** Otherwise */
					} else {
						/** Set entry_ctrls to the value of VMCS VM-entry controls
						 *  field on the current physical cpu */
						entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
						/** Set the IA32E mode bit in the entry_ctrls */
						entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
						/** Configure the VMX_ENTRY_CONTROLS in the VMCS to be entry_ctrls */
						exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);
						/** Set the long mode active bit in the guest EFER of \a vcpu */
						vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) | MSR_IA32_EFER_LMA_BIT);
					}
					/** \a vcpu has PAE mode enabled */
				} else if (is_pae(vcpu)) {
					/** If reloading PTPDRs for \a vcpu fails */
					if (load_pdptrs(vcpu) != 0) {
						/** Record the error found */
						err_found = true;
						/** Call vcpu_inject_gp to inject \# GP(0) to \a vcpu */
						vcpu_inject_gp(vcpu, 0U);
					}
				/** Not long mode and not pae mode */
				} else {
					/** do nothing */
				}
			/** If the given vCPU attempts to change guest CR0.PG from 1 to 0 */
			} else if (old_paging_enabled && ((cr0_mask & CR0_PG) == 0UL)) {
				/** If \a vcpu has long mode enabled */
				if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
					/** If the vcpu is running in the COMPATIBILITY mode */
					if (get_vcpu_mode(vcpu) == CPU_MODE_COMPATIBILITY) {
						/** Call exec_vmread32 with the following parameters,
						 *  in order to get the value of VMX_ENTRY_CONTROLS in
						 *  VMCS and assign it to entry_ctrls.
						 *  - VMX_ENTRY_CONTROLS
						 */
						entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
						/** Clear the VMX_ENTRY_CTLS_IA32E_MODE bit in
						 *  entry_ctrls.
						 */
						entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
						/** Call exec_vmwrite32 with the following parameters,
						 *  in order to set the value of VMX_ENTRY_CONTROLS in
						 *  VMCS.
						 *  - VMX_ENTRY_CONTROLS
						 *  - entry_ctrls
						 */
						exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);
						/** Call vcpu_set_efer with the following parameters,
						 *  in order to clear the long mode active bit in the
						 *  guest EFER of \a vcpu.
						 *  - vcpu
						 *  - vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT
						 */
						vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT);
					/** else if the vcpu is not running in the compatibility mode */
					} else {
						/** Print a debug message */
						pr_dbg("This is an invalid case");
						/** Set the err_found to be true */
						err_found = true;
						/** Call vcpu_inject_gp to inject \# GP(0) to \a vcpu */
						vcpu_inject_gp(vcpu, 0U);
					}
				}
				/** If no above condition meet */
			} else {
				/** do nothing */
			}

			/** No error is found in previous operations */
			if (err_found == false) {
				/** If cr0_changed_bits has CR0.CD set */
				if ((cr0_changed_bits & CR0_CD) != 0UL) {
					/** If cr0_mask has CD bit set */
					if ((cr0_mask & CR0_CD) != 0UL) {
						/** Configure the vCPU VMCS VMX_GUEST_IA32_PAT_FULL to be
						 *  PAT_ALL_UC_VALUE. When the guest requests to set CR0.CD,
						 *  we don't allow guest's CR0.CD to be actually set,
						 *  instead, we write guest IA32_PAT with all-UC
						 *  entries to emulate the cache disabled behavior */
						exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, PAT_ALL_UC_VALUE);
					/** cr0_mask has CD bit cleared */
					} else {
						/** Configure the VMX_GUEST_IA32_PAT_FULL in VMCS to be
						 *  vcpu_get_guest_msr(vcpu, MSR_IA32_PAT) */
						exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL,
							vcpu_get_guest_msr(vcpu, MSR_IA32_PAT));
					}
				}
				/** If \a vcpu attempts to change guest CR0.PG or CR0.WP or CR0_CD */
				if ((cr0_changed_bits & (CR0_PG | CR0_WP | CR0_CD)) != 0UL) {
					/** Call vcpu_make_request to flush TLB entries derived from
					 *  the EPT of \a vcpu */
					vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
				}
			}
		}
	}

	/** No error is found in previous operations */
	if (err_found == false) {
		/** Set the cr0_vmx to be cr0_always_on_mask | cr0_mask */
		cr0_vmx = cr0_always_on_mask | cr0_mask;

		/** Set cr0_vmx to be (cr0_vmx & ~(CR0_CD | CR0_NW)) */
		cr0_vmx &= ~(CR0_CD | CR0_NW);
		/** Set cr0_mask to be (cr0_mask | CR0_NE) */
		cr0_mask |= CR0_NE;
		/** Configure the VMX_GUEST_CR0 in VMCS to be cr0_vmx & 0xFFFFFFFFUL */
		exec_vmwrite(VMX_GUEST_CR0, cr0_vmx & 0xFFFFFFFFUL);
		/** Configure the VMX_CR0_READ_SHADOW in VMCS to be cr0_mask & 0xFFFFFFFFUL */
		exec_vmwrite(VMX_CR0_READ_SHADOW, cr0_mask & 0xFFFFFFFFUL);

		/** Call bitmap_clear_lock(CPU_REG_CR0, &vcpu->reg_cached) to clear read cache of CR0 */
		bitmap_clear_lock(CPU_REG_CR0, &vcpu->reg_cached);

		/** Print cr0_mask and cr0_vmx for debug */
		pr_dbg("VMM: Try to write %016lx, allow to write 0x%016lx to CR0", cr0_mask, cr0_vmx);
	}
}

/**
 * @brief This function checks if writing \a CR4 to the guest CR4 of \a vcpu is valid.
 *
 * @param[in] vcpu pointer to the vcpu whose guest CR4 is to be written.
 *
 * @param[in] cr4 the value to be written to CR4
 *
 * @return Whether the given value is valid when written to the given vCPU
 *
 * @retval true \a cr4 is a valid new value for guest CR4
 * @retval false \a cr4 is an invalid new value for guest CR4
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
static bool is_cr4_write_valid(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	/** Declare the following local variable of type bool
	 *  - ret representing the return value of this function, initialized as true */
	bool ret = true;

	/** If guest try to set fixed to 0 bits or reserved bits */
	if ((cr4 & cr4_always_off_mask) != 0U) {
		/** Set the ret to false */
		ret = false;
	/** If no always off bit is set */
	} else {
		/** If CR4 has CR4.VMXE or CR4.SMXE or CR4.PKE or CR4_PCE or CR4_DE set
		 *  or CR4_VME set or CR4_PVI set or
		 *  the vcpu is belong to non-safety-vm and the CR4 has CR4.MCE.*/
		if (((cr4 & CR4_VMXE) != 0UL) || ((cr4 & CR4_SMXE) != 0UL) || ((cr4 & CR4_PKE) != 0UL)
		 || ((cr4 & CR4_PCE) != 0UL) || ((cr4 & CR4_DE) != 0UL)
		 || ((cr4 & CR4_VME) != 0UL) || ((cr4 & CR4_PVI) != 0UL)
		 || (!is_safety_vm(vcpu->vm) && ((cr4 & CR4_MCE) != 0UL))) {
			/** Set the ret to false */
			ret = false;
		/** If the corresponding if condition does not meet */
		} else {
			/** If CR4 has CR4.PCIDE set */
			if ((cr4 & CR4_PCIDE) != 0UL) {
				/** Set the ret to false */
				ret = false;
			/** If the corresponding if condition does not meet */
			} else {
				/** If \a vcpu is in long mode */
				if (is_long_mode(vcpu)) {
					/** If CR4 does not has CR4.PAE set */
					if ((cr4 & CR4_PAE) == 0UL) {
						/** Set the ret to false */
						ret = false;
					}
				}
			}
		}
	}

	/** Return the result */
	return ret;
}

/**
 * @brief This function does the write operation to CR4
 *
 * This is a helper function for virtual CR4 setting, the public API vcpu_set_cr4
 * actually call this function to do the set operation.
 *
 * Handling of CR4:
 * Assume "unrestricted guest" feature is supported by vmx.
 *
 * For CR4, if some feature is not supported by hardware, the corresponding bit
 * will be set in cr4_always_off_mask. If guest try to set these bits after
 * vmexit, it will be injected a \# GP.
 * If a bit for a feature not supported by hardware, which is flexible to guest,
 * and write to it do not lead to a VM exit, a \# GP should be generated inside
 * guest.
 *
 *   - VME (0) Flexible to guest
 *   - PVI (1) Flexible to guest
 *   - TSD (2) Flexible to guest
 *   - DE  (3) Flexible to guest
 *   - PSE (4) Trapped to track paging mode.
 *	     Set the value according to the value from guest.
 *   - PAE (5) Trapped to track paging mode.
 *	     Set the value according to the value from guest.
 *   - MCE (6) Trapped to hide from guest
 *   - PGE (7) Flexible to guest
 *   - PCE (8) Flexible to guest
 *   - OSFXSR (9) Flexible to guest
 *   - OSXMMEXCPT (10) Flexible to guest
 *   - VMXE (13) Trapped to hide from guest
 *   - SMXE (14) must always be 0 => must lead to a VM exit
 *   - PCIDE (17) Trapped to hide from guest
 *   - OSXSAVE (18) Flexible to guest
 *   - XSAVE (19) Flexible to guest
 *	 We always keep align with physical cpu. So it's flexible to
 *	 guest
 *   - SMEP (20) Flexible to guest
 *   - SMAP (21) Flexible to guest
 *   - PKE (22) Flexible to guest
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR4 is to be written.
 *
 * @param[in] cr4 the value to be written to CR4
 *
 * @param[in] is_init indicate if the guest CR4 write is for initializing
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
static void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4, bool is_init)
{
	/** Declare the following local variable of type bool
	 *  - err_found representing if error found or not, initialized as false */
	bool err_found = false;

	/** Declare the following local variable of type uint64_t
	 *  - cr4_vmx representing the vCPU CR4 value to be set, not initialized
	 *  - cr4_shadow  representing the vCPU CR4 shadow value to be set, not initialized */
	uint64_t cr4_vmx, cr4_shadow;

	/** If is_init is false. */
	if (is_init == false) {
		/** If the write to CR4 is invalid */
		if (!is_cr4_write_valid(vcpu, cr4)) {
			/** Print a debug message */
			pr_dbg("Invalid cr4 write operation from guest");
			/** Call vcpu_inject_gp to inject \# GP(0) to \a vcpu */
			vcpu_inject_gp(vcpu, 0U);
			/** Set the err_found to be true */
			err_found = true;
		/** If write to CR4 is valid */
		} else {
			/** Declare the following local variable of type uint64_t
			 *  - old_cr4 representing the old guest CR4 value */
			uint64_t old_cr4 = vcpu_get_cr4(vcpu);

			/** If any of PGE, PSE, PAE, SMEP or SMAP bit is changed */
			if (((cr4 ^ old_cr4) & (CR4_PGE | CR4_PSE | CR4_PAE | CR4_SMEP | CR4_SMAP)) != 0UL) {
				/** If \a cr4 has PAE bit set and \a vcpu has paging enabled while
				 *  long mode disabled */
				if (((cr4 & CR4_PAE) != 0UL) && (is_paging_enabled(vcpu)) && (!is_long_mode(vcpu))) {
					/** If reloading PTPDRs for \a vcpu fails */
					if (load_pdptrs(vcpu) != 0) {
						/** Record the error found */
						err_found = true;
						/** Call vcpu_inject_gp to inject \# GP(0) to \a vcpu */
						vcpu_inject_gp(vcpu, 0U);
					}
				}
				/** If no error is found in previous operations */
				if (err_found == false) {
					/** Call vcpu_make_request to flush TLB entries derived from the EPT
					 *  of \a vcpu */
					vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
				}
			}
		}
	}

	/** If no error is found in previous operations */
	if (err_found == false) {
		/** Set cr4_shadow to cr4. */
		cr4_shadow = cr4;
		/** Set cr4_vmx to cr4_always_on_mask | cr4_shadow */
		cr4_vmx = cr4_always_on_mask | cr4_shadow;
		/** Configure VMX_GUEST_CR4 in VMCS to be cr4_vmx & 0xFFFFFFFFUL */
		exec_vmwrite(VMX_GUEST_CR4, cr4_vmx & 0xFFFFFFFFUL);
		/** Configure VMX_CR4_READ_SHADOW in VMCS to be cr4_shadow & 0xFFFFFFFFUL */
		exec_vmwrite(VMX_CR4_READ_SHADOW, cr4_shadow & 0xFFFFFFFFUL);

		/** Call bitmap_clear_lock(CPU_REG_CR4, &vcpu->reg_cached) to clear read cache of CR4 */
		bitmap_clear_lock(CPU_REG_CR4, &vcpu->reg_cached);

		/** Print cr4 and cr4_vmx for debug */
		pr_dbg("VMM: Try to write %016lx, allow to write 0x%016lx to CR4", cr4, cr4_vmx);
	}
}

/**
 * @brief public APIs for VCRs initialization configuration
 *
 * Initialize the CR0 Guest/Host Masks and CR4 Guest/Host Masks in the current VMCS.
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
 * @reentrancy unspecified
 * @threadsafety yes
 */
void init_cr0_cr4_host_mask(void)
{
	/** Declare the following local variables of type uint64_t
	 *  - cr0_host_owned_bits representing the CR0 bits owned by the host, not initialized
	 *  - cr4_host_owned_bits representing the CR4 bits owned by the host, not initialized */
	uint64_t cr0_host_owned_bits, cr4_host_owned_bits;
	/** Declare the following local variable of type uint64_t
	 *  - fixed0 representing the temp value read from MSR_IA32_VMX_CR0/CR4_FIXED0, not initialized
	 *  - fixed1 representing the temp value read from MSR_IA32_VMX_CR0/CR4_FIXED1, not initialized */
	uint64_t fixed0, fixed1;
	/** Declare the following local variable of type uint64_t
	 *  - cr0_always_on_bits representing the temp value for calculating cr0_always_on_mask, not initialized
	 *  - cr0_always_off_bits representing the temp value for calculating cr0_always_off_mask, not initialized
	 *  - cr4_always_off_bits representing the temp value for calculating cr4_always_off_mask, not initialized */
	uint64_t cr0_always_on_bits, cr0_always_off_bits, cr4_always_off_bits;

	/** Read the value of MSR_IA32_VMX_CR0_FIXED0 and write it to fixed0 */
	fixed0 = msr_read(MSR_IA32_VMX_CR0_FIXED0);
	/** Read the value of MSR_IA32_VMX_CR0_FIXED1 and write it to fixed1 */
	fixed1 = msr_read(MSR_IA32_VMX_CR0_FIXED1);

	/** Set the value of cr0_host_owned_bits to be ~(fixed0 ^ fixed1) */
	cr0_host_owned_bits = ~(fixed0 ^ fixed1);
	/** Set cr0_host_owned_bits to be (cr0_host_owned_bits | CR0_TRAP_MASK) */
	cr0_host_owned_bits |= CR0_TRAP_MASK;
	/** Set cr0_host_owned_bits to be (cr0_host_owned_bits & ~CR0_RESERVED_MASK) */
	cr0_host_owned_bits &= ~CR0_RESERVED_MASK;
	/** Set cr0_always_on_bits to be (fixed0 & (~(CR0_PE | CR0_PG))) */
	cr0_always_on_bits = fixed0 & (~(CR0_PE | CR0_PG));
	/** Set cr0_always_on_mask to be cr0_always_on_bits */
	cr0_always_on_mask = cr0_always_on_bits;
	/** Set the value of cr0_always_off_bits to be ~fixed1 */
	cr0_always_off_bits = ~fixed1;
	/** Set cr0_always_off_bits to be (cr0_always_off_bits | CR0_RESERVED_MASK) */
	cr0_always_off_bits |= CR0_RESERVED_MASK;
	/** Set cr0_always_off_mask to be cr0_always_off_bits & 0xffffffffe005003fUL
	 *  which indicates written to the CR0 reserved bits 28:19, 17 and 15:6 will
	 *  not cause GP.
	 */
	cr0_always_off_mask = cr0_always_off_bits & 0xffffffffe005003fUL;

	/** Read the value of MSR_IA32_VMX_CR4_FIXED0 and write it to fixed0 */
	fixed0 = msr_read(MSR_IA32_VMX_CR4_FIXED0);
	/** Read the value of MSR_IA32_VMX_CR4_FIXED1 and write it to fixed1 */
	fixed1 = msr_read(MSR_IA32_VMX_CR4_FIXED1);

	/** Set the value of cr4_host_owned_bits to be ~(fixed0 ^ fixed1) */
	cr4_host_owned_bits = ~(fixed0 ^ fixed1);
	/** Set cr4_host_owned_bits to be (cr4_host_owned_bits | CR4_TRAP_MASK) */
	cr4_host_owned_bits |= CR4_TRAP_MASK;
	/** Set cr4_host_owned_bits to be (cr4_host_owned_bits &  ~CR4_RESERVED_MASK) */
	cr4_host_owned_bits &= ~CR4_RESERVED_MASK;
	/** Set cr4_always_on_mask to be fixed0 */
	cr4_always_on_mask = fixed0;
	/** Set cr4_always_off_bits to be ~fixed1 */
	cr4_always_off_bits = ~fixed1;
	/** Set cr4_always_off_bits to be (cr4_always_off_bits | CR4_RESERVED_MASK) */
	cr4_always_off_bits |= CR4_RESERVED_MASK;
	/** Set cr4_always_off_mask to be cr4_always_off_bits */
	cr4_always_off_mask = cr4_always_off_bits;

	/** Configure the VMX_CR0_GUEST_HOST_MASK in VMCS to be cr0_host_owned_bits */
	exec_vmwrite(VMX_CR0_GUEST_HOST_MASK, cr0_host_owned_bits);
	/** Print CR0 mask value for debug */
	pr_dbg("CR0 guest-host mask value: 0x%016lx", cr0_host_owned_bits);

	/** Configure the VMX_CR4_GUEST_HOST_MASK in VMCS to be cr4_host_owned_bits */
	exec_vmwrite(VMX_CR4_GUEST_HOST_MASK, cr4_host_owned_bits);
	/** Print CR4 mask value for debug */
	pr_dbg("CR4 guest-host mask value: 0x%016lx", cr4_host_owned_bits);
}

/**
 * @brief public API for get the value of virtual CR0 of \a vcpu.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR0 is to be read.
 *
 * @return the value of CR0
 *
 * @pre vcpu != NULL
 * @pre The host physical address calculated by hva2hpa(vcpu>arch.vmcs) is
 *      equal to the current-VMCS pointer of the current pcpu.
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variable of type uint64_t
	 *  - mask representing the value read from VMCS VMX_CR0_GUEST_HOST_MASK, not initialized */
	uint64_t mask;
	/** Declare the following local variable of type 'struct run_context *'
	 *  - ctx representing the current run_context structure, initialized as
	 *	&vcpu->arch.context.run_ctx */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** Set bit CPU_REG_CR0 in vcpu->reg_cached to 1 while check if its old value is 0
	 *  which means guest CR0 of \a vcpu is not cached in ctx */
	if (bitmap_test_and_set_lock(CPU_REG_CR0, &vcpu->reg_cached) == 0) {
		/** Read the value from VMCS VMX_CR0_GUEST_HOST_MASK into mask */
		mask = exec_vmread(VMX_CR0_GUEST_HOST_MASK);
		/** Set ctx->cr0 to be (exec_vmread(VMX_CR0_READ_SHADOW) & mask) | (exec_vmread(VMX_GUEST_CR0)
		 *  & (~mask)) */
		ctx->cr0 = (exec_vmread(VMX_CR0_READ_SHADOW) & mask) | (exec_vmread(VMX_GUEST_CR0) & (~mask));
	}
	/** Return the cr0 value in ctx */
	return ctx->cr0;
}

/**
 * @brief public API for set the value of virtual CR0 of \a vcpu.
 *
 * This API call helper function vmx_write_cr0 to do virtual CR0 setting.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR0 is to be written.
 * @param[in] val the value to be set to guest CR0 of \a vcpu
 * @param[in] is_init indicate if the guest CR0 write is for initializing
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre The host physical address calculated by hva2hpa(vcpu>arch.vmcs) is
 *      equal to the current-VMCS pointer of the current pcpu.
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val, bool is_init)
{
	/** Call vmx_write_cr0 to write the cr0 */
	vmx_write_cr0(vcpu, val, is_init);
}

/**
 * @brief public API for set the value of virtual CR2 of \a vcpu.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR2 is to be written.
 * @param[in] val the value to be set to guest CR2 of \a vcpu
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val)
{
	/** Set vcpu->arch.context.run_ctx.cr2 to be \a val */
	vcpu->arch.context.run_ctx.cr2 = val;
}

/**
 * @brief public API for get the value of virutal CR4 of \a vcpu.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR4 is to be read.
 *
 * @return the value of CR4
 *
 * @pre vcpu != NULL
 * @pre The host physical address calculated by hva2hpa(vcpu>arch.vmcs) is
 *      equal to the current-VMCS pointer of the current pcpu.
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variable of type uint64_t
	 *  - mask representing the value read from VMCS VMX_CR4_GUEST_HOST_MASK, not initialized */
	uint64_t mask;
	/** Declare the following local variable of type 'struct run_context *'
	 *  - ctx representing the current run_context structure, initialized as
	 *	&vcpu->arch.context.run_ctx */
	struct run_context *ctx = &vcpu->arch.context.run_ctx;

	/** Set bit CPU_REG_CR4 in vcpu->reg_cached to 1 while check if its old value is 0
	 *  which means guest CR4 of \a vcpu is not cached in ctx */
	if (bitmap_test_and_set_lock(CPU_REG_CR4, &vcpu->reg_cached) == 0) {
		/** Read the value from VMCS VMX_CR4_GUEST_HOST_MASK into mask */
		mask = exec_vmread(VMX_CR4_GUEST_HOST_MASK);
		/** Set ctx->cr4 to be (exec_vmread(VMX_CR4_READ_SHADOW) & mask) |
		 *  (exec_vmread(VMX_GUEST_CR4) & (~mask)) */
		ctx->cr4 = (exec_vmread(VMX_CR4_READ_SHADOW) & mask) | (exec_vmread(VMX_GUEST_CR4) & (~mask));
	}
	/** Return the cr4 value in ctx */
	return ctx->cr4;
}

/**
 * @brief public API for set the value of virutal CR4 of \a vcpu.
 *
 * This API call helper function vmx_write_cr4 to do virtual CR4 setting.
 *
 * @param[inout] vcpu pointer to the vcpu whose guest CR4 is to be written.
 * @param[in] val the value to be set to guest CR4 of \a vcpu
 * @param[in] is_init indicate if the guest CR4 write is for initializing
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre The host physical address calculated by hva2hpa(vcpu>arch.vmcs) is
 *      equal to the current-VMCS pointer of the current pcpu.
 *
 * @post N/A
 *
 * @mode HV_INIT
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val, bool is_init)
{
	/** Call vmx_write_cr4 to write the CR4 */
	vmx_write_cr4(vcpu, val, is_init);
}

/**
 * @brief VM-Exit handler for VCRs access
 *
 * The handler for VM-Exit with the reason "control-register accesses".
 * Guest software attempted to write CR0, CR3, CR4, or CR8 using CLTS,
 * LMSW, or MOV CR and the VM-execution control fields indicate that a
 * VM exit should occur. This function shall be called. Currently, it
 * only handle the cases of CR0 and CR4, other cases will be treated
 * as error.
 *
 * @param[inout] vcpu pointer to the vcpu which attempts to access a CR
 *
 * @return 0 if no error happened, otherwise return -EINVAL (-22)
 *
 * @retval -EINVAL when the VM-exit is not caused by moving to CR0, moving to CR4 or LMSW
 * @retval 0 otherwise
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy unspecified
 * @threadsafety when \a vcpu is different among parallel invocation
 */
int32_t cr_access_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variable of type uint64_t
	 *  - reg representing value to be written to a CR, not initialized */
	uint64_t reg;
	/** Declare the following local variable of type uint32_t
	 *  - idx representing the index of the general-purpose register, not initialized */
	uint32_t idx;
	/** Declare the following local variable of type uint64_t
	 *  - exit_qual representing the value of vcpu->arch.exit_qualification, not initialized */
	uint64_t exit_qual;
	/** Declare the following local variable of type uint32_t
	 *  - ret representing the return value of this function, initialized as 0 */
	int32_t ret = 0;

	/** Set exit_qual to be vcpu->arch.exit_qualification */
	exit_qual = vcpu->arch.exit_qualification;
	/** decode the VM-Exit reason to get the info which general-purpose register is used
	 *  and assign the result to idx */
	idx = (uint32_t)vm_exit_cr_access_reg_idx(exit_qual);

	/** For MOV CR, set reg to the value from the general-purpose register indicated by idx */
	reg = vcpu_get_gpreg(vcpu, idx);

	/** Switch based on the accessed CR and access type */
	switch ((vm_exit_cr_access_type(exit_qual) << 4U) | vm_exit_cr_access_cr_num(exit_qual)) {
	/** Move to CR0 */
	case 0x00UL:
		/** Call vcpu_set_cr0 to set reg to the guest CR0 of \a vcpu
		 *  - vcpu
		 *  - reg
		 *  - false */
		vcpu_set_cr0(vcpu, reg, false);
		/** go out of the switch */
		break;
	/** Move to CR4 */
	case 0x04UL:
		/** Call vcpu_set_cr4 to set reg to the guest CR4 of \a vcpu
		 *  - vcpu
		 *  - reg
		 *  - false */
		vcpu_set_cr4(vcpu, reg, false);
		/** go out of the switch */
		break;
	/** LMSW access type */
	case 0x30UL:
		/** Call vcpu_set_cr0() with the following parameters, in order to set
		 * (vcpu_get_cr0(vcpu) & (~0x0eUL)) | ((exit_qual >> 16UL) & 0x0fUL)
		 *  to the guest CR0 of \a vcpu.
		 *  - vcpu
		 *  - (vcpu_get_cr0(vcpu) & (~0x0eUL)) | ((exit_qual >> 16UL) & 0x0fUL)
		 *  - false
		 */
		vcpu_set_cr0(vcpu, (vcpu_get_cr0(vcpu) & (~0x0eUL)) | ((exit_qual >> 16UL) & 0x0fUL), false);
		/** go out of the switch */
		break;
	/** default branch of the switch */
	default:
		/** This is unhandled CR access */
		ASSERT(false, "Unhandled CR access");
		/** Set the ret to -ERANGE */
		ret = -ERANGE;
		/** go out of the switch */
		break;
	}

	/** Print the Exit Qualification field: number of control register and Access type */
	TRACE_2L(TRACE_VMEXIT_CR_ACCESS, vm_exit_cr_access_type(exit_qual), vm_exit_cr_access_cr_num(exit_qual));

	/** ret as return value */
	return ret;
}

/**
 * @}
 */
