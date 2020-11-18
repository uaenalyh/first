/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)segments.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#ifndef CPU_H
#define CPU_H

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief Defines macros, data structures and APIs related to the register and the CPU status.
 *
 * Following functionalities are provided in this file:
 *
 * 1.Define register related macros.
 * 2.Define the enumeration type to identify the architecturally defined registers.
 * 3.Define the enumeration type to identify the physical CPU's state.
 * 4.Define the structures to store the information of descriptor table.
 * 5.Implement register related APIS to read/write MSR, load/store idt, load/store GDT.
 * 6.Implement CPU status related APIS to pause/halt CPU, disable/enable interrupt.
 *
 * This file is decomposed into the following functions:
 *
 * CPU_SEG_READ(seg, result_ptr) - Read from the specified segment \a seg and save the content to 16-bit address
 *                                 \a result_ptr.
 * CPU_CR_READ(cr, result_ptr)   - Read the content of the control register specified by \a cr and save the content to
 *                                 \a result_ptr.
 * CPU_CR_WRITE(cr, value)       - Write the content specified by \a value into the control register specified by
 *                                 \a cr.
 * sgdt                          - Get the base address of the GDT table.
 * sidt                          - Get the base address of the IDT.
 * asm_pause                     - Pause the current CPU.
 * asm_hlt                       - Stop instruction execution and place the processor in a HALT state.
 * CPU_IRQ_DISABLE               - Disable interrupts on the current CPU.
 * CPU_IRQ_ENABLE                - Enable interrupts on the current CPU.
 * cpu_write_memory_barrier      - Synchronize all write and read accesses to memory.
 * CPU_LTR_EXECUTE               - Load Task Register.
 * CPU_RFLAGS_SAVE               - Save rflags register.
 * CPU_RFLAGS_RESTORE            - Restore rflags register.
 * CPU_INT_ALL_DISABLE           - Save the current architecture status register to the specified address.
 * CPU_INT_ALL_RESTORE(rflags)   - Restore the architecture status register used to lockout interrupts to the specified
 *                                 value \a rflags.
 * get_pcpu_id                   - Get the current physical CPU ID.
 * msr_read                      - Read MSR.
 * msr_write                     - Write MSR.
 * write_xcr(reg, val)           - Write value into the XCR specified by \a reg.
 * read_xcr(reg)                 - Read value from the XCR specified by \a reg.
 * stac                          - Execute stac instruction.
 * clac                          - Execute clac instruction.
 */
#include <types.h>
#include <util.h>
#include <acrn_common.h>

/**
 * @brief The alignment of stack.
 */
#define CPU_STACK_ALIGN 16UL
/**
 * @brief CR0 paging enable bit.
 */
#define CR0_PG (1UL << 31U)
/**
 * @brief CR0 cache disable bit.
 */
#define CR0_CD (1UL << 30U)
/**
 * @brief CR0 not write through bit.
 */
#define CR0_NW (1UL << 29U)
/**
 * @brief CR0 alignment mask bit.
 */
#define CR0_AM (1UL << 18U)
/**
 * @brief CR0 write protect bit.
 */
#define CR0_WP (1UL << 16U)
/**
 * @brief CR0 numeric error bit.
 */
#define CR0_NE (1UL << 5U)
/**
 * @brief CR0 extension type bit.
 */
#define CR0_ET (1UL << 4U)
/**
 * @brief CR0 task switched bit.
 */
#define CR0_TS (1UL << 3U)
/**
 * @brief CR0 emulation bit.
 */
#define CR0_EM (1UL << 2U)
/**
 * @brief CR0 monitor coprocessor bit.
 */
#define CR0_MP (1UL << 1U)
/**
 * @brief CR0 protected mode enabled bit.
 */
#define CR0_PE (1UL << 0U)
/**
 * @brief CR4 virtual 8086 mode extensions bit.
 */
#define CR4_VME (1UL << 0U)
/**
 * @brief CR4 protected mode virtual interrupts bit.
 */
#define CR4_PVI (1UL << 1U)
/**
 * @brief CR4 time stamp disable bit.
 */
#define CR4_TSD (1UL << 2U)
/**
 * @brief CR4 debugging extensions bit.
 */
