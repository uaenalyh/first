/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains pure vmx operations
 */

#include <types.h>
#include <msr.h>
#include <per_cpu.h>
#include <pgtable.h>
#include <vmx.h>

/**
 * @defgroup hwmgmt_vmx hwmgmt.vmx
 * @ingroup hwmgmt
 * @brief This module implements all operations related to VMX.
 *
 * These operations include turning on/off VMX, clearing and loading VMCS pointer.
 * In addition, read/write operations on VMCS fields are also implemented.
 *
 * Usage:
 * - vp-base.base_hv_main module depends on this module to do following things:
 *   - Set up some fields in the VMCS.
 *   - Set up MSR store/load area information on VM exit.
 *   - Set up MSR load area information on VM entry.
 *   - Set up VMCS pointer.
 *   - Read and write VMCS fields.
 * - vp-base.vmsr module depends on this module to do following things:
 *   - Set address of MSR bitmaps (full) in the VMCS.
 *   - Set up TSC offset in the VMCS.
 *   - Set up PAT in the VMCS.
 *   - Read and write VMCS fields.
 * - vp-base.vlapic module depends on this module to do following things:
 *   - Get value of TSC offset in the VMCS.
 *   - Read and write VMCS fields.
 * - vp-base.virq module depends on this module to do following things:
 *   - Read and write VMCS fields.
 * - vp-dm.io_req module depends on this module to do following things:
 *   - Get guest physical address which triggers the EPT violation.
 *   - Read and write VMCS fields.
 * - vp-base.vcr module depends on this module to do following things:
 *   - Set up PAT in the VMCS.
 *   - Set up PDPTEs in the VMCS.
 *   - Read and write VMCS fields.
 * - vp-base.vcpuid module depends on this module to do following things:
 *   - Read VMCS fields.
 * - vp-base.vcpu module depends on this module to do following things:
 *   - Set up VPID.
 *   - Manipulate IA32_EFER register in the VMCS.
 *   - Read and write VMCS fields.
 * - init module depends on this module to enable VMX.
 * - hwmgmt.cpu module depends on this module to disable VMX.
 *
 * Dependency:
 * - This module depends on hwmgmt.cpu to do following things:
 *   - Get per-cpu data for current CPU.
 *   - Read and write MSR.
 *   - Read and write CR registers.
 * - This module depends on lib.util to do memory copy.
 * - This module depends on hwmgmt.page to do memory address translation.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external and helper APIs for operations on VMX.
 *
 * This file implements all the operations on VMX including switching on and off
 * VMX, loading and clearing VMCS pointer. It also supports reading and writing
 * fields in VMCS.
 *
 * External functions include: vmx_on, vmx_off, exec_vmclear, exec_vmptrld, exec_vmread32,
 * exec_vmread64, exec_vmwrite16, exec_vmwrite32 and exec_vmwrite64.
 *
 * Internal helper functions include: exec_vmxon, exec_vmxoff.
 */

/**
 * @brief This helper function puts current logical processor in VMX operation.
 *
 * @param[in] addr A 4KB-aligned physical address (the VMXON pointer) that references the VMXON region.
 *
 * @return None
 *
 * @pre addr != NULL
 * @pre addr is 4KB-aligned
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline void exec_vmxon(void *addr)
{
	/** Execute 'vmxon' instruction in order to put the logical processor in VMX
	 *  operation with no current VMCS, blocks INIT signals, disables A20M, and clears
	 *  any address-range monitoring established by the MONITOR instruction.
	 *  - Input operands: \a addr which is the operand.
	 *  - Output operands: none.
	 *  - Clobbers: memory and flags register.
	 */
	asm volatile("vmxon (%%rax)\n" : : "a"(addr) : "cc", "memory");
}

