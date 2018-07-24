/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/**
 * @pre io_req->type == REQ_PORTIO
 */
static int32_t
emulate_pio_post(struct vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;
	struct pio_request *pio_req = &io_req->reqs.pio;
	uint64_t mask = 0xFFFFFFFFUL >> (32UL - 8UL * pio_req->size);

	if (io_req->processed == REQ_STATE_SUCCESS) {
		if (pio_req->direction == REQUEST_READ) {
			uint64_t value = (uint64_t)pio_req->value;
			int32_t context_idx = vcpu->arch_vcpu.cur_context;
			struct run_context *cur_context;
			uint64_t *rax;

			cur_context = &vcpu->arch_vcpu.contexts[context_idx];
			rax = &cur_context->guest_cpu_regs.regs.rax;

			*rax = ((*rax) & ~mask) | (value & mask);
		}
		status = 0;
	} else {
		status = -1;
	}

	return status;
}

/**
 * @pre vcpu->req.type == REQ_PORTIO
 */
int32_t dm_emulate_pio_post(struct vcpu *vcpu)
{
	uint16_t cur = vcpu->vcpu_id;
	union vhm_request_buffer *req_buf = NULL;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	vhm_req = &req_buf->req_queue[cur];

	io_req->processed = vhm_req->processed;
	pio_req->value = vhm_req->reqs.pio.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	vhm_req->valid = 0;

	return emulate_pio_post(vcpu, io_req);
}

/**
 * Try handling the given request by any port I/O handler registered in the
 * hypervisor.
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @return 0       - Successfully emulated by registered handlers.
 * @return -ENODEV - No proper handler found.
 * @return -EIO    - The request spans multiple devices and cannot be emulated.
 */
int32_t
hv_emulate_pio(struct vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	uint16_t port, size;
	uint32_t mask;
	struct vm *vm = vcpu->vm;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vm_io_handler *handler;

	port = (uint16_t)pio_req->address;
	size = (uint16_t)pio_req->size;
	mask = 0xFFFFFFFFU >> (32U - 8U * size);

	for (handler = vm->arch_vm.io_handler;
		handler != NULL; handler = handler->next) {
		uint16_t base = handler->desc.addr;
		uint16_t end = base + (uint16_t)handler->desc.len;

		if ((port >= end) || (port + size <= base)) {
			continue;
		} else if (!((port >= base) && ((port + size) <= end))) {
			pr_fatal("Err:IO, port 0x%04x, size=%hu spans devices",
					port, size);
			status = -EIO;
			break;
		} else {
			if (pio_req->direction == REQUEST_WRITE) {
				handler->desc.io_write(handler, vm, port, size,
					pio_req->value & mask);

				pr_dbg("IO write on port %04x, data %08x", port,
					pio_req->value & mask);
			} else {
				pio_req->value = handler->desc.io_read(handler,
						vm, port, size);

				pr_dbg("IO read on port %04x, data %08x",
					port, pio_req->value);
			}
			/* TODO: failures in the handlers should be reflected
			 * here. */
			io_req->processed = REQ_STATE_SUCCESS;
			status = 0;
			break;
		}
	}

	return status;
}

/**
 * Handle an I/O request by either invoking a hypervisor-internal handler or
 * deliver to VHM.
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @return 0       - Successfully emulated by registered handlers.
 * @return -EIO    - The request spans multiple devices and cannot be emulated.
 * @return Negative on other errors during emulation.
 */
int32_t
emulate_io(struct vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;

	status = hv_emulate_pio(vcpu, io_req);

	if (status == -ENODEV) {
		/*
		 * No handler from HV side, search from VHM in Dom0
		 *
		 * ACRN insert request to VHM and inject upcall.
		 */
		status = acrn_insert_request_wait(vcpu, io_req);

		if (status != 0) {
			struct pio_request *pio_req = &io_req->reqs.pio;
			pr_fatal("Err:IO %s access to port 0x%04lx, size=%lu",
				(pio_req->direction != REQUEST_READ) ? "read" : "write",
				pio_req->address, pio_req->size);
		}
	}

	return status;
}