#define CR4_DE  (1UL << 3U)
/**
 * @brief CR4 page size extensions bit.
 */
#define CR4_PSE (1UL << 4U)
/**
 * @brief CR4 physical address extensions bit.
 */
#define CR4_PAE (1UL << 5U)
/**
 * @brief CR4 machine check enable bit.
 */
#define CR4_MCE (1UL << 6U)
/**
 * @brief CR4 page global enable bit.
 */
#define CR4_PGE (1UL << 7U)
/**
 * @brief CR4 performance monitoring counter enable bit.
 */
#define CR4_PCE (1UL << 8U)
/**
 * @brief CR4 OS support for FXSAVE/FXRSTOR bit.
 */
#define CR4_OSFXSR     (1UL << 9U)
/**
 * @brief CR4 OS support for unmasked SIMD floating point exceptions bit.
 */
#define CR4_OSXMMEXCPT (1UL << 10U)
/**
 * @brief CR4 user-mode instruction prevention bit.
 */
#define CR4_UMIP     (1UL << 11U)
/**
 * @brief CR4 VMX enable bit.
 */
#define CR4_VMXE     (1UL << 13U)
/**
 * @brief CR4 SMX enable bit.
 */
#define CR4_SMXE     (1UL << 14U)
/**
 * @brief CR4 bits that used to enable the instructions RDFSBASE, RDGSBASE, WRFSBASE, and WRGSBASE.
 */
#define CR4_FSGSBASE (1UL << 16U)
/**
 * @brief CR4 PCID enable bit.
 */
#define CR4_PCIDE    (1UL << 17U)
/**
 * @brief CR4 XSAVE and Processor Extended States enable bit bit.
 */
#define CR4_OSXSAVE  (1UL << 18U)
/**
 * @brief CR4 Supervisor-Mode Execution Prevention enable bit.
 */
#define CR4_SMEP (1UL << 20U)
/**
 * @brief CR4 Supervisor-Mode Access Prevention enable bit.
 */
#define CR4_SMAP (1UL << 21U)
/**
 * @brief CR4 Protect-key-enable bit.
 */
#define CR4_PKE  (1UL << 22U)

/**
 * @brief XCR0 SSE state.
 *
 * If the SSE state is enabled, the XSAVE feature set can be used to manage MXCSR and the XMM registers.
 */
#define XCR0_SSE (1UL << 1U)
/**
 * @brief XCR0 AVX state.
 *
 * If the AVX state is enabled, AVX instructions can be executed and the XSAVE feature set can be used to manage the
 * upper halves of the YMM registers.
 */
#define XCR0_AVX (1UL << 2U)
/**
 * @brief XCR0 BNDREGS state.
 *
 * If the BNDREGS state is enabled, MPX instructions can be executed and the XSAVE feature set can be used to
 * manage the bounds registers BND0–BND3.
 */
#define XCR0_BNDREGS (1UL << 3U)
/**
 * @brief XCR0 BNDCSR state.
 *
 * If the BNDCSR state is enabled, MPX instructions can be executed and the XSAVE feature set can be used to
 * manage the BNDCFGU and BNDSTATUS registers
 */
#define XCR0_BNDCSR (1UL << 4U)
/**
 * @brief XCR0 reserved bits
 *
 */
#define XCR0_RESERVED_BITS ((~((1UL << 10U) - 1UL)) | (1UL << 8U))
/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
/**
 * @brief Divide Error vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_DE  0U
/**
 * @brief Debug vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_DB  1U
/**
 * @brief Nonmaskable External Interrupt vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_NMI 2U
/**
 * @brief Breakpoint vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_BP  3U
/**
 * @brief Overflow vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_OF  4U
/**
 * @brief Undefined/Invalid Opcode vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_UD  6U
/**
 * @brief Double Fault vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_DF  8U
/**
 * @brief Invalid TSS vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_TS  10U
/**
 * @brief Segment Not Present vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_NP  11U
/**
 * @brief Stack Segment Fault vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_SS  12U
/**
 * @brief General Protection Fault vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_GP  13U
/**
 * @brief Page Fault vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_PF  14U
/**
 * @brief Machine Check vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_MC  18U
/**
 * @brief Virtualization Exception vector in the Interrupt Descriptor Table (IDT).
 */