/**
 * @brief This function enables current logical processor's VMX.
 *
 * @return None
 *
 * @pre hva2hpa(get_cpu_var(vmxon_region)) is not NULL, and is 4KB aligned.
 * @pre Logical and of the values of CR0 of the current processor and (~08005003FH) shall be zero.
 * @pre Logical and of the values of CR0 of the current processor and 080000001H shall be 080000001H.
 * @pre Logical and of the values of CR4 of the current processor and 0FFC89800H shall be zero.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark The physical IA32_FEATURE_CONTROL[bit 2] shall be 1.
 * @remark The physical IA32_FEATURE_CONTROL[bit 0] shall be 1.
 * @remark The current processor is outside SMX operation.
 * @remark The hypervisor shall have no address-range monitoring established by the MONITOR instruction.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void vmx_on(void)
{
	/** Declare the following local variable of type uint64_t
	 *  - 'tmp64' representing a temporary variable to store the value
	 *    read from the physical register, not initialized.
	 *  - 'vmxon_region_pa' representing the physical address of vmxon region, not initialized.
	 */
	uint64_t tmp64, vmxon_region_pa;
	/** Declare the following local variable of type uint32_t
	 *  - 'tmp32' representing a temporary variable to store the value read from
	 *    the physical register, not initialized.
	 */
	uint32_t tmp32;
	/** Declare the following local variable of type void *
	 *  - 'vmxon_region_va' representing the virtual address of vmxon region, initialized as
	 *    the return value of 'get_cpu_var(vmxon_region)', which is the current
	 *    CPU's vmxon_region address.
	 */
	void *vmxon_region_va = (void *)get_cpu_var(vmxon_region);

	/** Call msr_read with the following parameters, in order to
	 *  get current MSR_IA32_VMX_BASIC value and set low 32 bits of the return value to 'tmp32'.
	 *  - MSR_IA32_VMX_BASIC
	 */
	tmp32 = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
	/** Call memcpy_s with the following parameters, in order to
	 *  copy value of 'tmp32' to low 4 bytes of the current CPU's vmxon_region.
	 *  - vmxon_region_va
	 *  - 4
	 *  - &tmp32
	 *  - 4
	 */
	(void)memcpy_s(vmxon_region_va, 4U, (void *)&tmp32, 4U);

	/** Call CPU_CR_READ with the following parameters, in order to
	 *  get current value of CR0 register and set it to 'tmp64'.
	 *  - cr0
	 *  - &tmp64
	 */
	CPU_CR_READ(cr0, &tmp64);
	/** Call CPU_CR_WRITE with the following parameters, in order to
	 *  set value of 'tmp64' bitwise OR with CR0_NE back to CR0 register.
	 *  - cr0
	 *  - tmp64 | CR0_NE
	 */
	CPU_CR_WRITE(cr0, tmp64 | CR0_NE);
	/** Call CPU_CR_READ with the following parameters, in order to
	 *  get current value of CR4 register and set it to 'tmp64'.
	 *  - cr4
	 *  - &tmp64
	 */
	CPU_CR_READ(cr4, &tmp64);
	/** Call CPU_CR_WRITE with the following parameters, in order to
	 *  set value of 'tmp64' bitwise OR with CR4_VMXE back to CR4 register.
	 *  - cr4
	 *  - tmp64 | CR4_VMXE
	 */
	CPU_CR_WRITE(cr4, tmp64 | CR4_VMXE);

	/** Call hva2hpa with the following parameters, in order to
	 *  get current physical address of current CPU's vmxon region
	 *  and set its return value to 'vmxon_region_pa'.
	 *  - vmxon_region_va
	 */
	vmxon_region_pa = hva2hpa(vmxon_region_va);
	/** Call exec_vmxon with the following parameters, in order to
	 *  put current logical processor in VMX operation.
	 *  - &vmxon_region_pa
	 */
	exec_vmxon(&vmxon_region_pa);
}

/**
 * @brief This helper function takes the logical processor out of VMX operation.
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
 * @threadsafety Unspecified
 */
static inline void exec_vmxoff(void)
{
	/** Execute 'vmxoff' instruction in order to take the logical processor
	 *  out of VMX operation, unblocks INIT signals, conditionally re-enables A20M,
	 *  and clears any address-range monitoring.
	 *  - Input operands: none.
	 *  - Output operands: none.
	 *  - Clobbers: memory.
	 */
	asm volatile("vmxoff" : : : "memory");
}

