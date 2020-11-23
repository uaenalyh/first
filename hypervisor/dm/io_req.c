/*
 * Copyright (C) 2019 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <irq.h>
#include <errno.h>
#include <logmsg.h>

/**
 * @defgroup vp-dm_io-req vp-dm.io-req
 * @ingroup vp-dm
 * @brief Implementation of API to register port IO emulation handlers and API to handle I/O instruction VM exit.
 *
 * This module implements API to register a port I/O handler for specific VM and handler for VM exit on I/O
 * instructions.
 *
 * Usage Remark: vp-dm.vperipheral module uses this module to register emulation handlers for PCI configuration
 * and RTC pot I/O respectively.vp-base.hv_main module uses this module to handle VM exit on I/O instructions.
 *
 * Dependency justification: This module uses vp-base.hv_main module to parse I/O instruction VM exit qualification
 * and uses vp-base.vcpu module to access to vCPU registers during the emulation of I/O instructions.
 *
 * External APIs:
 * - register_pio_emulation_handler()  This function is the API to register port IO emulation handlers
 *                                     (Read/Write callback functions) for specific VM.
 *					Depends on:
 *					 - N/A.
 *
 * - pio_instr_vmexit_handler()        This function handles VM exits on I/O instructions.
 *					Depends on:
 *					 - vm_exit_io_instruction_size()
 *					 - vm_exit_io_instruction_port_number()
 *					 - vm_exit_io_instruction_access_direction()
 *					 - vcpu_get_gpreg()
 *					 - emulate_io()
 *
 * Internal functions:
 * - emulate_io()			This function implements port I/O instruction emulation and updates
 *					vCPU registers by wrapping hv_emulate_pio() and emulate_pio_complete().
 *					Depends on:
 *					 - hv_emulate_pio()
 *					 - emulate_pio_complete()
 *
 * - hv_emulate_pio()			This function implements port I/O read/write emulation.
 *					Depends on:
 *					 - N/A.
 *
 * - emulate_pio_complete()		This function updates vCPU register for port I/O read
 *					instruction emulation.
 *					Depends on:
 *					 - vcpu_get_gpreg()
 *					 - vcpu_set_gpreg()
 *
 * - pio_default_read()			Default handler for port I/O read.
 *					Depends on:
 *					- N/A.
 *
 * - pio_default_write()		Default handler for port I/O write.
 *					Depends on:
 *					- N/A.
 *
 * @{
 */

 /**
  * @file
  * @brief This file implements API to register port I/O emulation handlers for specific VM
  *	   and I/O instruction emulation internal functions for port read/write operations.
  *
  * External APIs:
  * - register_pio_emulation_handler()
  *
  * Internal functions:
  * - pio_default_read()
  * - pio_default_write()
  * - hv_emulate_pio()
  * - emulate_pio_complete()
  * - emulate_pio()
  */


/**
 * @brief Default handler for port I/O read request from guest VM.
 *
 *  This function fills a dummy value (all 1s)  for the I/O read request by default
 *  if there is no registered handler for this I/O port.
 *
 * @param [inout]  vcpu  Pointer to the context of virtual CPU that requests the I/O read.
 * @param [in]     addr  Unused parameter,I/O port number being accessed, the same dummy
 *			 value (all 1s) is used regardless of the port addresses, hence
 *			 this parameter is not used.
 * @param [in]     size  Size of port being accessed in bytes, this parameter shall be 1/2/4.
 *
 * @return None
 *
 * @pre size <= 4U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety True
 */
static void pio_default_read(struct acrn_vcpu *vcpu, __unused uint16_t addr, size_t size)
{
	/** Declare the following local variables of type struct pio_request *.
	 *  - pio_req representing a pointer to instance of struct pio_request,
	 *    initialized as &vcpu->req.reqs.pio.
	 */
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	/** Set pio_req->value to:
	 *  0xFF       if size is 1
	 *  0xFFFF     if size is 2
	 *  0xFFFFFFFF if size  is 4
	 */
	pio_req->value = (uint32_t)((1UL << (size * 8U)) - 1UL);
}

/**
 * @brief Default handler for port I/O write request from guest VM.
 *
 *  This is a dummy function defined following the definition of function pointer type
 *  of callback for port I/O write.
 *  In case that if no callback function is registered for I/O write, this default callback
 *  function shall be called and port I/O write shall be ignored, hence all parameters are
 *  not used.
 *
 * @param [inout]  vcpu  Unused parameter, pointer to the context of virtual CPU that
 *			 requests the I/O read.
 * @param [in]     addr  Unused parameter, I/O port being accessed, which is not used.
 * @param [in]     size Unused Parameter, Size of port being accessed in bytes.
 * @param [in]     v	 Unused parameter, The value to be written for the I/O port.
 *
 * @return None
 *
 * @pre size <= 4U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety True
 */