#define IDT_VE  20U

/**
 * @brief Bootstrap Processor ID
 */
#define BOOT_CPU_ID 0U

/**
 * @brief Number of general-purpose registers saved/restored for vCPU.
 */
#define NUM_GPRS 16U
/**
 * @brief Maximum size in bytes of the XSAVE area.
 */
#define XSAVE_STATE_AREA_SIZE  4096U
/**
 * @brief The size of the legacy region of an XSAVE area.
 */
#define XSAVE_LEGACY_AREA_SIZE 512U
/**
 * @brief The size of the XSAVE header of an XSAVE area.
 */
#define XSAVE_HEADER_AREA_SIZE 64U
/**
 * @brief The size of extended region of an XSAVE area.
 */
#define XSAVE_EXTEND_AREA_SIZE (XSAVE_STATE_AREA_SIZE - XSAVE_HEADER_AREA_SIZE - XSAVE_LEGACY_AREA_SIZE)
/**
 * @brief The highest bit of XCOMP_BV in the header of XSAVE area.
 *
 * Indicates the format of the extended region of the XSAVE area.
 * If this bit is clear, the format is standard format;
 * If this bit is set, the format is compacted format.
 */
#define XSAVE_COMPACTED_FORMAT (1UL << 63U)
/**
 * @brief The initial value of XCR0.
 */
#define XCR0_INIT		1UL
/**
 * @brief The initial value of IA32_XSS.
 */
#define XSS_INIT		0UL

#ifndef ASSEMBLER

/**
 * @brief The Application Processors mask bit.
 *
 * This macro only used to boot APs. Each bit in the AP_MASK representing the physical CPU that will be started.
 */
#define AP_MASK (((1UL << get_pcpu_nums()) - 1UL) & ~(1UL << 0U))

/**
 *
 * Identifiers for architecturally defined registers.
 *
 * These register names is used in condition statement.
 * Within the following groups,register name need to be
 * kept in order:
 * General register names group (CPU_REG_RAX~CPU_REG_R15);
 * Non general register names group (CPU_REG_CR0~CPU_REG_GDTR);
 * Segement register names group (CPU_REG_ES~CPU_REG_GS).
 */
/**
 * @brief This enumeration type defines identifiers for architecturally defined registers. The order of general-purpose
 * registers are kept the same as those in the structure acrn_gp_regs.
 */
enum cpu_reg_name {
	/* General purpose register layout should align with
	 * struct acrn_gp_regs
	 */
	CPU_REG_RAX, /**< The RAX general purpose register */
	CPU_REG_RCX, /**< The RCX general purpose register */
	CPU_REG_RDX, /**< The RDX general purpose register */
	CPU_REG_RBX, /**< The RBX general purpose register */
	CPU_REG_RSP, /**< The RSP general purpose register */
	CPU_REG_RBP, /**< The RBP general purpose register */
	CPU_REG_RSI, /**< The RSI general purpose register */
	CPU_REG_RDI, /**< The RDI general purpose register */
	CPU_REG_R8, /**< The R8 general purpose register */
	CPU_REG_R9, /**< The R9 general purpose register */
	CPU_REG_R10, /**< The R10 general purpose register */
	CPU_REG_R11, /**< The R11 general purpose register */
	CPU_REG_R12, /**< The R12 general purpose register */
	CPU_REG_R13, /**< The R13 general purpose register */
	CPU_REG_R14, /**< The R14 general purpose register */
	CPU_REG_R15, /**< The R15 general purpose register */