/**
 * @brief This function copies VMCS data to VMCS region in memory and makes it invalid
 * only if it is the current-VMCS pointer.
 *
 * @param[inout] addr Pointed to the host physical start address of the specified VMCS memory region.

 * @return None
 *
 * @pre addr is 4KB align
 * @pre addr[63:39] is 0H
 * @pre addr != NULL
 * @pre addr is a physical address different from the VMXON pointer of any physical processor.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a addr is different among parallel invocation.
 */
void exec_vmclear(void *addr)
{
	/** Execute 'vmclear' instruction in order to copy VMCS data to VMCS region specified by \a addr
	 *  - Input operands: \a addr which is the operand.
	 *  - Output operands: none.
	 *  - Clobbers: memory and flags register.
	 */
	asm volatile("vmclear (%%rax)\n" : : "a"(addr) : "cc", "memory");
}

/**
 * @brief This function loads the current VMCS pointer from memory.
 *
 * @param[in] addr Pointed to the host physical start address of the specified VMCS memory region.

 * @return None
 *
 * @pre addr is 4KB align
 * @pre addr[63:39] is 0H
 * @pre addr != NULL
 * @pre addr is a physical address different from the VMXON pointer of any physical processor.
 * @pre addr (*(uint32_t)addr & 7FFFFFFFH) == msr_read(MSR_IA32_VMX_BASIC) & 7FFFFFFFH
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void exec_vmptrld(void *addr)
{
	/** Execute 'vmptrld' instruction in order to load the VMCS pointer from memory specified by \a addr.
	 *  - Input operands: \a addr which is the operand.
	 *  - Output operands: none.
	 *  - Clobbers: memory and flags register.
	 */
	asm volatile("vmptrld (%%rax)\n" : : "a"(addr) : "cc", "memory");
}

/**
 * @brief This function disables current logical processor's VMX.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The physical IA32_SMM_MONITOR_CTL [bit 0] of the current processor shall be 0.
 * @remark The current processor is in VMX operation.
 * @remark The hypervisor shall have no address-range monitoring established by the MONITOR instruction.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void vmx_off(void)
{
	/** Declare the following local variable of type void **
	 *  - 'vmcs_ptr' address to the pointer of current processor's vmcs,
	 *    initialized as the return value of 'get_cpu_var(vmcs_run)',
	 *    which is the current CPU's vmcs_run address.
	 */
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	/** If value located at 'vmcs_ptr' is not NULL. */
	if (*vmcs_ptr != NULL) {
		/** Declare the following local variable of type uint64_t
		 *  - 'vmcs_pa' representing the physical host address to the pointer of current
		 *    processor's VMCS, not initialized.
		 */
		uint64_t vmcs_pa;

		/** Call hva2hpa with the following parameters, in order to
		 *  get physical host address of current processor's vmcs and set its return value to 'vmcs_pa'.
		 *  - *vmcs_ptr
		 */
		vmcs_pa = hva2hpa(*vmcs_ptr);
		/** Call exec_vmclear with the following parameters, in order to
		 *  make processor's vmcs pointer invalid.
		 *  - &vmcs_pa
		 */
		exec_vmclear((void *)&vmcs_pa);
		/** Set the value located at vmcs_ptr to NULL */
		*vmcs_ptr = NULL;
	}

	/** Call exec_vmxoff in order to take processor out of VMX operation. */
	exec_vmxoff();
}

/**
 * @brief This function reads a specified 64-bit field from the current VMCS.
 *
 * @param[in] field_full The field of VMCS to be accessed.

 * @return The value of that field in VMCS.
 *
 * @pre field_full is a valid 64-bit VMCS component field encoding defined in Appendix B, Vol. 3, SDM.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This interface can be called only after exec_vmptrld is called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When Current-VMCS pointers are different on different processors.
 */