static void pio_default_write(
	__unused struct acrn_vcpu *vcpu, __unused uint16_t addr, __unused size_t size, __unused uint32_t v)
{
	/* ignore write */
}

/**
 * @brief This function handles the port I/O request by I/O handler registered to VMs or by
 *	  the default handlers when port range specific handlers are not registered.
 *
 * @param [in]     vcpu    Pointer to the instance of struct acrn_vcpu, which is the context of virtual
 *			   CPU that triggers the I/O access.
 * @param [inout]  io_req  Pointer to the instance of struct io_request, which holds I/O request
 *			   information such as port address, size of the access and value to
 *			   write(I/O Write) or buffer to hold read result(I/O Read).
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre io_req != NULL
 * @pre io_req->io_type == REQ_PORTIO
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu and /p io_req
 *		 are different among parallel invocations.
 */
static void hv_emulate_pio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	/** Declare the following local variables of type uint16_t.
	 *  - port representing I/O port address, not initialized. */
	uint16_t port;
	/** Declare the following local variables of type uint16_t.
	 *  - size representing the size of port being accessed, not initialized. */
	uint16_t size;
	/** Declare the following local variables of type uint32_t.
	 *  - idx representing index to vm->emul_pio[] array, not initialized. */
	uint32_t idx;
	/** Declare the following local variables of type struct acrn_vm *.
	 *  - vm representing pointer to an instance of struct acrn_vm, initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;
	/** Declare the following local variables of type struct pio_request *.
	 *  - pio_req representing a pointer to an instance of struct pio_request,
	 *    initialized as &io_req->reqs.pio
	 */
	struct pio_request *pio_req = &io_req->reqs.pio;
	/** Declare the following local variables of type struct vm_io_handler_desc *.
	 *  - handler representing a pointer to an instance of struct vm_io_handler_desc,
	 *    initialized as NULL. */
	struct vm_io_handler_desc *handler = NULL;
	/** Declare the following local variables of type io_read_fn_t.
	 *  - io_read representing callback handler for I/O port read, initialized as pio_default_read. */
	io_read_fn_t io_read = pio_default_read;
	/** Declare the following local variables of type io_write_fn_t.
	 *  - io_write representing callback handler for I/O port write, initialized as pio_default_write. */
	io_write_fn_t io_write = pio_default_write;

	/** Set port to the I/O port address being accessed from the guest VM */
	port = (uint16_t)pio_req->address;
	/** Set size to the size of I/O port being accessed from the guest VM */
	size = (uint16_t)pio_req->size;

	/** For each idx  ranging from 0 to EMUL_PIO_IDX_MAX - 1 with a step of 1 */
	for (idx = 0U; idx < EMUL_PIO_IDX_MAX; idx++) {
		/** Set handler to the address to (vm->emual_pio[idx)) */
		handler = &(vm->emul_pio[idx]);

		/**
		 * port being accessed from guest VM is less than the start port address
		 * of handler or  greater than or equal to the end port address of handler.
		 */
		if ((port < handler->port_start) || (port >= handler->port_end)) {
			/** Continue to next iteration */
			continue;
		}

		/** If io_read callback of handler is not NULL. */
		if (handler->io_read != NULL) {
			/** Set io_read to handler->io_read */
			io_read = handler->io_read;
		}
		/** If io_write callback of handler is not NULL. */
		if (handler->io_write != NULL) {
			/** Set io_write to handler->io_write */
			io_write = handler->io_write;
		}
		/** Terminate the loop */
		break;
	}

	/** If direction of pio_req is REQUEST_WRITE. */
	if (pio_req->direction == REQUEST_WRITE) {
		/** Call io_write with the following parameters, in order to emulate I/O port write.
		 *  - vcpu
		 *  - port
		 *  - size
		 *  - pio_req->value
		 */
		io_write(vcpu, port, size, pio_req->value);
	/** If direction of pio_req is REQUEST_READ. */
	} else if (pio_req->direction == REQUEST_READ) {
		/** Call io_read  with the following parameters, in order to emulate I/O port read.
		 *  - vcpu
		 *  - port
		 *  - size
		 */
		io_read(vcpu, port, size);
	} else {
		/* do nothing */
	}

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - port
	 *  - pio_req->value
	 */
	pr_dbg("IO %s on port %04x, data %08x", (pio_req->direction == REQUEST_READ) ? "read" : "write", port,
		pio_req->value);
}

