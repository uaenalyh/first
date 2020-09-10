/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <spinlock.h>
#include <per_cpu.h>
#include <io.h>
#include <irq.h>
#include <idt.h>
#include <ioapic.h>
#include <lapic.h>
#include <dump.h>
#include <logmsg.h>
#include <vcpu.h>


/**
 * @defgroup hwmgmt_irq hwmgmt.irq
 * @ingroup hwmgmt
 * @brief  The hwmgmt.irq module implements functions to initialize interrupt handling facilities of
 *         the physical processor and implementing interrupt handlers.
 *
 * The hwmgmt.irq module mainly contains two files: irq.c which implements initialization of host IDT,
 * idt.S which implements handler of all these interrupts and exceptions.
 * TBD: Currently idt.S is not covered in this module design.
 *
 * Dependency:
 * - This module depends on 'hwmgmt.apic' module to initialize the LAPIC.
 *
 * Usage:
 * - 'hwmgmt.cpu' module depends on this module to initialize the host IDT and relevant handlers.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all functions and global variable that shall be provided
 *        by the hwmgmt.irq module.
 *
 * It also defines some helper functions to implement internal initialization for IDT and IRQ.
 *
 * Helper functions include: fixup_idt, set_idt, disable_pic_irqs, and init_default_irqs.
 *
 * Decomposed functions include: dispatch_interrupt, dispatch_exception and handle_nmi.
 */

/**
 * @brief This structure exception_spinlock is used to protect the operations on the
 * context of the exception.
 */
static spinlock_t exception_spinlock = {
	.head = 0U,   /**< head of the LinkedList */
	.tail = 0U,   /**< tail of the LinkedList */
};

/**
 * @brief This function is used to handle the interrupt in root mode.
 *
 * @param[in] ctx A pointer points to a structure intr_excp_ctx which containing
 * the information of the to-be-handled external interrupt.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a ctx is different among parallel invocation.
 */
void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	/** Call panic with the following parameters, in order to output fatal failure log and could halt
	 *  current physical CPU.
	 *  - "Unexpected external interrupt."
	 */
	panic("Unexpected external interrupt.");
}

/**
 * @brief This function is used to handle the exceptions in root mode.
 *
 * @param[in] ctx A pointer points to a structure intr_excp_ctx which containing
 * the information of the to-be-handled exception.
 *
 * @return None
 *
 * @pre ctx != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a ctx is different among parallel invocation.
 */
void dispatch_exception(struct intr_excp_ctx *ctx)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing id of the current running physical CPU,
	 *  initialized as get_pcpu_id(). */
	uint16_t pcpu_id = get_pcpu_id();

	/** Call spinlock_obtain() with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on exception.
	 *  - &exception_spinlock */
	spinlock_obtain(&exception_spinlock);

	/** Call dump_exception() with the following parameters, in order to print the exception content of
	 *  the given physical cpu.
	 *  - ctx
	 *  - pcpu_id */
	dump_exception(ctx, pcpu_id);

	/** Call spinlock_release() with the following parameters, in order to release the spin lock that is used to
	 *  protect the operations on exception.
	 *  - &exception_spinlock */
	spinlock_release(&exception_spinlock);

	/** Call panic() with the following parameters, in order to output fatal failure log and could halt
	 *  current physical CPU.
	 *  - "Unexpected exception."
	 */
	panic("Unexpected exception.");
}

/**
 * @brief This function is used to handle the NMI.
 *
 * @param[in] ctx A pointer points to a structure intr_excp_ctx which containing
 * the information of the to-be-handled NMI.
 *
 * @return None
 *
 * @pre ctx != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when \a ctx is different among parallel invocation.
 */
void handle_nmi(struct intr_excp_ctx *ctx)
{
	/** Call vcpu_queue_exception() with the following parameters, in order to
	 *  inject the exception to the guest vcpu.
	 *  - get_cpu_var(ever_run_vcpu)
	 *  - ctx->vector
	 *  - ctx->error_code
	 */
	vcpu_queue_exception(get_cpu_var(ever_run_vcpu), ctx->vector, ctx->error_code);
}

/**
 * @brief This function is used to disable the PIC interrupts.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static void disable_pic_irqs(void)
{
	/** Call pio_write8() with the following parameters, in order to write FFH
	 *  to the I/O port A1H to disable PIC interrupts.
	 *  - 0xffU
	 *  - 0xA1U
	 */
	pio_write8(0xffU, 0xA1U);
	/** Call pio_write8() with the following parameters, in order write FFH
	 *  to the I/O port 21H to disable PIC interrupts.
	 *  - 0xffU
	 *  - 0x21U
	 */
	pio_write8(0xffU, 0x21U);
}