	CPU_REG_CR0, /**< The CR0 control register */
	CPU_REG_CR2, /**< The CR2 control register */
	CPU_REG_CR3, /**< The CR3 control register */
	CPU_REG_CR4, /**< The CR4 control register */
	CPU_REG_DR7, /**< The DR7 register */
	CPU_REG_RIP, /**< The RIP register */
	CPU_REG_RFLAGS, /**< The RFLAGS register */
	/*CPU_REG_NATURAL_LAST*/
	CPU_REG_EFER, /**< The IA32_EFER MSR */
	CPU_REG_PDPTE0, /**< The first page-directory-pointer table entry (PDPTE) */
	CPU_REG_PDPTE1, /**< The second page-directory-pointer table entry (PDPTE) */
	CPU_REG_PDPTE2, /**< The third page-directory-pointer table entry (PDPTE) */
	CPU_REG_PDPTE3, /**< The fourth page-directory-pointer table entry (PDPTE) */
	/*CPU_REG_64BIT_LAST,*/
	CPU_REG_ES, /**< The ES segment selector */
	CPU_REG_CS, /**< The CS segment selector */
	CPU_REG_SS, /**< The SS segment selector */
	CPU_REG_DS, /**< The DS segment selector */
	CPU_REG_FS, /**< The FS segment selector */
	CPU_REG_GS, /**< The GS segment selector */
	CPU_REG_LDTR, /**< The local descriptor table register (LDTR) */
	CPU_REG_TR, /**< The task register */
	CPU_REG_IDTR, /**< The interrupt descriptor table register (IDTR) */
	CPU_REG_GDTR /**< The global descriptor table register (GDTR) */
};

/* In trampoline range, hold the jump target which trampline will jump to */
extern uint64_t main_entry[1];
extern uint64_t secondary_cpu_stack[1];

/*
 * To support per_cpu access, we use a special struct "per_cpu_region" to hold
 * the pattern of per CPU data. And we allocate memory for per CPU data
 * according to multiple this struct size and pcpu number.
 *
 *   +-------------------+------------------+---+------------------+
 *   | percpu for pcpu0  | percpu for pcpu1 |...| percpu for pcpuX |
 *   +-------------------+------------------+---+------------------+
 *   ^		   ^
 *   |		   |
 *   <per_cpu_region size>
 *
 * To access per CPU data, we use:
 *   per_cpu_base_ptr + sizeof(struct per_cpu_region) * curr_pcpu_id
 *   + offset_of_member_per_cpu_region
 * to locate the per CPU data.
 */

/**
 * @brief The invalid CPU id.
 *
 * This macro is error code for error handling, this means that caller can't find a valid physical
 * CPU or virtual CPU.
 */
#define INVALID_CPU_ID 0xffffU

/**
 * @brief This data structure represents the content in system table pointer registers for the logical processor.
 *
 * The system table pointer registers are GDTR, LDTR, IDTR, task register.
 *
 * @remark This data structure is packed.
 */
struct descriptor_table {
	uint16_t limit; /**< A value is added to the base address to get the address of the last valid byte. */
	uint64_t base; /**< The start address of the system table or segment. */
} __packed;

/**
 * @brief Physical CPU's boot state: running or dead.
 */
enum pcpu_boot_state {
	PCPU_STATE_DEAD = 0U, /**< pcpu is dead */
	PCPU_STATE_RUNNING, /**< pcpu is running */
};

/**
 * @brief The message flag of CPU representing the CPU will do offline operation.
 */
#define NEED_OFFLINE     (1U)
/**
 * @brief The message flag of CPU representing the vm will shutdown, where the vm holds the CPU.
 */
#define NEED_SHUTDOWN_VM (2U)
void make_pcpu_offline(uint16_t pcpu_id);
bool need_offline(uint16_t pcpu_id);

/* Function prototypes */
void cpu_do_idle(void);
void cpu_dead(void);
void init_pcpu_pre(bool is_bsp);
/* The function should be called on the same CPU core as specified by pcpu_id,
 * hereby, pcpu_id is actually the current physical CPU id.
 */
void init_pcpu_post(uint16_t pcpu_id);
bool start_pcpus(uint64_t mask);
void wait_pcpus_offline(uint64_t mask);
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);

/**
 * @brief Read from the specified segment \a seg and save the content to 16-bit address \a result_ptr.
 *
 * @param[in]    seg The segment to read.
 * @param[inout] result_ptr The 16-bit address to save the value read from the segment.
 *
 * @return None
 *
 * @pre result_ptr != NULL
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use mov instruction to read segment register into value pointed by \a result_ptr.
 */
