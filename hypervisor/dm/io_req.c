/*
 * Copyright (C) 2019 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <irq.h>
#include <errno.h>
#include <logmsg.h>

/**
 * @defgroup vp-dm_io_req vp-dm.io_req
 * @ingroup vp-dm
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

/**
 * @pre width < 8U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pio_default_read(struct acrn_vcpu *vcpu, __unused uint16_t addr, size_t width)
{
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	pio_req->value = (uint32_t)((1UL << (width * 8U)) - 1UL);

	return true;
}

/**
 * @pre width < 8U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pio_default_write(
	__unused struct acrn_vcpu *vcpu, __unused uint16_t addr, __unused size_t width, __unused uint32_t v)
{
	return true; /* ignore write */
}

/**
 * Try handling the given request by any port I/O handler registered in the
 * hypervisor.
 *
 * @pre io_req->io_type == REQ_PORTIO
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval -ENODEV No proper handler found.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 */
static int32_t hv_emulate_pio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	uint16_t port, size;
	uint32_t idx;
	struct acrn_vm *vm = vcpu->vm;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vm_io_handler_desc *handler;
	io_read_fn_t io_read = NULL;
	io_write_fn_t io_write = NULL;

	io_read = pio_default_read;
	io_write = pio_default_write;

	port = (uint16_t)pio_req->address;
	size = (uint16_t)pio_req->size;

	for (idx = 0U; idx < EMUL_PIO_IDX_MAX; idx++) {
		handler = &(vm->emul_pio[idx]);

		if ((port < handler->port_start) || (port >= handler->port_end)) {
			continue;
		}

		if (handler->io_read != NULL) {
			io_read = handler->io_read;
		}
		if (handler->io_write != NULL) {
			io_write = handler->io_write;
		}
		break;
	}

	if ((pio_req->direction == REQUEST_WRITE) && (io_write != NULL)) {
		if (io_write(vcpu, port, size, pio_req->value)) {
			status = 0;
		}
	} else if ((pio_req->direction == REQUEST_READ) && (io_read != NULL)) {
		if (io_read(vcpu, port, size)) {
			status = 0;
		}
	} else {
		/* do nothing */
	}

	pr_dbg("IO %s on port %04x, data %08x", (pio_req->direction == REQUEST_READ) ? "read" : "write", port,
		pio_req->value);

	return status;
}

/**
 * @brief Emulate \p io_req for \p vcpu
 *
 * Handle an I/O request by either invoking a hypervisor-internal handler or
 * deliver to VHM.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval IOREQ_PENDING The I/O request is delivered to VHM.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 * @retval -EINVAL \p io_req has an invalid io_type.
 * @retval <0 on other errors during emulation.
 */
int32_t emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vcpu->vm->vm_id);

	switch (io_req->io_type) {
	case REQ_PORTIO:
		status = hv_emulate_pio(vcpu, io_req);
		if (status == 0) {
			emulate_pio_complete(vcpu, io_req);
		}
		break;
	default:
		/* Unknown I/O request io_type */
		status = -EINVAL;
		break;
	}

	return status;
}

/**
 * @brief Register a port I/O handler
 *
 * @param vm      The VM to which the port I/O handlers are registered
 * @param pio_idx The emulated port io index
 * @param range   The emulated port io range
 * @param io_read_fn_ptr The handler for emulating reads from the given range
 * @param io_write_fn_ptr The handler for emulating writes to the given range
 * @pre pio_idx < EMUL_PIO_IDX_MAX
 */
void register_pio_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx, const struct vm_io_range *range,
	io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr)
{
	vm->emul_pio[pio_idx].port_start = range->base;
	vm->emul_pio[pio_idx].port_end = range->base + range->len;
	vm->emul_pio[pio_idx].io_read = io_read_fn_ptr;
	vm->emul_pio[pio_idx].io_write = io_write_fn_ptr;
}

/**
 * @}
 */
