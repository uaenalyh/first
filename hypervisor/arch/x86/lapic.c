/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <bits.h>
#include <msr.h>
#include <cpu.h>
#include <per_cpu.h>
#include <cpu_caps.h>
#include <lapic.h>

/**
 * @defgroup hwmgmt_apic hwmgmt.apic
 * @ingroup hwmgmt
 * @brief Implementation of software interfaces for physical local APIC and IOAPIC.
 *
 * This module provides functions to initialize physical local APIC, and implements APIs
 * to send startup and INIT IPI messages using local APIC.
 *
 * Usage Remarks: hwmgmt.cpu module uses functions in this module to early initialize local APIC
 *		  and to send IPI messages.
 *		  vp-base.virq module uses get_cur_lapic_id() to fetch local APIC ID.
 *
 *
 * Dependency Justification: This module uses APIs in hwmgmt.cpu module to access MSR registers.
 *			     This module uses APIs in hwmgmt.io module to access MMIO space.
 *			     This module uses API in hwmgmt.mmu module to update configuration for page table.
 *			     This module uses APIs in lib.lock module to access spinlock.
 *			     This module uses APIs in debug module to dump debug message.
 *
 *
 * External APIs:
 *  - early_init_lapic()  This function enables x2APIC mode.
 *			  Depends on:
 *			    - msr_write()
 *			    - msr_read()
 *
 *  - init_lapic()	  This function initializes the local APIC on the current processor.
 *			  Depends on:
 *			    - msr_write()
 *			    - msr_read()
 *			    - clear_lapic_isr()
 *
 *  - get_cur_lapic_id()  This function gets local APIC ID for current processor.
 *			  Depends on:
 *			    - msr_read()
 *
 *  - send_startup_ipi()  This function sends a INIT-SIPI sequence to target logical processor.
 *			  Depends on:
 *			    - msr_write()
 *
 *  - send_single_init()  This function sends INIT IPI message to target logical processor.
 *			  Depends on:
 *			    - msr_write()
 *
 *  - ioapic_setup_irqs()  This function masks all IOAPIC pins.
 *			  Depends on:
 *			    - spinlock_init()
 *			    - map_ioapic()
 *			    - parse_madt_ioapic()
 *			    - ioapic_nr_pins()
 *			    - ioapic_set_routing()
 *			    - hv_access_memory_region_update()
 * @{
 */


/**
 * @file
 * @brief This file implements software interfaces for local APIC provided by the hwmgmt.apic module.
 *
 * External APIs:
 *  - early_init_lapic()  This function enables x2APIC mode.
 *			  Depends on:
 *			    - msr_write()
 *			    - msr_read()
 *
 *  - init_lapic()	  This function initializes the local APIC on the current processor.
 *			  Depends on:
 *			    - msr_write()
 *			    - msr_read()
 *			    - clear_lapic_isr()
 *
 *  - get_cur_lapic_id()  This function gets local APIC ID for current processor.
 *			  Depends on:
 *			    - msr_read()
 *
 *  - send_startup_ipi()  This function sends a INIT-SIPI sequence to target logical processor.
 *			  Depends on:
 *			    - msr_write()
 *
 *  - send_single_init()  This function sends INIT IPI message to target logical processor.
 *			  Depends on:
 *			    - msr_write()
 *
 *
 * Internal functions:
 *  - clear_lapic_isr()	   This function clears local APIC ISR registers for current processor.
 *			   Depends on:
 *			    - msr_write()
 */

/**
 * @brief Definition for the layout of IA32_APIC_BASE MSR(1BH) on processor with x2APIC support.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 */
union lapic_base_msr {
	uint64_t value;  /**< Bitmap value for IA32_APIC_BASE MSR */

	/**
	* @brief Definition for the bitmap layout of IA32_APIC_BASE MSR.
	*
	* @consistency N/A
	*
	* @alignment 8
	*
	*/
	struct {
		uint32_t rsvd_1 : 8;		/**< Reserved */
		uint32_t bsp : 1;		/**< Indicates whether the processor is BP or not */
		uint32_t rsvd_2 : 1;		/**< Reserved */
		uint32_t x2APIC_enable : 1;	/**< Enable x2APIC mode */
		uint32_t xAPIC_enable : 1;	/**< xAPIC global enable/disable */
		uint32_t lapic_paddr : 24;	/**< APIC base physical address */
		uint32_t rsvd_3 : 28;		/**< Reserved */
	} fields;
};

