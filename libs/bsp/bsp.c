/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp.h>

#include <cpu.h>
#include <logmsg.h>

static uint32_t get_lapic_id(void)
{
	uint32_t ebx;
	asm volatile("cpuid" : "=b"(ebx)
			: "a" (1)
			: "memory");
	return (ebx >> 24);
}

void bsp_init(void)
{
	pr_info("%s: lapic id = %x\n", __func__, get_lapic_id());
}

void bsp_fatal_error(void)
{
	pr_err("%s: lapic id = %x\n", __func__, get_lapic_id());

	/* Halt the current processor */
	while(1) {
		asm_hlt();
	}
}