#define CPU_SEG_READ(seg, result_ptr)                                               \
	{                                                                           \
		asm volatile("mov %%" STRINGIFY(seg) ", %0" : "=r"(*(result_ptr))); \
	}

/**
 * @brief Read the content of the control register specified by \a cr and save the content to \a result_ptr.
 *
 * @param[in]    cr The control register that will be read from.
 * @param[inout] result_ptr The address that stores the content read from the control register.
 *
 * @pre result_ptr != NULL
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use mov instruction to read control register into value pointed by \a result_ptr.
 */
#define CPU_CR_READ(cr, result_ptr)                                                \
	{                                                                          \
		asm volatile("mov %%" STRINGIFY(cr) ", %0" : "=r"(*(result_ptr))); \
	}

/**
 * @brief Write the content specified by \a value into the control register specified by \a cr.
 *
 * @param[in]    cr The control register that the content will be written into.
 * @param[in]    value The value that needs to be written into the control register.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use mov instruction to write the content specified by \a value into the control register specified by \a cr.
 */
#define CPU_CR_WRITE(cr, value)                         \
	{                                               \
		asm volatile("mov %0, %%" STRINGIFY(cr) \
			     : /* No output */          \
			     : "r"(value));             \
	}

/**
 * @brief Get the base address of the GDT.
 *
 * @return The base address of the GDT.
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t sgdt(void)
{
	/** Declare the following local variables of type struct descriptor_table.
	 *  - gdtb representing the content in GDTR, initialized as all 0s. */
	struct descriptor_table gdtb = { 0U, 0UL };
	/** Execute sgdt in order to store the content of the GDTR into gdtb.
	 *  - Input operands: None
	 *  - Output operands: the content of the GDTR is stored to variable gdtb.
	 *  - Clobbers: memory */
	asm volatile("sgdt %0" : "=m"(gdtb)::"memory");
	/** Return the base field of gdtb */
	return gdtb.base;
}

/**
 * @brief Get the base address of the IDT.
 *
 * @return The base address of the IDT.
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t sidt(void)
{
	/** Declare the following local variables of type struct descriptor_table.
	 *  - idtb representing the content in IDTR, initialized as all 0s. */
	struct descriptor_table idtb = { 0U, 0UL };
	/** Execute sidt in order to store the content of the IDTR into idtb.
	 *  - Input operands: None
	 *  - Output operands: the content of the IDTR is stored to variable idtb.
	 *  - Clobbers: memory */
	asm volatile("sidt %0" : "=m"(idtb)::"memory");
	/** Return the base field of the idtb. */
	return idtb.base;
}

/**
 * @brief Pause the current CPU.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT, HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void asm_pause(void)
{
	/** Execute 'pause' instruction.
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("pause" ::: "memory");
}

/**
 * @brief Stop instruction execution and place the processor in a HALT state.
 *
 * @return None
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void asm_hlt(void)
{
	/** Execute hlt in order to stop instruction execution and place the processor in a HALT state.
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: None */
	asm volatile("hlt");
}

/**
 * @brief Disable interrupts on the current CPU.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use cli instruction to cause the processor to ignore maskable external interrupts.
 */
#define CPU_IRQ_DISABLE()                         \
	{                                         \
		asm volatile("cli\n" : : : "cc"); \
	}

/**
 * @brief Enable interrupts on the current CPU.
 *
 * @return None
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use sti instruction to allow the processor to respond to maskable hardware interrupts.
 */
#define CPU_IRQ_ENABLE()                          \
	{                                         \
		asm volatile("sti\n" : : : "cc"); \
	}

/**
 * @brief Synchronize all write and read accesses to memory.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void cpu_write_memory_barrier(void)
{
	/** Execute mfence in order to synchronize all write/read accesses to memory.
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("mfence\n" : : : "memory");
}

/**
 * @brief Load Task Register.
 *
 * @param[in]    ltr_ptr The segment selector field of the task register.
 *
 * @return None
 *
 * @pre ltr_ptr >= 8
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * Use ltr instruction to load \a ltr_ptr into the segment selector field of the task register.
 */