int32_t pio_instr_vmexit_handler(struct vcpu *vcpu)
{
	int32_t status;
	uint64_t exit_qual;
	int32_t cur_context_idx = vcpu->arch_vcpu.cur_context;
	struct run_context *cur_context;
	struct cpu_gp_regs *regs;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;

	exit_qual = vcpu->arch_vcpu.exit_qualification;
	cur_context = &vcpu->arch_vcpu.contexts[cur_context_idx];
	regs = &cur_context->guest_cpu_regs.regs;

	io_req->type = REQ_PORTIO;
	io_req->processed = REQ_STATE_PENDING;
	pio_req->size = VM_EXIT_IO_INSTRUCTION_SIZE(exit_qual) + 1UL;
	pio_req->address = VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual);
	if (VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual) == 0UL) {
		pio_req->direction = REQUEST_WRITE;
		pio_req->value = (uint32_t)regs->rax;
	} else {
		pio_req->direction = REQUEST_READ;
	}

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION,
		(uint32_t)pio_req->address,
		(uint32_t)pio_req->direction,
		(uint32_t)pio_req->size,
		(uint32_t)cur_context_idx);

	status = emulate_io(vcpu, io_req);

	/* io_req is hypervisor-private. For requests sent to VHM,
	 * io_req->processed will be PENDING till dm_emulate_pio_post() is
	 * called on vcpu resume. */
	if (status == 0) {
		if (io_req->processed != REQ_STATE_PENDING) {
			status = emulate_pio_post(vcpu, io_req);
		}
	}

	return status;
}

static void register_io_handler(struct vm *vm, struct vm_io_handler *hdlr)
{
	if (vm->arch_vm.io_handler != NULL) {
		hdlr->next = vm->arch_vm.io_handler;
	}

	vm->arch_vm.io_handler = hdlr;
}

static void empty_io_handler_list(struct vm *vm)
{
	struct vm_io_handler *handler = vm->arch_vm.io_handler;
	struct vm_io_handler *tmp;

	while (handler != NULL) {
		tmp = handler;
		handler = tmp->next;
		free(tmp);
	}
	vm->arch_vm.io_handler = NULL;
}

void free_io_emulation_resource(struct vm *vm)
{
	empty_io_handler_list(vm);

	/* Free I/O emulation bitmaps */
	free(vm->arch_vm.iobitmap[0]);
	free(vm->arch_vm.iobitmap[1]);
}

void allow_guest_io_access(struct vm *vm, uint32_t address_arg, uint32_t nbytes)
{
	uint32_t address = address_arg;
	uint32_t *b;
	uint32_t i;
	uint32_t a;

	b = vm->arch_vm.iobitmap[0];
	for (i = 0U; i < nbytes; i++) {
		if ((address & 0x8000U) != 0U) {
			b = vm->arch_vm.iobitmap[1];
		}
		a = address & 0x7fffU;
		b[a >> 5] &= ~(1 << (a & 0x1fU));
		address++;
	}
}

static void deny_guest_io_access(struct vm *vm, uint32_t address_arg, uint32_t nbytes)
{
	uint32_t address = address_arg;
	uint32_t *b;
	uint32_t i;
	uint32_t a;

	b = vm->arch_vm.iobitmap[0];
	for (i = 0U; i < nbytes; i++) {
		if ((address & 0x8000U) != 0U) {
			b = vm->arch_vm.iobitmap[1];
		}
		a = address & 0x7fffU;
		b[a >> 5U] |= (1U << (a & 0x1fU));
		address++;
	}
}

static struct vm_io_handler *create_io_handler(uint32_t port, uint32_t len,
				io_read_fn_t io_read_fn_ptr,
				io_write_fn_t io_write_fn_ptr)
{

	struct vm_io_handler *handler;

	handler = calloc(1, sizeof(struct vm_io_handler));

	if (handler != NULL) {
		handler->desc.addr = port;
		handler->desc.len = len;
		handler->desc.io_read = io_read_fn_ptr;
		handler->desc.io_write = io_write_fn_ptr;
	} else {
		pr_err("Error: out of memory");
	}

	return handler;
}

void setup_io_bitmap(struct vm *vm)
{
	/* Allocate VM architecture state and IO bitmaps A and B */
	vm->arch_vm.iobitmap[0] = alloc_page();
	vm->arch_vm.iobitmap[1] = alloc_page();

	ASSERT((vm->arch_vm.iobitmap[0] != NULL) &&
	       (vm->arch_vm.iobitmap[1] != NULL), "");

	if (is_vm0(vm)) {
		(void)memset(vm->arch_vm.iobitmap[0], 0x00, CPU_PAGE_SIZE);
		(void)memset(vm->arch_vm.iobitmap[1], 0x00, CPU_PAGE_SIZE);
	} else {
		/* block all IO port access from Guest */
		(void)memset(vm->arch_vm.iobitmap[0], 0xFF, CPU_PAGE_SIZE);
		(void)memset(vm->arch_vm.iobitmap[1], 0xFF, CPU_PAGE_SIZE);
	}
}

void register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr)
{
	struct vm_io_handler *handler = NULL;

	if (io_read_fn_ptr == NULL || io_write_fn_ptr == NULL) {
		pr_err("Invalid IO handler.");
		return;
	}

	if (is_vm0(vm)) {
		deny_guest_io_access(vm, range->base, range->len);
	}

	handler = create_io_handler(range->base,
			range->len, io_read_fn_ptr, io_write_fn_ptr);

	register_io_handler(vm, handler);
}