/**
 * @brief This function clears local APIC ISR registers on current processor.
 *
 * This is an Intel recommended procedure and assures that the processor
 * does not get hung up due to already set "in-service" interrupts left
 * over from the boot loader environment. This actually occurs in real
 * life, therefore we will ensure all the in-service bits are clear.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static void clear_lapic_isr(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing loop counter, not initialized. */
	uint32_t i;
	/** Declare the following local variables of type uint32_t.
	 *  - isr_reg representing address of ISR register, not initialized. */
	uint32_t isr_reg;

	/** For each isr_reg ranging from MSR_IA32_EXT_APIC_ISR7 to MSR_IA32_EXT_APIC_ISR0 with a step of -1 */
	for (isr_reg = MSR_IA32_EXT_APIC_ISR7; isr_reg >= MSR_IA32_EXT_APIC_ISR0; isr_reg--) {
		/** If return value of msr_read(isr_reg) does not equal 0H */
		if (msr_read(isr_reg) != 0U) {
			/** For each i ranging from 0 to 31H with a step of 1H */
			for (i = 0U; i < 32U; i++) {
				/** Call msr_write() with the following parameters,
				 *  in order to write in order to write MSR_IA32_EXT_APIC_EOI with 0H
				 *	to clear the highest priority bit in the ISR.
				 *  - MSR_IA32_EXT_APIC_EOI
				 *  - 0H
				 */
				msr_write(MSR_IA32_EXT_APIC_EOI, 0U);
			}
		}
	}
}

/**
 * @brief This function enables x2APIC mode.
 *
 * This function enables x2APIC mode of LAPIC on the current processor
 * this interface is called upon. The firmware should not put LAPIC into
 * the invalid state.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void early_init_lapic(void)
{
	/** Declare the following local variables of type union lapic_base_msr.
	 *  - base representing the value of APICBASE register, not initialized. */
	union lapic_base_msr base;

	/** Set base.value to msr_read(MSR_IA32_APIC_BASE) */
	base.value = msr_read(MSR_IA32_APIC_BASE);

	/** Set base.fields.xAPIC_enable to 1H. */
	base.fields.xAPIC_enable = 1U;

	/** Call msr_write() with the following parameters, in order to enable local APIC in xAPIC mode.
	 *  - MSR_IA32_APIC_BASE
	 *  - base.value
	 */
	msr_write(MSR_IA32_APIC_BASE, base.value);

	/** Set base.fields.x2APIC_enable to 1H. */
	base.fields.x2APIC_enable = 1U;
	/** Call msr_write() with the following parameters, in order to enable local APIC in x2APIC mode.
	 *  - MSR_IA32_APIC_BASE
	 *  - base.value
	 */
	msr_write(MSR_IA32_APIC_BASE, base.value);
}