#define CPU_LTR_EXECUTE(ltr_ptr)                             \
	{                                                    \
		asm volatile("ltr %%ax\n" : : "a"(ltr_ptr)); \
	}

/**
 * @brief Save RFLAGS register.
 *
 * @param[inout]    rflags_ptr The address to save the RFLAGS register.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * 1. Use pushf instruction to push RFLAGS register onto the stack.
 * 2. Use pop instruction to pop 64-bit value into the value pointed by \a rflags_ptr.
 */

#define CPU_RFLAGS_SAVE(rflags_ptr)                                              \
	{                                                                        \
		asm volatile(" pushf");                                          \
		asm volatile(" pop %0" : "=r"(*(rflags_ptr)) : /* No inputs */); \
	}

/**
 * @brief Restore RFLAGS register.
 *
 * @param[in]    rflags The 64-bit value will be loaded into RFLAGS register.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * 1. Use push instruction to push \a rflags onto the stack.
 * 2. Use popf instruction to pop a 64-bit value into the RFLAGS register.
 */
#define CPU_RFLAGS_RESTORE(rflags)          \
	{                                   \
		asm volatile(" push %0\n\t" \
			     "popf	\n\t"    \
			     :              \
			     : "r"(rflags)  \
			     : "cc");       \
	}

/**
 * @brief Save the content in the RFLAGS register to the specified address and disable interrupts.
 *
 * @param[inout]    p_rflags The address to save the interrupt status register.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * 1. Call CPU_RFLAGS_SAVE with the following parameters, in order to save RFLAGS register.
 *    - p_rflags.
 *
 * 2. Call CPU_IRQ_DISABLE in order to disable interrupts on the current CPU.
 */
#define CPU_INT_ALL_DISABLE(p_rflags)      \
	{                                  \
		CPU_RFLAGS_SAVE(p_rflags); \
		CPU_IRQ_DISABLE();         \
	}

/**
 * @brief Restore the architecture status register used to lockout interrupts to the specified value \a rflags.
 *
 * @param[in]    rflags The value that the architectures status register used to lockout interrupts will be
 * restored to.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * 1. Call CPU_INT_ALL_RESTORE with the following parameters, in order to restore RFLAGS register.
 * - rflags.
 */
#define CPU_INT_ALL_RESTORE(rflags)         \
	{                                   \
		CPU_RFLAGS_RESTORE(rflags); \
	}

/**
 * @brief Get the current physical CPU ID.
 *
 * @return Return the current physical CPU ID.
 *
 * @pre N/A
 * @post N/A
 *
 * @remark This API can be called only after init_pcpu_pre has been called once on the current logical processor to set
 * MSR IA32_TSC_AUX.
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint16_t get_pcpu_id(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - tsl representing low 32 bit of the processor's time-stamp counter, not initialized.
	 *  - tsh representing high 32 bit of the processor's time-stamp counter, not initialized.
	 *  - cpu_id representing a physical CPU ID, not initialized. */
	uint32_t tsl, tsh, cpu_id;

	/** Execute rdtscp in order to get the current physical CPU ID.
	 *  - Input operands: None
	 *  - Output operands: EAX is stored to tsl, EDX is stored to tsh, ECX is stored to cpu_id.
	 *  - Clobbers: None */
	asm volatile("rdtscp" : "=a"(tsl), "=d"(tsh), "=c"(cpu_id)::);
	/** Return cpu_id. */
	return (uint16_t)cpu_id;
}

