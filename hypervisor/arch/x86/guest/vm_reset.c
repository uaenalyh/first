/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <host_pm.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <vm_reset.h>

/**
 * @addtogroup vp-base_vm
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
 * @pre vm != NULL
 */
void fatal_error_shutdown_vm(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;
	/* Either SOS or pre-launched VMs */
	pause_vm(vm);

	per_cpu(shutdown_vm_id, pcpuid_from_vcpu(vcpu)) = vm->vm_id;
	make_shutdown_vm_request(pcpuid_from_vcpu(vcpu));
}

/*
 * Reset Control register at I/O port 0xcf9.
 *     Bit 1 - 0: "soft" reset. Force processor begin execution at power-on reset vector.
 *	     1: "hard" reset. e.g. assert PLTRST# (if implemented) to do a host reset.
 *     Bit 2 - initiates a system reset when it transitions from 0 to 1.
 *     Bit 3 - 1: full reset (aka code reset), SLP_S3#/4#/5# or similar pins are asserted for full power cycle.
 *	     0: will be dropped if system in S3/S4/S5.
 */

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));

	(void)shutdown_vm(vm);
}

/**
 * @}
 */
