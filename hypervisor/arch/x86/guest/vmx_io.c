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

/**
 * @addtogroup vp-dm_io_req
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
 * @return 0, Emulation for I/O instruction shall be always handled successfully.
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

	/** Set exit_qual to vcpu->arch.exit_qualification. */
	exit_qual = vcpu->arch.exit_qualification;

	/** Set pio_req->size to size of access parsed from exit_qual plus 1 */
	pio_req->size = vm_exit_io_instruction_size(exit_qual) + 1UL;
	/** Set pio_req->address to port number of access parsed from exit_qual */
	pio_req->address = vm_exit_io_instruction_port_number(exit_qual);
	/** If direction of attempted access is 0 (OUT). */
	if (vm_exit_io_instruction_access_direction(exit_qual) == 0UL) {
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

	/**
	 * Call TRACE_4I() with the following parameters,
	 * in order to log current I/O access infomation.
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

	/** Return 0 which means I/O access has been emulated successfully. */
	return 0;
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
	 */
	uint64_t gpa;

	/** Set exit_qual to vcpu->arch.exit_qualification */
	exit_qual = vcpu->arch.exit_qualification;
	/** Set gpa to the value of VMX_GUEST_PHYSICAL_ADDR_FULL (VMCS field)*/
	gpa = exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL);

	/**
	 * Call TRACE_2L with the following parameters, in order to log
	 * exit_qual and gpa infomation.
	 *  - exit_qual
	 *  - gpa
	 */
	TRACE_2L(TRACE_VMEXIT_EPT_VIOLATION, exit_qual, gpa);

	/** If EPT violation is caused by instruction fetch access. */
	if ((exit_qual & 0x4UL) != 0UL) {
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
		 *  - exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL)
		 *  - 0U
		 */
		vcpu_inject_pf(vcpu, exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL), 0U);
	}

	/** Return 0 which means EPT violation has been handled successfully. */
	return 0;
}

/**
 * @}
 */
