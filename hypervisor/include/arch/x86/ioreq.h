/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOREQ_H
#define IOREQ_H

#include <types.h>
#include <acrn_common.h>

/* Internal representation of a I/O request. */
struct io_request {
	/** Type of the request (PIO, MMIO, etc). Refer to vhm_request. */
	uint32_t type;

	/** Status of request handling. Written by request handlers and read by
	 * the I/O emulation framework. Refer to vhm_request. */
	int32_t processed;

	/** Details of this request in the same format as vhm_request. */
	union vhm_io_request reqs;
};

/* Definition of a IO port range */
struct vm_io_range {
	uint16_t base;		/* IO port base */
	uint16_t len;		/* IO port range */
	uint32_t flags;		/* IO port attributes */
};

struct vm_io_handler;
struct vm;
struct vcpu;

typedef
uint32_t (*io_read_fn_t)(struct vm_io_handler *, struct vm *,
				uint16_t, size_t);

typedef
void (*io_write_fn_t)(struct vm_io_handler *, struct vm *,
				uint16_t, size_t, uint32_t);

/* Describes a single IO handler description entry. */
struct vm_io_handler_desc {

	/** The base address of the IO range for this description. */
	uint16_t addr;
	/** The number of bytes covered by this description. */
	size_t len;

	/** A pointer to the "read" function.
	 *
	 * The read function is called from the hypervisor whenever
	 * a read access to a range described in "ranges" occur.
	 * The arguments to the callback are:
	 *
	 *    - The address of the port to read from.
	 *    - The width of the read operation (1,2 or 4).
	 *
	 * The implementation must return the ports content as
	 * byte, word or doubleword (depending on the width).
	 *
	 * If the pointer is null, a read of 1's is assumed.
	 */

	io_read_fn_t io_read;
	/** A pointer to the "write" function.
	 *
	 * The write function is called from the hypervisor code
	 * whenever a write access to a range described in "ranges"
	 * occur. The arguments to the callback are:
	 *
	 *   - The address of the port to write to.
	 *   - The width of the write operation (1,2 or 4).
	 *   - The value to write as byte, word or doubleword
	 *     (depending on the width)
	 *
	 * The implementation must write the value to the port.
	 *
	 * If the pointer is null, the write access is ignored.
    */

	io_write_fn_t io_write;
};

struct vm_io_handler {
	struct vm_io_handler *next;
	struct vm_io_handler_desc desc;
};

#define IO_ATTR_R               0U
#define IO_ATTR_RW              1U
#define IO_ATTR_NO_ACCESS       2U

/* Typedef for MMIO handler and range check routine */
struct mmio_request;
typedef int (*hv_mem_io_handler_t)(struct vcpu *, struct io_request *, void *);

/* Structure for MMIO handler node */
struct mem_io_node {
	hv_mem_io_handler_t read_write;
	void *handler_private_data;
	struct list_head list;
	uint64_t range_start;
	uint64_t range_end;
};

/* External Interfaces */
int32_t pio_instr_vmexit_handler(struct vcpu *vcpu);
void   setup_io_bitmap(struct vm *vm);
void   free_io_emulation_resource(struct vm *vm);
void   allow_guest_io_access(struct vm *vm, uint32_t address_arg, uint32_t nbytes);
void   register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr);
int dm_emulate_pio_post(struct vcpu *vcpu);

int register_mmio_emulation_handler(struct vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data);
void unregister_mmio_emulation_handler(struct vm *vm, uint64_t start,
        uint64_t end);
int dm_emulate_mmio_post(struct vcpu *vcpu);

int32_t acrn_insert_request_wait(struct vcpu *vcpu, struct io_request *req);

#endif /* IOREQ_H */
