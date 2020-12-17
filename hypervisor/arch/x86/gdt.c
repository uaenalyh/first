/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <gdt.h>
#include <per_cpu.h>

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief This file implement APIs to set TSS descriptor and load GDTR and TR.
 *
 * This file is decomposed into the following functions:
 *
 * - set_tss_desc: Set TSS descriptor.
 * - load_gdt(gdtr): Load \a gdtr into the GDTR.
 * - load_gdtr_and_tr: Load GDTR and TR.
 */

/**
 * @brief Set TSS descriptor.
 *
 * @param[inout]    desc TSS descriptor used to be set.
 * @param[in]       tss Specify the base address of TSS.
 * @param[in]       tss_limit Specify the size of TSS.
 * @param[in]       type TSS Type.
 *
 * @return None
 *
 * @pre desc != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety When \a desc is different among parallel invocation.
 */
static void set_tss_desc(struct tss_64_descriptor *desc, uint64_t tss, size_t tss_limit, uint32_t type)
{
	/** Declare the following local variables of type uint32_t.
	 *  - u1 representing a helper 32-bit value (high 16-bit is bits 15:0 of TSS descriptor base address,
	 *  low 16-bit is all 0s), not initialized.
	 *  - u2 representing a helper 32-bit value (high 8-bit is bits 24:31 of TSS descriptor base address,
	 *  low 24-bit is all 0s), not initialized.
	 *  - u3 representing a helper 32-bit value (low 8-bit is bits 16:23 of TSS descriptor base address,
	 *  low 24-bit is all 0s), not initialized. */
	uint32_t u1, u2, u3;
	/** Declare the following local variables of type uint32_t.
	 *  - tss_hi_32 representing 32 bits of TSS descriptor base address from 32 to 63,
	 *  initialized as bits 31:63 of \a tss */
	uint32_t tss_hi_32 = (uint32_t)(tss >> 32U);
	/** Declare the following local variables of type uint32_t.
	 *  - tss_lo_32 representing 32 bits of tss descriptor base address from 0 to 31,
	 * initialized as bits 0:31 of \a tss */
	uint32_t tss_lo_32 = (uint32_t)tss;

	/** Set u1 to the value, where the value is calculated by shifting tss_lo_32 to the left by 16. */
	u1 = tss_lo_32 << 16U;
	/** Set u2 to the value, where the value is calculated by bitwise AND tss_lo_32 by FF000000H. */
	u2 = tss_lo_32 & 0xFF000000U;
	/** Set u3 to the value, where the value is calculated by shifting the v to the right by 16, where the v is
	 *  calculated by bitwise AND tss_lo_32 by 00FF0000H. */
	u3 = (tss_lo_32 & 0x00FF0000U) >> 16U;

	/** Set low32_value field of desc to the value, where the value is calculated by bitwise OR u1 by the v, where
	 *  the v is calculated by bitwise AND tss_limit by FFFFH. */
	desc->low32_value = u1 | (tss_limit & 0xFFFFU);
	/** Set base_addr_63_32 field of desc to tss_hi_32. */
	desc->base_addr_63_32 = tss_hi_32;

	/** Set high32_value of desc to the value , where the value is calculated by bitwise OR u2, 0x8000U (which is
	 *  the present bit in TSS descriptor), u3, and the v togerther, where the v is calculated by shifting type to
	 *  left by 8. */
	desc->high32_value = u2 | (type << 8U) | 0x8000U | u3;
}

/**
 * @brief Load the given descriptor into the GDTR.
 *
 * @param[in]    gdtr A pointer to the descriptor (containing the limit and base of the GDT) to be loaded to GDTR.
 *
 * @return None
 *
 * @pre gdtr != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void load_gdt(struct host_gdt_descriptor *gdtr)
{
	/** Execute lgdt instruction to load the descriptor (containing the limit and base of the GDT) pointed by
	 *  \a lgdt into the GDTR
	 *  - Input operands: a memory operand holds the value pointed by \a gdtr
	 *  - Output operands: None
	 *  - Clobbers: None */
	asm volatile("lgdt %0" : : "m"(*gdtr));
}