/**
 * @brief This function is used to disable the PIC interrupt and mask all IOAPIC pins.
 *
 * @param[in] cpu_id The ID of the physical CPU whose interrupt resources are to be initialized.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void init_default_irqs(uint16_t cpu_id)
{
	/** If cpu_id equals to BOOT_CPU_ID */
	if (cpu_id == BOOT_CPU_ID) {
		/** Call disable_pic_irqs() to disable PIC interrupts.
		 */
		disable_pic_irqs();
		/** Call ioapic_setup_irqs() to write to every pin's RTE in the IOAPIC to mask pin.
		 */
		ioapic_setup_irqs();
	}
}

/**
 * @brief This function is used to modify the given table to let it be a valid IDT.
 *
 * @param[in] idtd A pointer points to a structure host_idt_descriptor to be fixed
 *
 * @return None
 *
 * @pre idtd != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark None
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline void fixup_idt(const struct host_idt_descriptor *idtd)
{
	/** Declare the following local variables of type 'uint32_t'.
	 *  - i representing the loop counter as array index, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type 'union idt_64_descriptor *'.
	 *  - idt_desc representing a pointer which points to a union idt_64_descriptor which
	 *  recording the 64-bit MODE IDT information, initialized as idtd->idt.
	 */
	union idt_64_descriptor *idt_desc = (union idt_64_descriptor *)idtd->idt;
	/** Declare the following local variables of type 'uint32_t'.
	 *  - entry_hi_32 representing the bits 32-63 of the 64 bit IDT descriptor, not initialized.
	 *  - entry_lo_32 representing the low 32 bits of the 64 bit IDT descriptor, not initialized.
	 */
	uint32_t entry_hi_32, entry_lo_32;

	/** For each i ranging from 0H to (HOST_IDT_ENTRIES-1) [with a step of 1] */
	for (i = 0U; i < HOST_IDT_ENTRIES; i++) {
		/** Set 'entry_lo_32' to idt_desc[i].fields.offset_63_32 */
		entry_lo_32 = idt_desc[i].fields.offset_63_32;
		/** Set 'entry_hi_32' to idt_desc[i].fields.rsvd */
		entry_hi_32 = idt_desc[i].fields.rsvd;
		/** Set 'idt_desc[i].fields.rsvd' to 0 */
		idt_desc[i].fields.rsvd = 0U;
		/** Set 'idt_desc[i].fields.offset_63_32' to entry_hi_32 */
		idt_desc[i].fields.offset_63_32 = entry_hi_32;
		/** Set 'idt_desc[i].fields.high32.bits.offset_31_16' to entry_lo_32 >> 16H */
		idt_desc[i].fields.high32.bits.offset_31_16 = entry_lo_32 >> 16U;
		/** Set 'idt_desc[i].fields.low32.bits.offset_15_0' to entry_lo_32 & ffffH */
		idt_desc[i].fields.low32.bits.offset_15_0 = entry_lo_32 & 0xffffUL;
	}
}

/**
 * @brief This function is used to load the specified IDT into the host IDTR.
 *
 * @param[in] idtd a pointer points to a structure which stores the IDT to be loaded to IDTR.
 *
 * @return None
 *
 * @pre idtd != NULL
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark None
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void set_idt(struct host_idt_descriptor *idtd)
{
	/** Execute lidtq instruction in order to load IDT pointed by \a idtd into the host IDTR.
	 * - input operands: *idtd which is the source operand of lidtq instruction
	 * - output operands: none
	 * - clobbers: none
	 */
	asm volatile("   lidtq %[idtd]\n"
		     : /* no output parameters */
		     : /* input parameters */
		     [ idtd ] "m"(*idtd));
}

/**
 * @brief This function is used to initialize interrupt related resource before usage.
 *
 * This function initializes the host IDTR, the LAPIC and these IRQs of the target pCPU.
 *
 * @param[in] pcpu_id The ID of the physical CPU whose interrupt resources are to be initialized.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety when pcpu_id is different among parallel invocation
 */
void init_interrupt(uint16_t pcpu_id)
{
	/** Declare the following local variables of type 'struct host_idt_descriptor *'.
	 *  - idtd representing a pointer which points to the host IDT to be filled,
	 *  initialized as address of HOST_IDTR */
	struct host_idt_descriptor *idtd = &HOST_IDTR;

	/** If pcpu_id equals to BOOT_CPU_ID */
	if (pcpu_id == BOOT_CPU_ID) {
		/** Call fixup_idt() with the following parameters, in order to modify
		 *  the table pointed by idtd to be a valid IDT.
		 *  - idtd
		 */
		fixup_idt(idtd);
	}
	/** Call set_idt() with the following parameters, in order to load the IDT information pointed
	 *  by 'idtd' into the host IDTR.
	 *  - idtd
	 */
	set_idt(idtd);
	/** Call init_lapic() with the following parameters, in order to initialize the LAPIC
	 *  of the physical processor whose ID is \a pcpu_id.
	 *  - pcpu_id
	 */
	init_lapic(pcpu_id);
	/** Call init_default_irqs() with the following parameters, in order to initialize the IRQs
	 *  of the physical processor whose ID is \a pcpu_id.
	 *  - pcpu_id
	 */
	init_default_irqs(pcpu_id);
}

/**
 * @}
 */
