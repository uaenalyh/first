/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <atomic.h>
#include <io_req.h>
#include <vcpu.h>
#include <vm.h>
#include <vmexit.h>
#include <vmx.h>
#include <ept.h>
#include <pgtable.h>
#include <trace.h>
#include <logmsg.h>
#include <virq.h>

/**
 * @addtogroup vp-dm_io-req
 *
 * @{
 */

 /**
  * @file
  * @brief This file implements APIs to handle VM exit on port I/O instruction and EPT violation.
  *
  * External APIs:
  * - pio_instr_vmexit_handler()
  * - ept_violation_vmexit_handler()
  */


/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param [in] vcpu Pointer to the instance of struct acrn_vcpu, which triggers
 *		    the VM exit on I/O instruction.
 *
 * @return Whether the emulation succeeds or not
 *
 * @retval 0 Emulation for the I/O instruction is handled successfully.
 * @retval -ERANGE Emulation for the I/O instruction fails due to errors of range checks on the VM exit qualification
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu is
 *		 different among parallel invocations.
 *
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/**
	 * Declare the following local variables of type uint64_t.
	 *  - exit_qual representing VM exit qualification for I/O
	 *    instructions, not initialized.
	 */
	uint64_t exit_qual;
	/**
	 * Declare the following local variables of type uint32_t.
	 *  - mask representing the bitmap for I/O request size, not initialized.
	 */
	uint32_t mask;
	/**
	 * Declare the following local variables of type struct io_request *.
	 *  - io_req representing current I/O request detailed information,
	 *    initialized as &vcpu->req.
	 */
	struct io_request *io_req = &vcpu->req;
	/**
	 * Declare the following local variables of type struct pio_request *.
	 *  - pio_req representing current port I/O request detailed information,
	 *    initialized as &io_req->reqs.pio.
	 */
	struct pio_request *pio_req = &io_req->reqs.pio;
	/** Declare the following local variable of type int32_t
	 *  - ret representing the return value of this function, initialized as 0 */
	int32_t ret = 0;

	/** Set exit_qual to vcpu->arch.exit_qualification. */
	exit_qual = vcpu->arch.exit_qualification;

	/** Set pio_req->size to size of access parsed from exit_qual plus 1 */
	pio_req->size = vm_exit_io_instruction_size(exit_qual) + 1UL;
	/** Set pio_req->address to port number of access parsed from exit_qual */
	pio_req->address = vm_exit_io_instruction_port_number(exit_qual);
	/** If pio_req->size equals to 3 or is greater than 4, which means the size is invalid */
	if ((pio_req->size == 3UL) || (pio_req->size >= 5UL)) {
		/** Set ret to -ERANGE, which means a range check has failed */
		ret = -ERANGE;
	/** If direction of attempted access is 0 (OUT). */
	} else if (vm_exit_io_instruction_access_direction(exit_qual) == 0UL) {
		/** Set mask to 0xFFFFFFFFU >> (32U - (8U * pio_req->size)) */
		mask = 0xFFFFFFFFU >> (32U - (8U * pio_req->size));
		/** Set pio_req->direction to REQUEST_WRITE */
		pio_req->direction = REQUEST_WRITE;
		/** Set pio_req->value to low 32bits of RAX of vcpu masked with mask. */
		pio_req->value = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RAX) & mask;
	} else {
		/** Set pio_req->direction to REQUEST_READ */
		pio_req->direction = REQUEST_READ;
	}

	/** If ret is 0, meaning no error occurs */
	if (ret == 0) {
		/**
		 * Call TRACE_4I() with the following parameters,
		 * in order to log current I/O access information.
		 *  - pio_req->address
		 *  - pio_req->direction
		 *  - pio_req->size
		 */
		TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION, (uint32_t)pio_req->address, (uint32_t)pio_req->direction,
			 (uint32_t)pio_req->size, 0U);

		/**
		 * Call emulate_io() with the following parameters, in order to emulate current I/O access.
		 *  - vcpu
		 *  - io_req
		 */
		emulate_io(vcpu, io_req);
	}

	/** Return ret */
	return ret;
}

/**
 * @brief The handler of VM exits on EPT violation.
 *
 * @param [in] vcpu Pointer to the instance of struct acrn_vcpu,
 *		    which triggers the VM exit on EPT violation.
 *
 * @return 0, EPT violation shall always be handled successfully.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu is
 *		 different among parallel invocations.
 *
 */
int32_t ept_violation_vmexit_handler(struct acrn_vcpu *vcpu)
{
	/**
	 * Declare the following local variables of type uint64_t.
	 *  - exit_qual representing VM exit qualification
	 *    for EPT violation, not initialized.
	 */
	uint64_t exit_qual;
	/**
	 * Declare the following local variables of type uint64_t.
	 *  - gpa representing guest physical address, not initialized.
	 *  - pg_size representing the page size of the faulting address in the EPT, not initialized.
	 */
	uint64_t gpa, pg_size;
	/**
	 * Declare the following local variables of type const uint64_t *.
	 *  - pgentry representing the paging entry of the faulting address in the EPT.
	 */
	const uint64_t *pgentry;

	/** Set exit_qual to vcpu->arch.exit_qualification */
	exit_qual = vcpu->arch.exit_qualification;
	/** Set gpa to the value of VMX_GUEST_PHYSICAL_ADDR_FULL (VMCS field)*/
	gpa = exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL);

	/**
	 * Call lookup_address with the following parameters and set pgentry to its return value, in order to get the
	 * paging entry of gpa in the EPT of the vCPU.
	 *  - vcpu->vm->arch_vm.nworld_eptp
	 *  - gpa
	 *  - &pg_size
	 *  - vcpu->vm->arch_vm.ept_mem_ops
	 */
	pgentry = lookup_address(vcpu->vm->arch_vm.nworld_eptp, gpa, &pg_size, &vcpu->vm->arch_vm.ept_mem_ops);

	/**
	 * Call TRACE_2L with the following parameters, in order to log
	 * exit_qual and gpa information.
	 *  - exit_qual
	 *  - gpa
	 */
	TRACE_2L(TRACE_VMEXIT_EPT_VIOLATION, exit_qual, gpa);

	/** If EPT violation is caused by instruction fetch access, pgentry is not NULL and bit 5:3 in *pgentry equals
	 *  to EPT_WB. */
	if (((exit_qual & 0x4UL) != 0UL) && (pgentry != NULL) && ((*pgentry & EPT_MT_MASK) == EPT_WB)) {
		/**
		 * Call ept_modify_mr() with the following parameters,
		 * in order to set EPT memory access right to be executable.
		 *  - vcpu->vm
		 *  - vcpu->vm->arch_vm.nworld_eptp
		 *  - gpa & PAGE_MASK
		 *  - PAGE_SIZE
		 *  - EPT_EXE
		 *  - 0UL
		 */
		ept_modify_mr(vcpu->vm, (uint64_t *)vcpu->vm->arch_vm.nworld_eptp, gpa & PAGE_MASK, PAGE_SIZE,
			EPT_EXE, 0UL);
		/**
		 * Call vcpu_retain_rip() with the following parameters,
		 * in order to retain guest RIP for next VM entry.
		 *  - vcpu
		 */
		vcpu_retain_rip(vcpu);
	} else {

		/** Call vcpu_inject_pf() with the following parameters,
		 *  in order to inject page fault exeception to guest VM.
		 *  - vcpu
		 *  - 0UL
		 *  - 0U
		 */
		vcpu_inject_pf(vcpu, 0UL, 0U);
	}

	/** Return 0 which means EPT violation has been handled successfully. */
	return 0;
}

/**
 * @}
 */