/**
 * @brief This function updates RAX register of vcpu with the value returned by emualtion handler
 *	  if current request is port I/O read.
 *
 * @param [in]     vcpu    Pointer to the instance of struct acrn_vcpu, which is the context of virtual
 *			   CPU that triggers the I/O access.
 * @param [inout]  io_req  Pointer to the instance of struct io_request, which holds I/O request
 *			   information such as port address, size of the access and value to
 *			   write(I/O Write) or buffer to hold read result(I/O Read).
 *
 * @return None
 *
 * @pre vcpu != NULL
 * @pre io_req != NULL
 * @pre io_req->io_type == REQ_PORTIO
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu and /p io_req
 *		 are different among parallel invocations.
 *
 * @remark This function must be called when \p io_req is completed, after
 * a previous call to emulate_io() returning 0.
 */
static void emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	/**
	 * Declare the following local variables of type struct pio_request *.
	 *  - pio_req representing a pointer to an instance of struct pio_request,
	 *    initialized as &io_req->reqs.pio.
	 */
	const struct pio_request *pio_req = &io_req->reqs.pio;
	/**
	 * Declare the following local variables of type uint64_t.
	 *  - mask representing the bitmap for I/O request size,
	 * initialized as 0xFFFFFFFFUL >> (32UL - (8UL * pio_req->size)
	 */
	uint64_t mask = 0xFFFFFFFFUL >> (32UL - (8UL * pio_req->size));

	/** If the direction of current port I/O access is REQUEST_READ */
	if (pio_req->direction == REQUEST_READ) {
		/**
		 * Declare the following local variables of type uint64_t.
		 *  - value representing the value returned from I/O emulation handler,
		 * initialized as pio_req->value.
		 */
		uint64_t value = (uint64_t)pio_req->value;
		/**
		 * Declare the following local variables of type uint64_t.
		 *  - rax representing the value of general register CPU_REG_RAX of vcpu,
		 *  initialized as the return value of vcpu_get_gpreg(vcpu, CPU_REG_RAX).
		 */
		uint64_t rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);

		/** Set rax to ((rax) & ~mask) | (value & mask) */
		rax = ((rax) & ~mask) | (value & mask);
		/**
		 * Call vcpu_set_gpreg() with the following parameters,
		 * in order to set CPU_REG_RAX of vcpu.
		 *  - vcpu
		 *  - CPU_REG_RAX
		 *  - rax
		 */
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, rax);
	}
}

/**
 * @brief This function wraps hv_emulate_pio() and emulate_pio_complete() to handle port I/O
 *	  access from guest VM.
 *
 * @param [in]     vcpu    Pointer to the instance of struct acrn_vcpu, which is the context of virtual
 *			   CPU that triggers the I/O access.
 * @param [inout]  io_req  Pointer to the instance of struct io_request, which holds I/O request
 *			   information such as port address, size of the access and value to
 *			   write(I/O Write) or buffer to hold read result(I/O Read).
 *
 * @return None
 *
 * @pre io_req != NULL
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu and /p io_req
 *		 are different among parallel invocations.
 */
void emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	/** Call hv_emulate_pio() with the following parameters, in order to
	 *  emulate port I/O request.
	 *  - vcpu
	 *  - io_req
	 */
	hv_emulate_pio(vcpu, io_req);
	/** Call emulate_pio_complete() with the following parameters, in order to
	 *  update RAX register of vcpu if current request is port I/O read.
	 *  - vcpu
	 *  - io_req
	 */
	emulate_pio_complete(vcpu, io_req);
}

/**
 * @brief Register a port I/O handler
 *
 * @param [inout] vm		  Pointer to instance of struct acrn_vm for which the port I/O
 *				  handlers are registered
 * @param [in]    pio_idx	  The emulated port I/O address
 * @param [in]    range		  Pointer to instance of struct vm_io_range that contains
 *				  the emulated port I/O range
 * @param [in]    io_read_fn_ptr  The handler for emulating reads from the given range
 * @param [in]    io_write_fn_ptr The handler for emulating writes to the given range
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre range != NULL
 * @pre pio_idx < EMUL_PIO_IDX_MAX
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety True
 */
void register_pio_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx, const struct vm_io_range *range,
	io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr)
{
	/** Set vm->emul_pio[pio_idx].port_start to range->base */
	vm->emul_pio[pio_idx].port_start = range->base;
	/** Set vm->emul_pio[pio_idx].port_end to range->base + range->len */
	vm->emul_pio[pio_idx].port_end = range->base + range->len;
	/** Set vm->emul_pio[pio_idx].io_read to io_read_fn_ptr */
	vm->emul_pio[pio_idx].io_read = io_read_fn_ptr;
	/** Set vm->emul_pio[pio_idx].io_write to io_write_fn_ptr */
	vm->emul_pio[pio_idx].io_write = io_write_fn_ptr;
}

/**
 * @}
 */