uint64_t exec_vmread64(uint32_t field_full)
{
	/** Declare the following local variable of type uint64_t
	 *  - 'value' representing the specified field's value in the VMCS, not initialized.
	 */
	uint64_t value;

	/** Execute 'vmread' instruction in order to read a specified field from the current processor's VMCS.
	 *  - Input operands: \a field_full which is the register source operand.
	 *  - Output operands: 'value' which is the destination operand.
	 *  - Clobbers: flags register.
	 */
	asm volatile("vmread %%rdx, %%rax " : "=a"(value) : "d"(field_full) : "cc");

	/** Return field's value which is stored in 'value' */
	return value;
}

/**
 * @brief This function reads a specified 32-bit field from the current VMCS.
 *
 * @param[in] field The field of VMCS to be accessed.

 * @return The value of that field in VMCS.
 *
 * @pre field is a valid 32-bit VMCS component field encoding defined in Appendix B, Vol. 3, SDM.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This interface can be called only after exec_vmptrld is called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When Current-VMCS pointers are different on different processors.
 */
uint32_t exec_vmread32(uint32_t field)
{
	/** Declare the following local variable of type uint64_t
	 *  - 'value' specified field's value in the VMCS, not initialized.
	 */
	uint64_t value;

	/** Call exec_vmread64 with the following parameters, in order to
	 *  reads a specified 64-bit field from the current VMCS and set its
	 *  returned value to 'value'.
	 *  - field
	 */
	value = exec_vmread64(field);

	/** Return the low 32-bits of 'value' */
	return (uint32_t)value;
}

/**
 * @brief This function writes a 64-bit value to a 64-bit field in the current VMCS.
 *
 * @param[in] field_full The field of VMCS to be accessed.
 * @param[in] value The value which will be written to the VMCS field.

 * @return None
 *
 * @pre field_full is a valid 64-bit VMCS component field encoding defined in Appendix B, Vol. 3, SDM.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This interface can be called only after exec_vmptrld is called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When Current-VMCS pointers are different on different processors.
 */
void exec_vmwrite64(uint32_t field_full, uint64_t value)
{
	/** Execute 'vmwrite' instruction in order to write a 64-bit value to a field in the VMCS.
	 *  - Input operands: \a value which is the primary source operand (register or memory)
	 *    that specifies the content.
	 *  - Input operands: \a field_full which is the secondary source operand (register)
	 *    that specifies the VMCS field.
	 *  - Output operands: none.
	 *  - Clobbers: flags register.
	 */
	asm volatile("vmwrite %%rax, %%rdx " : : "a"(value), "d"(field_full) : "cc");
}

/**
 * @brief This function writes a 32-bit value to a 32-bit field in the current VMCS.
 *
 * @param[in] field The field of VMCS to be accessed.
 * @param[in] value The value which will be written to the VMCS field.

 * @return None
 *
 * @pre \a field is a valid 32-bit VMCS component field encoding defined in Appendix B, Vol. 3, SDM.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This interface can be called only after exec_vmptrld is called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When Current-VMCS pointers are different on different processors.
 */
void exec_vmwrite32(uint32_t field, uint32_t value)
{
	/** Call exec_vmwrite64 with the following parameters, in order to
	 *  write a value to a field in the current VMCS.
	 *  - field
	 *  - (uint64_t)value
	 */
	exec_vmwrite64(field, (uint64_t)value);
}

/**
 * @brief This function writes a 16-bit value to a 16-bit field in the current VMCS.
 *
 * @param[in] field The field of VMCS to be accessed.
 * @param[in] value The value which will be written to the VMCS field.

 * @return None
 *
 * @pre \a field is a valid 16-bit VMCS component field encoding defined in Appendix B, Vol. 3, SDM.
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This interface can be called only after exec_vmptrld is called once on the current processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When Current-VMCS pointers are different on different processors
 */
void exec_vmwrite16(uint32_t field, uint16_t value)
{
	/** Call exec_vmwrite64 with the following parameters, in order to
	 *  write a value to a field in the current VMCS.
	 *  - field
	 *  - (uint64_t)value
	 */
	exec_vmwrite64(field, (uint64_t)value);
}

/**
 * @}
 */