/**
 * @brief This function initializes the local APIC on the current processor.
 *
 * This function initializes below the physical local APIC registers for current processor:
 * - MSR_IA32_EXT_APIC_LVT_CMCI
 * - MSR_IA32_EXT_APIC_LVT_TIMER
 * - MSR_IA32_EXT_APIC_LVT_THERMAL
 * - MSR_IA32_EXT_APIC_LVT_PMI
 * - MSR_IA32_EXT_APIC_LVT_LINT0
 * - MSR_IA32_EXT_APIC_LVT_LINT1
 * - MSR_IA32_EXT_APIC_LVT_ERROR
 * - MSR_IA32_EXT_APIC_DIV_CONF
 * - MSR_IA32_TSC_DEADLINE
 * - MSR_IA32_EXT_APIC_ICR
 * - MSR_IA32_EXT_APIC_SIVR
 * And this function also clears local APIC ISR registers by calling clear_lapic_isr() function.
 *
 * Temporal Constraints: This interface can be called only after early_init_lapic is called once on the current
 * processor.
 *
 * @param[in]  pcpu_id		ID of current logical processor.
 *
 * @return None
 *
 * @pre pcpu_id == get_pcpu_id()
 * @pre msr_read(MSR_IA32_APIC_BASE) & 0xc00 == 0xc00
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void init_lapic(uint16_t pcpu_id)
{
	/** Set per_cpu(lapic_ldr, pcpu_id) to (uint32_t)msr_read(MSR_IA32_EXT_APIC_LDR) */
	per_cpu(lapic_ldr, pcpu_id) = (uint32_t)msr_read(MSR_IA32_EXT_APIC_LDR);

	/** Call msr_write() with the following parameters, in order to mask LVT CMCI.
	 *  - MSR_IA32_EXT_APIC_LVT_CMCI
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_CMCI, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask timer interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_TIMER
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_TIMER, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask thermal interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_THERMAL
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_THERMAL, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask PMI interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_PMI
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_PMI, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask LINT0 interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_LINT0
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_LINT0, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask LINT1 interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_LINT1
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_LINT1, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to mask local APIC error interrupt.
	 *  - MSR_IA32_EXT_APIC_LVT_ERROR
	 *  - LAPIC_LVT_MASK
	 */
	msr_write(MSR_IA32_EXT_APIC_LVT_ERROR, LAPIC_LVT_MASK);

	/** Call msr_write() with the following parameters, in order to set spurious vector.
	 *  - MSR_IA32_EXT_APIC_SIVR
	 *  - LAPIC_SVR_VECTOR
	 */
	msr_write(MSR_IA32_EXT_APIC_SIVR, LAPIC_SVR_VECTOR);

	/** Call msr_write() with the following parameters, in order to set
	 *  APIC timer Divide Configuration Register.
	 *  - MSR_IA32_EXT_APIC_DIV_CONF
	 *  - 0H
	 */
	msr_write(MSR_IA32_EXT_APIC_DIV_CONF, 0x00000000U);

	/** Call msr_write() with the following parameters, in order to disarm local APIC timer.
	 *  - MSR_IA32_TSC_DEADLINE
	 *  - 0H
	 */
	msr_write(MSR_IA32_TSC_DEADLINE, 0x00000000U);

	/** Call msr_write() with the following parameters, in order to reset interrupt control registers.
	 *  - MSR_IA32_EXT_APIC_ICR
	 *  - 0H
	 */
	msr_write(MSR_IA32_EXT_APIC_ICR, 0x0000000000000000U);

	/** Call msr_write() with the following parameters, in order to reset Task Priority Register.
	 *  - MSR_IA32_EXT_APIC_TPR
	 *  - 0H
	 */
	msr_write(MSR_IA32_EXT_APIC_TPR, 0x00000000U);

	/** Call msr_write() with the following parameters, in order to reset Initial Count Register(for Timer).
	 *  - MSR_IA32_EXT_APIC_INIT_COUNT
	 *  - 0H
	 */
	msr_write(MSR_IA32_EXT_APIC_INIT_COUNT, 0x00000000U);

	/** Call clear_lapic_isr() with the following parameters,
	 *  in order to ensure there are no ISR bits set.
	 *  - N/A
	 */
	clear_lapic_isr();
}

/**
 * @brief This function gets local APIC ID for current processor.
 *
 * This function reads LAPIC ID register on the current processor,
 * via the RDMSR instruction reading the MSR at address 0802H.
 *
 * Temporal Constraints: This API can be called only after early_init_lapic has been called once on the current
 * processor.
 *
 * @return local APIC ID for current processor
 *
 * @pre msr_read(MSR_IA32_APIC_BASE) & 0xc00 == 0xc00
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
uint32_t get_cur_lapic_id(void)
{
	/** Declare the following local variables of type uint32_t.
	 *  - lapic_id representing the local APIC ID, not initialized. */
	uint32_t lapic_id;

	/** Set lapic_id to (uint32_t)msr_read(MSR_IA32_EXT_XAPICID) */
	lapic_id = (uint32_t)msr_read(MSR_IA32_EXT_XAPICID);

	/** Return lapic_id */
	return lapic_id;
}

