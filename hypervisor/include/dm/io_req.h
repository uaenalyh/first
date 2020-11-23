/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOREQ_H
#define IOREQ_H

/**
 * @addtogroup vp-dm_io-req
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the external data structures and APIs for registering
 *	  port I/O emulation handler to a specific VM.
 */

#include <types.h>
#include <acrn_common.h>
#include <list.h>

/**
 * @brief Internal representation of a I/O request.
 *
 * @consistency n/a
 *
 * @alignment 8
 *
 */
struct io_request {
	union vhm_io_request reqs; /**< Details of IO request. */
};

/**
 * @brief Definition of a IO port range
 *
 * @consistency n/a
 *
 * @alignment 2
 *
 * @remark
 */
struct vm_io_range {
	uint16_t base; /**< IO port base */
	uint16_t len; /**< IO port range */
} __aligned(2);

struct vm_io_handler_desc;
struct acrn_vm;
struct acrn_vcpu;

/**
 * @brief Callback function type for port I/O read emulation.
 *
 *  Callback function with this type shall be registered, in order to emulate port I/O
 *  read for specific port range for guest VM.
 *
 * @param [inout]  vcpu  Pointer to the context of virtual CPU that requests the I/O read.
 * @param [in]     addr  I/O port being accessed, the same dummy is used
 *			 regardless of the port addresses, hence this parameter is not used.
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
 * @threadsafety This function is thread-safe under condition that /p vcpu is
 *		 different among parallel invocations.
 */
typedef void (*io_read_fn_t)(struct acrn_vcpu *vcpu, uint16_t port, size_t size);

/**
 * @brief Callback function type handler for port I/O write emulation.
 *
 *  Callback function with this type shall be registered, in order to emulate port I/O
 *  write for specific port range for guest VM.
 *
 * @param [inout]  vcpu  Pointer to the context of virtual CPU that
 *			 requests the I/O read.
 * @param [in]     addr  I/O port being accessed, which is not used.
 * @param [in]     size  Size of port being accessed in bytes.
 * @param [in]     v	 The value to be written for the I/O port.
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
 * @threadsafety This function is thread safe under condition that /p vcpu is
 *		 different among parallel invocations.
 */
typedef void (*io_write_fn_t)(struct acrn_vcpu *vcpu, uint16_t port, size_t size, uint32_t val);

/**
 * @brief Describes a single port I/O handler description entry.
 *
 * @consistency n/a
 *
 * @alignment 8
 *
 * @remark The size of the read/write operation shall be 1,2 or 4.
 */
struct vm_io_handler_desc {

	uint16_t port_start; /**< Base port number of the I/O range. */

	uint16_t port_end; /**< Last port number of the I/O range. */

	/**
	 * @brief A pointer to the "read" function.
	 *
	 * The read function is called from the hypervisor whenever
	 * a read access to a range described in "ranges" occur.
	 * The arguments to the callback are:
	 *
	 *    - The address of the port to read from.
	 *    - The size of the read operation (1,2 or 4).
	 *
	 * The implementation must return the ports content as
	 * byte, word or doubleword (depending on the size).
	 *
	 * If the pointer is null, a read of 1's is assumed.
	 */
	io_read_fn_t io_read;

	/**
	 * @brief A pointer to the "write" function.
	 *
	 * The write function is called from the hypervisor code
	 * whenever a write access to a range described in "ranges"
	 * occur. The arguments to the callback are:
	 *
	 *   - The address of the port to write to.
	 *   - The size of the write operation (1,2 or 4).
	 *   - The value to write as byte, word or doubleword
	 *     (depending on the size)
	 *
	 * The implementation must write the value to the port.
	 *
	 * If the pointer is null, the write access is ignored.
	 */
	io_write_fn_t io_write;
} __aligned(8);

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
void emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req);

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
	io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr);

/**
 * @}
 */

/**
 * @}
 */

#endif /* IOREQ_H */
