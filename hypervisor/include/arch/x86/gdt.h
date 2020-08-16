/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GDT_H
#define GDT_H

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief Define structures and macros, declare APIs to hold and maintain segment descriptors.
 *
 * Following functionalities are provided in this file:
 *
 * 1.Define the structures to hold TSS descriptor and GDT descriptor.
 * 2.Define macros related to segment descriptors.
 */

/* GDT is defined in assembly so it can be used to switch modes before long mode
 * is established.
 * With 64-bit EFI this is not required since are already in long mode when EFI
 * transfers control to the hypervisor.  However, for any instantiation of the
 * ACRN Hypervisor that requires a boot from reset the GDT will be
 * used as mode transitions are being made to ultimately end up in long mode.
 * For this reason we establish the GDT in assembly.
 * This should not affect usage and convenience of interacting with the GDT in C
 * as the complete definition of the GDT is driven by the defines in this file.
 *
 * Unless it proves to be not viable we will use a single GDT for all hypervisor
 * CPUs, with space for per CPU LDT and TSS.
 */

/*
 * Segment selectors in x86-64 and i386 are the same size, 8 bytes.
 * Local Descriptor Table (LDT) selectors are 16 bytes on x86-64 instead of 8
 * bytes.
 * Task State Segment (TSS) selectors are 16 bytes on x86-64 instead of 8 bytes.
 */
/**
 * @brief The size(in bytes) of Segment Descriptors.
 */
#define X64_SEG_DESC_SIZE (0x8U)

/*****************************************************************************
 *
 * BEGIN: Definition of the GDT.
 *
 * NOTE:
 * If you change the size of the GDT or rearrange the location of descriptors
 * within the GDT you must change both the defines and the C structure header.
 *
 *****************************************************************************/
/**
 * @brief Number of global 8 bytes segment descriptors.
 */
#define HOST_GDT_RING0_SEG_SELECTORS (0x3U) /* rsvd, code, data */
/**
 * @brief Data Segment Selector of host.
 */
#define HOST_GDT_RING0_DATA_SEL      (0x0010U)
/* One for each CPU in the hypervisor. */

/*****************************************************************************
 *
 * END: Definition of the GDT.
 *
 *****************************************************************************/

/**
 * @brief Offset to start of LDT.
 */
#define HOST_GDT_RING0_LDT_SEL (HOST_GDT_RING0_SEG_SELECTORS * X64_SEG_DESC_SIZE)
/**
 * @brief TSS Selector of host.
 */
#define HOST_GDT_RING0_CPU_TSS_SEL (HOST_GDT_RING0_LDT_SEL)

#ifndef ASSEMBLER

#include <types.h>
#include <cpu.h>

/*
 * @brief The value of the type field of the TSS Descriptor. It indicates an inactive task.
 */
#define TSS_AVAIL (9U)

/*
 * Definition of 16 byte TSS and LDT selectors.
 */
/**
 * @brief This struct represents Task State Segment descriptor for the logical processor.
 *
 * @alignment 8
 */
struct tss_64_descriptor {
	uint32_t low32_value; /**< bits[31:0] of TSS descriptor in 64 bit mode. */
	uint32_t high32_value; /**< bits[63:32] of TSS descriptor in 64 bit mode. */
	uint32_t base_addr_63_32; /**< bits[95:64] of TSS descriptor in 64 bit mode. */
	uint32_t offset_12; /**< bits[127:96] of TSS descriptor in 64 bit mode. */
} __aligned(8);

/*****************************************************************************
 *
 * BEGIN: Definition of the GDT.
 *
 * NOTE:
 * If you change the size of the GDT or rearrange the location of descriptors
 * within the GDT you must change both the defines and the C structure header.
 *
 *****************************************************************************/
/**
 * @brief It represents the Global Descriptor Table used for the logical processor.
 *
 * @alignment 8
 */
struct host_gdt {
	uint64_t rsvd; /**< reserved descriptor. */

	uint64_t code_segment_descriptor; /**< represent the attribute for hypervisor code segment.. */
	uint64_t data_segment_descriptor; /**< represent the attribute for hypervisor data segment. */
	struct tss_64_descriptor host_gdt_tss_descriptors; /**< represent the TSS descriptor used by the logical
							    *   processor. */
} __aligned(8);

/*****************************************************************************
 *
 * END: Definition of the GDT.
 *
 *****************************************************************************/

/*
 * x86-64 Task State Segment (TSS) definition.
 */
/**
 * @brief Task State Segment (TSS) definition.
 *
 * This structure is packed.
 * @alignment: 16
 */
struct tss_64 {
	uint32_t rsvd1; /**< reserved bit. */
	uint64_t rsp0;  /**< RING0's RSP. */
	uint64_t rsp1;  /**< RING1's RSP. */
	uint64_t rsp2;  /**< RING2's RSP. */
	uint32_t rsvd2; /**< reserved bit. */
	uint32_t rsvd3; /**< reserved bit. */
	uint64_t ist1;  /**< The address of machine check stack. */
	uint64_t ist2;  /**< The address of double fault stack. */
	uint64_t ist3;  /**< The address of segment fault stack. */
	uint64_t ist4;  /**< exception stack address in TSS. */
	uint64_t ist5;  /**< exception stack address in TSS, not used currently. */
	uint64_t ist6;  /**< exception stack address in TSS, not used currently. */
	uint64_t ist7;  /**< exception stack address in TSS, not used currently. */
	uint32_t rsvd4; /**< reserved bit. */
	uint32_t rsvd5; /**< reserved bit. */
	uint16_t rsvd6; /**< reserved bit. */
	uint16_t io_map_base_addr; /**< I/O Map Base Address. */
} __packed __aligned(16);

/**
 * @brief Definition of the GDT descriptor.
 *
 * Representing the contents to be loaded to GDTR.
 * This structure is packed.
 */
struct host_gdt_descriptor {
	uint16_t len; /**< It represents the limit (size of table in bytes) of the global descriptor table (GDT). */
	struct host_gdt *gdt; /**< It represents the base address of the global descriptor table (GDT). */
} __packed;

void load_gdtr_and_tr(void);

#endif /* end #ifndef ASSEMBLER */

/**
 * @}
 */

#endif /* GDT_H */