/**
 * @brief Load GDTR and TR.
 *
 * Load gdt field of per-CPU region into the GDTR, and load tss field of GDT into the TR.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void load_gdtr_and_tr(void)
{
	/** Declare the following local variables of type struct host_gdt *.
	 *  - gdt representing a pointer the GDT to be constructed for current processor, initialized
	 *    as &get_cpu_var(gdt). */
	struct host_gdt *gdt = &get_cpu_var(gdt);
	/** Declare the following local variables of type struct host_gdt_descriptor.
	 *  - gdtr representing the GDT descriptor, not initialized. */
	struct host_gdt_descriptor gdtr;
	/** Declare the following local variables of type struct tss_64 *.
	 *  - tss representing a pointer the TSS to be constructed for current processor, initialized
	 *    as &get_cpu_var(tss). */
	struct tss_64 *tss = &get_cpu_var(tss);

	/** Set rsvd field of gdt to AAAAAAAAAAAAAAAAH, as the first entry in GDT is not used */
	gdt->rsvd = 0xAAAAAAAAAAAAAAAAUL;
	/* ring 0 code sel descriptor */
	/** Set code_segment_descriptor field of gdt to 00Af9b000000ffffH, the magic number here represented that
	 *  the segment is a 64-bit code segment(DPL is 0) with executed/read/accessed authority which mapped address
	 *  from host address 0 to host address 4GB. */
	gdt->code_segment_descriptor = 0x00Af9b000000ffffUL;
	/* ring 0 data sel descriptor */
	/** Set data_segment_descriptor field of gdt to 00cf93000000ffffH, the magic number here represented that
	 *  the segment is a 32-bit data segment(DPL is 0) with read/write/accessed authority which mapped address from
	 *  host address 0 to host address 4GB. */
	gdt->data_segment_descriptor = 0x00cf93000000ffffUL;

	/** Set ist1 field of tss to the value, where the value is CONFIG_STACK_SIZE plus the return value of
	 *  get_cpu_var with parameter mc_stack. */
	tss->ist1 = (uint64_t)get_cpu_var(mc_stack) + CONFIG_STACK_SIZE;
	/** Set ist2 field of tss to the value, where the value is CONFIG_STACK_SIZE plus the return value of
	 *  get_cpu_var with parameter df_stack. */
	tss->ist2 = (uint64_t)get_cpu_var(df_stack) + CONFIG_STACK_SIZE;
	/** Set ist3 field of tss to the value, where the value is CONFIG_STACK_SIZE plus the return value of
	 *  get_cpu_var with parameter sf_stack. */
	tss->ist3 = (uint64_t)get_cpu_var(sf_stack) + CONFIG_STACK_SIZE;
	/** Set ist4 field of tss to 0. */
	tss->ist4 = 0UL;

	/** Call set_tss_desc with the following parameters,
	 *  in order to set up the TSS descriptor specified by 'gdt->host_gdt_tss_descriptors' with the given
	 *  information,
	 *  including base address, type, etc.
	 *  - &gdt->host_gdt_tss_descriptors
	 *  - tss
	 *  - sizeof(struct tss_64)
	 *  - TSS_AVAIL */
	set_tss_desc(&gdt->host_gdt_tss_descriptors, (uint64_t)tss, sizeof(struct tss_64), TSS_AVAIL);

	/** Set len field of gdtr to the value, where the value is sizeof(struct host_gdt) minus 1. */
	gdtr.len = sizeof(struct host_gdt) - 1U;
	/** Set gdt field of gdtr to gdt. */
	gdtr.gdt = gdt;

	/** Call load_gdt with the following parameters,
	 *  in order to load the descriptor 'gdtr' (containing the limit and base of the GDT) into GDTR.
	 *  - &gdtr */
	load_gdt(&gdtr);

	/** Call CPU_LTR_EXECUTE with the following parameters,
	 *  in order to load HOST_GDT_RING0_CPU_TSS_SEL into the segment selector field of the task register.
	 *  - HOST_GDT_RING0_CPU_TSS_SEL */
	CPU_LTR_EXECUTE(HOST_GDT_RING0_CPU_TSS_SEL);
}

/**
 * @}
 */