/**
 * @brief Read MSR.
 *
 * @param[in]    reg_num The MSR that needs to be read.
 *
 * @return The value read from MSR specified by \a reg_num.
 *
 * @pre MSR_IA32_EXT_APIC_ISR0 <= reg_num <= MSR_IA32_EXT_APIC_ISR7 or
 * reg_num shall be one from the following list:
 * - MSR_IA32_VMX_BASIC
 * - MSR_IA32_FEATURE_CONTROL
 * - MSR_IA32_VMX_PROCBASED_CTLS
 * - MSR_IA32_VMX_PROCBASED_CTLS2
 * - MSR_IA32_VMX_EPT_VPID_CAP
 * - MSR_IA32_MISC_ENABLE
 * - MSR_IA32_PAT
 * - MSR_IA32_EFER
 * - MSR_IA32_FS_BASE
 * - MSR_IA32_GS_BASE
 * - MSR_IA32_VMX_PINBASED_CTLS
 * - MSR_IA32_VMX_ENTRY_CTLS
 * - MSR_IA32_VMX_EXIT_CTLS
 * - MSR_IA32_TSC_ADJUST
 * - MSR_IA32_VMX_CR0_FIXED0
 * - MSR_IA32_VMX_CR0_FIXED1
 * - MSR_IA32_VMX_CR4_FIXED0
 * - MSR_IA32_VMX_CR4_FIXED1
 * - MSR_IA32_ARCH_CAPABILITIES
 * - MSR_IA32_APIC_BASE
 * - MSR_IA32_EXT_APIC_LDR
 * - MSR_IA32_EXT_XAPICID
 * - MSR_IA32_TSC_DEADLINE
 * - VMX_TSC_OFFSET_FULL
 * @post N/A
 *
 * @remark early_init_lapic interface should have been called once on the logical processor before msr_read reads from
 * below MSRs:
 * - MSR_IA32_EXT_APIC_LDR
 * - MSR_IA32_EXT_XAPICID
 * - MSR_IA32_APIC_BASE
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t msr_read(uint32_t reg_num)
{
	/** Declare the following local variables of type uint32_t.
	 *  - msrl representing the low 32 bit value read from the MSR, where the address of the MSR is \a reg_num,
	 *    not initialized.
	 *  - msrh representing the high 32 bit value read from the MSR, where the address of the MSR is \a reg_num,
	 *    not initialized. */
	uint32_t msrl, msrh;

	/** Execute rdmsr in order to read MSR.
	 *  - Input operands: ECX holds reg_num.
	 *  - Output operands: EAX stored to msrl, EDX stored to msrh.
	 *  - Clobbers: None */
	asm volatile(" rdmsr " : "=a"(msrl), "=d"(msrh) : "c"(reg_num));
	/** Return the value, where the value is calculated by bitwise OR msrl by the v, where the v is shifting msrh
	 *  right by 32. */
	return (((uint64_t)msrh << 32U) | msrl);
}

/**
 * @brief Write MSR.
 *
 * @param[in]    reg_num The MSR that the \a value64 will be written into.
 * @param[in]    value64 The value that needs to be written into the MSR.
 *
 * @return None
 *
 * @pre one of the following conditions shall be held:
 * - (reg_num == MSR_IA32_FEATURE_CONTROL) && ((value64 & FFFFFFFFFFE900F8H) == 0)
 * - (reg_num == MSR_IA32_TSC_AUX) && ((value64 & FFFFFFFF00000000H) == 0)
 * - (reg_num == MSR_IA32_MISC_ENABLE) && ((value64 & FFFFFFF8FF7AE776H) == 0)
 * - (reg_num == MSR_IA32_FS_BASE) && (((value64 >> 47) == 1FFFFH) || ((value64 >> 47) == 0))
 * - (reg_num == MSR_IA32_EFER) && ((value64 & FFFFFFFFFFFFF2FEH) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_EOI) && (value64 == 0)
 * - (reg_num == MSR_IA32_APIC_BASE) && ((value64 & FFFFFFF0000002FFH) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_CMCI) && ((value64 & FFFEE800H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_TIMER) && ((value64 & FFF8EF00H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_THERMAL) && ((value64 & FFFEE800H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_PMI) && ((value64 & FFFEE800H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_LINT0) && ((value64 & FFFE0800H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_LINT1) && ((value64 & FFFE0800H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_LVT_ERROR) && ((value64 & FFFFFFFFFFFEEF00H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_SIVR) && ((value64 & FFFFFFFFFFFFEE00H) == 0)
 * - (reg_num == MSR_IA32_EXT_APIC_ICR) && ((value64 & FFF32000H) == 0)
 * @post N/A
 *
 * @remark early_init_lapic has been called once on the logical processor before msr_write writes to below MSRs:
 * - MSR_IA32_EXT_APIC_ICR
 * - MSR_IA32_EXT_APIC_EOI
 * - MSR_IA32_EXT_APIC_LVT_CMCI
 * - MSR_IA32_EXT_APIC_LVT_THERMAL
 * - MSR_IA32_EXT_APIC_LVT_PMI
 * - MSR_IA32_EXT_APIC_LVT_LINT0
 * - MSR_IA32_EXT_APIC_LVT_LINT1
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void msr_write(uint32_t reg_num, uint64_t value64)
{
	/** Execute wrmsr in order to write MSR.
	 *  - Input operands: ECX holds reg_num, EAX holds low 32-bit of value64, EDX holds the value which is
	 *    calculated by shifting value64 right by 32.
	 *  - Output operands: None
	 *  - Clobbers: None */
	asm volatile(" wrmsr " : : "c"(reg_num), "a"((uint32_t)value64), "d"((uint32_t)(value64 >> 32U)));
}

