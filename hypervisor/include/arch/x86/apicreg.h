/*-
 * Copyright (c) 1996, by Peter Wemm and Steve Passe
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef APICREG_H
#define APICREG_H

/**
 * @addtogroup hwmgmt_apic
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the external data structures and MACROs for local APIC and IOAPIC in hwmgmt.apic module.
 */


#include <page.h>


/**
 * @brief Structure definition presenting one local APIC register.
 *
 * @consistency N/A
 *
 * @alignment 4
 *
 */
struct lapic_reg {
	uint32_t v;	/**<  Value of a local APIC register */
};

/**
 * @brief Structure definition presenting one local APIC register.
 *
 * @consistency N/A
 *
 * @alignment 4096
 *
 */
struct lapic_regs {
	struct lapic_reg rsv0[2]; /**< Reserved */
	struct lapic_reg id;	  /**< Local APIC ID Register */
	struct lapic_reg version; /**< Local APIC Version Register */
	struct lapic_reg rsv1[4]; /**< Reserved */
	struct lapic_reg tpr; /**< Task Priority Register (TPR) */
	struct lapic_reg apr; /**< Arbitration Priority Register (APR) */
	struct lapic_reg ppr; /**< Processor Priority Register (PPR) */
	struct lapic_reg eoi; /**< EOI Register */
	struct lapic_reg rrd; /**< Remote Read Register (RRD) */
	struct lapic_reg ldr; /**< Logical Destination Register */
	struct lapic_reg dfr; /**< Destination Format Register */
	struct lapic_reg svr; /**< Spurious Interrupt Vector Register */
	struct lapic_reg isr[8]; /**< In-Service Registers (ISR) */
	struct lapic_reg tmr[8]; /**< Trigger Mode Registers (TMR)*/
	struct lapic_reg irr[8]; /**< Interrupt Request Registers (IRR) */
	struct lapic_reg esr;	 /**< Error Status Register */
	struct lapic_reg rsv2[6]; /**< Reserved */
	struct lapic_reg lvt_cmci; /**< LVT Corrected Machine Check Interrupt (CMCI) Register */
	struct lapic_reg icr_lo;   /**< Interrupt Command Register (ICR); bits 0-31 */
	struct lapic_reg icr_hi;   /**< Interrupt Command Register (ICR); bits 32-63 */
	struct lapic_reg lvt[6];   /**< LVT Registers */
	struct lapic_reg icr_timer; /**< Initial Count Register (for Timer) */
	struct lapic_reg ccr_timer; /**< Current Count Register (for Timer) */
	struct lapic_reg rsv3[4];   /**< Reserved */
	struct lapic_reg dcr_timer; /**< Divide Configuration Register (for Timer) */
	struct lapic_reg self_ipi;  /**< SELF IPI Register, Available only in x2APIC mode */

	struct lapic_reg rsv5[192]; /**< Reserved, roundup sizeof current struct to 4KB */
} __aligned(PAGE_SIZE);


#define IOAPIC_RTE_MASK_SET 0x1U /**< Set IOAPIC interrupt mask */

#define IOAPIC_RTE_TRGRMODE_EDGE  0x0U /**< IOAPIC edge trigger mode */
#define IOAPIC_RTE_TRGRMODE_LEVEL 0x1U /**< IOAPIC Level Trigger Mode */

#define IOAPIC_RTE_INTPOL_AHI 0x0U /**< IOAPIC interrupt active high polarity */

#define IOAPIC_RTE_DESTMODE_LOGICAL 0x1U /**< IOAPIC logical destination mode */

#define IOAPIC_RTE_DELMODE_LOPRI 0x1U /**< IOAPIC lowest priority delivery mode */

/**
 * @brief Union definition presenting layout IOAPIC Redirection Table (RTE) Entry .
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 */
union ioapic_rte {
	uint64_t full; /**< Value of a IOAPIC RTE entry */

	/**
	 * @brief Structure definition presenting layout IOAPIC RTE Entry,
	 *		  including low 32-bits and high 32-bits value.
	 *
	 * @consistency N/A
	 *
	 * @alignment 8
	 *
	 */
	struct {
		uint32_t lo_32; /**< Value of low 32 bits of IOAPIC RTE entry */
		uint32_t hi_32; /**< Value of high 32 bits IOAPIC RTE entry */
	} u;

	/**
	 * @brief Structure definition presenting bitmap layout IOAPIC RTE Entry.
	 *
	 * @consistency N/A
	 *
	 * @alignment 8
	 *
	 * @remark This structure shall be packed.
	 */
	struct {
		uint8_t vector : 8; /**< The interrupt vector for this interrupt */
		uint64_t delivery_mode : 3; /**< Delivery mode */
		uint64_t dest_mode : 1; /**< Determines the interpretation of the destination field */
		uint64_t delivery_status : 1; /**< Current status of the delivery of this interrupt */
		uint64_t intr_polarity : 1; /**< Interrupt input pin polarity, 0=High active, 1=Low active. */
		uint64_t remote_irr : 1; /**< Remote IRR */
		uint64_t trigger_mode : 1; /**< Trigger mode, 1=Level sensitive, 0=Edge sensitive */
		uint64_t intr_mask : 1; /**< Interrupt mask, When this bit is 1, the interrupt signal is masked */
		uint64_t rsvd_1 : 39; /**< Reserved */
		uint8_t dest_field : 8;/**< Destination field */
	} bits __packed;
};


#define DEFAULT_APIC_BASE 0xfee00000UL /**< Default physical address of local APICs */

#define APIC_ID_MASK  0xff000000U /**< Mask of APIC ID register */
#define APIC_ID_SHIFT 24U /**< Shift of bits of APIC ID register */

#define APIC_VECTOR_MASK 0x000000ffU /**< Mask of vector in local APIC ICR register */

#define APIC_DELMODE_MASK    0x00000700U /**< Mask of delivery mode in local APIC ICR register */
#define APIC_DELMODE_INIT    0x00000500U /**< Delivery mode of INIT in local APIC ICR register */
#define APIC_DELMODE_STARTUP 0x00000600U /**< Delivery mode of STARTUP in local APIC ICR register */

#define APIC_DESTMODE_LOG 0x00000800U /**< Destination mode of logical mode in local APIC ICR register */

#define APIC_LEVEL_MASK     0x00004000U /**< Mask of trigger level in local APIC ICR register */

#define APIC_TRIGMOD_MASK 0x00008000U /**< Mask of trigger mode in local APIC ICR register */
#define APIC_DEST_MASK    0x000c0000U /**< Mask of destination mode in local APIC ICR register */
#define APIC_DEST_DESTFLD 0x00000000U /**< Mask of destination field in local APIC ICR register */

#define IOAPIC_REGSEL 0x00U /**< IOAPIC I/O register select register */
#define IOAPIC_WINDOW 0x10U /**< IOAPIC I/O window register */

#define IOAPIC_VER 0x01U /**< Version of IOAPIC */

#define IOAPIC_MAX_RTE_MASK 0x00ff0000U /**< Mask of maximum redirection entry in IOAPIC version register */
#define MAX_RTE_SHIFT       16U /**< Shift of bits of maximum redirection entry in IOAPIC version register */

/**
 * @}
 */

#endif /* APICREG_H */
