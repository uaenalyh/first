/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vm.h>
#include <vcpu.h>
#include <io_req.h>

static struct vm_io_range testdev_range = {
	.base = 0xf4U,
	.len = 4U,
};

static void
testdev_io_read(__unused struct acrn_vcpu *vcpu, __unused uint16_t port, __unused size_t size)
{
}

static void
testdev_io_write(struct acrn_vcpu *vcpu, __unused uint16_t port, __unused size_t size, __unused uint32_t vali)
{
	uint16_t i;
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vcpu *vcpu_tmp = NULL;

	foreach_vcpu(i, vm, vcpu_tmp) {
		pause_vcpu(vcpu_tmp, VCPU_PAUSED);
	}
}

void
register_testdev(struct acrn_vm *vm)
{
	/* Hack the CF9 handler */
	register_pio_emulation_handler(vm, CF9_PIO_IDX, &testdev_range, testdev_io_read, testdev_io_write);
}