/**
 * @brief Write value into the XCR specified by \a reg.
 *
 * Write the \a val into the XCR specified by \a reg
 *
 * @param[in]    reg The index of the extended register to be used.
 * @param[in]    val The value that needs to be written into the XCR.
 *
 * @pre reg == 0H
 * @pre val & FFFFFFFFFFFFDC01H == 1H
 *
 * @remark This interface can only be called after init_pcpu_post has been called once on the current logical processor.
 *
 * @return None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void write_xcr(int32_t reg, uint64_t val)
{
	/** Execute xsetbv in order to write value into the XCR specified by \a reg.
	 *  - Input operands: RCX holds zero-extended reg, RAX holds zero-extended low 32bits of val, RDX holds
	 *    zero-extended high 32bits of val.
	 *  - Output operands: None
	 *  - Clobbers: None */
	asm volatile("xsetbv" : : "c"(reg), "a"((uint32_t)val), "d"((uint32_t)(val >> 32U)));
}

/**
 * @brief Read value from the XCR specified by \a reg.
 *
 * @param[in]    reg The index of the extended register to be read from.
 *
 * @return The value read from XCR specified by \a reg.
 *
 * @pre reg == 0
 * @post N/A
 *
 * @remark This interface can only be called after init_pcpu_post has been called once on the current logical processor.
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t read_xcr(int32_t reg)
{
	/** Declare the following local variables of type uint32_t.
	 *  - xcrl representing low 32 bit value of XCR, not initialized.
	 *  - xcrh representing high 32 bit value of XCR, not initialized. */
	uint32_t xcrl, xcrh;

	/** Execute xgetbv in order to read value from the XCR specified by \a reg.
	 *  - Input operands: RCX holds zero-extended reg.
	 *  - Output operands: EAX stored to xcrl, EDX stored to xcrh.
	 *  - Clobbers: None */
	asm volatile("xgetbv " : "=a"(xcrl), "=d"(xcrh) : "c"(reg));
	/** Return the value, where the value is calculated by bitwise OR xcrl by the v, where the v is shifting xcrh
	 *  right by 32. */
	return (((uint64_t)xcrh << 32U) | xcrl);
}

/**
 * @brief Execute stac instruction.
 *
 * Set the AC flag bit in the EFLAGS register and make the supervisor-mode data have
 * rights to access the user-mode pages.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void stac(void)
{
	/** Execute stac in order to set the AC flag bit in the EFLAGS register and make the supervisor-mode data have
	 *  rights to access the user-mode pages.
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("stac" : : : "memory");
}

/**
 * @brief Execute clac instruction.
 *
 * Clear the AC bit in the EFLAGS register and make the supervisor-mode data do not have rights to access the user-mode
 * pages.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 * @remark This API can be called only after enable_smap has been called once on the current logical processor.
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void clac(void)
{
	/** Execute clac in order to clear the AC bit in the EFLAGS register and make the supervisor-mode data do not
	 *  have rights to access the user mode pages.
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("clac" : : : "memory");
}

uint16_t get_pcpu_nums(void);
extern void write_trampoline_stack_sym(uint16_t pcpu_id);
extern uint64_t prepare_trampoline(void);
#else /* ASSEMBLER defined */

#endif /* ASSEMBLER defined */

/**
 * @}
 */

#endif /* CPU_H */