/**
 * @brief This function sends a INIT-SIPI sequence to target logical processor, to notify target logical processor to
 *        start booting.
 *
 * Temporal Constraints: This API can be called only after early_init_lapic has been called once on the current
 * processor.
 *
 * @param[in]  cpu_startup_shorthand	Not used. No shorthand is used for current scope,
 *					keep it for future scope extension.
 * @param[in]  dest_pcpu_id		ID of destination logical processor.
 * @param[in]  cpu_startup_start_address	startup start address for target logical processor.
 *
 * @return None
 *
 * @pre cpu_startup_shorthand == INTR_CPU_STARTUP_USE_DEST
 * @pre cpu_startup_start_address < 0x100000 && (cpu_startup_start_address & 0FFFH == 0)
 * @pre dest_pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
void send_startup_ipi(__unused enum intr_cpu_startup_shorthand cpu_startup_shorthand, uint16_t dest_pcpu_id,
	uint64_t cpu_startup_start_address)
{
	/** Declare the following local variables of type union apic_icr.
	 *  - icr representing value of ICR register, not initialized. */
	union apic_icr icr;
	/** Declare the following local variables of type uint8_t.
	 *  - shorthand representing shorthand setting of ICR register, not initialized. */
	uint8_t shorthand;

	/** Set icr.value to 0H */
	icr.value = 0U;
	/** Set icr.bits.destination_mode to INTR_LAPIC_ICR_PHYSICAL */
	icr.bits.destination_mode = INTR_LAPIC_ICR_PHYSICAL;
	/** Set shorthand to INTR_LAPIC_ICR_USE_DEST_ARRAY */
	shorthand = INTR_LAPIC_ICR_USE_DEST_ARRAY;
	/** Set icr.value_32.hi_32 to per_cpu(lapic_id, dest_pcpu_id) */
	icr.value_32.hi_32 = per_cpu(lapic_id, dest_pcpu_id);

	/** Set icr.bits.shorthand to shorthand */
	icr.bits.shorthand = shorthand;
	/** Set icr.bits.delivery_mode to INTR_LAPIC_ICR_INIT */
	icr.bits.delivery_mode = INTR_LAPIC_ICR_INIT;

	/** Call msr_write() with the following parameters, in order to send INIT IPI message.
	 *  - MSR_IA32_EXT_APIC_ICR
	 *  - icr.value
	 */
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);

	/** Set icr.value_32.lo_32 to 0H */
	icr.value_32.lo_32 = 0U;
	/** Set icr.bits.shorthand to shorthand */
	icr.bits.shorthand = shorthand;
	/** Set icr.bits.delivery_mode to INTR_LAPIC_ICR_STARTUP */
	icr.bits.delivery_mode = INTR_LAPIC_ICR_STARTUP;
	/** Set icr.bits.vector to (uint8_t)(cpu_startup_start_address >> 12H) */
	icr.bits.vector = (uint8_t)(cpu_startup_start_address >> 12U);

	/** Call msr_write() with the following parameters, in order to send STARTUP IPI message.
	 *  - MSR_IA32_EXT_APIC_ICR
	 *  - icr.value
	 */
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);
}

/**
 * @brief This function sends INIT IPI message to target logical processor.
 *
 * According to Intel SDM Vol3 23.8, the INIT signal is blocked whenever a logical
 * processor is in VMX root operation. But INIT signal is not blocked in VMX non-root
 * operation and causes VM exit.
 *
 * Temporal Constraints: This interface can be called only after early_init_lapic is called once on the current
 * processor.
 *
 * @param[in]  pcpu_id		ID of target logical processor.
 *
 * @return None
 *
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void send_single_init(uint16_t pcpu_id)
{
	/** Declare the following local variables of type union apic_icr.
	 *  - icr representing value of ICR register, not initialized. */
	union apic_icr icr;


	/** Set icr.value_32.hi_32 to per_cpu(lapic_id, pcpu_id) */
	icr.value_32.hi_32 = per_cpu(lapic_id, pcpu_id);
	/** Set icr.value_32.lo_32 to (INTR_LAPIC_ICR_PHYSICAL << 11H) | (INTR_LAPIC_ICR_INIT << 8H) */
	icr.value_32.lo_32 = (INTR_LAPIC_ICR_PHYSICAL << 11U) | (INTR_LAPIC_ICR_INIT << 8U);

	/** Call msr_write() with the following parameters, in order to send INIT IPI message.
	 *  - MSR_IA32_EXT_APIC_ICR
	 *  - icr.value
	 */
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);
}

/**
 * @}
 */
