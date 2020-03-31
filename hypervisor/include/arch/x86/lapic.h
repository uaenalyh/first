/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INTR_LAPIC_H
#define INTR_LAPIC_H

/**
 * @addtogroup hwmgmt_apic
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the external data structures and APIs for local APIC in hwmgmt.apic module.
 */

#include <types.h>
#include <apicreg.h>

#define INTR_LAPIC_ICR_INIT    0x5U	/**< Delivery Mode for INIT */
#define INTR_LAPIC_ICR_STARTUP 0x6U	/**< Delivery Mode for SIPI */

/* intr_lapic_icr_dest_mode */
#define INTR_LAPIC_ICR_PHYSICAL 0x0U	/**< Physical destination mode */

/* intr_lapic_icr_level */
#define INTR_LAPIC_ICR_DEASSERT 0x0U	/**< Level for De-assert */
#define INTR_LAPIC_ICR_ASSERT   0x1U	/**< Level for Assert */

/* intr_lapic_icr_shorthand */
#define INTR_LAPIC_ICR_USE_DEST_ARRAY 0x0U /**< No shorthand */

/* LAPIC register bit and bitmask definitions */
#define LAPIC_SVR_VECTOR           0x000000FFU	/**< Spurious interrupt vector */

#define LAPIC_LVT_MASK 0x00010000U	/**< Local APIC LVT mask */

/**
 * @brief Enumeration type definition for SIPI IPI message.
 *
 */
enum intr_cpu_startup_shorthand {
	INTR_CPU_STARTUP_USE_DEST,	/**< No shorthand used */
	INTR_CPU_STARTUP_ALL_EX_SELF,	/**< Shorthand for all excluding self  */
	INTR_CPU_STARTUP_UNKNOWN,	/**< Invalid shorthand type */
};

/**
 * @brief Union definition presenting layout of x2APIC Interrupt Command Register (ICR).
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 */
union apic_icr {
	uint64_t value;		/**<  Value of ICR value */
	/**
	 * @brief Structure definition presenting layout of x2APIC ICR,
	 *		  including low 32-bits and high 32-bits value.
	 *
	 * @consistency N/A
	 *
	 * @alignment 8
	 *
	 */
	struct {
		uint32_t lo_32;	/**< Low 32-bits of ICR value */
		uint32_t hi_32;	/**<  High 32-bits of ICR value*/
	} value_32;

	/**
	 * @brief Structure definition presenting bitmap layout of x2APIC ICR register.
	 *
	 * @consistency N/A
	 *
	 * @alignment 8
	 *
	 */
	struct {
		uint32_t vector : 8;		/**< The vector number of the interrupt being sent */
		uint32_t delivery_mode : 3;	/**< IPI message type */
		uint32_t destination_mode : 1;	/**< physical (0) or logical (1) destination mode */
		uint32_t rsvd_1 : 2;		/**< Reserved */
		uint32_t level : 1;		/**< De-assert(0) or Assert(1) */
		uint32_t trigger_mode : 1;	/**< Trigger Mode, edge (0) or level(1) */
		uint32_t rsvd_2 : 2;		/**< Reserved */
		uint32_t shorthand : 2;		/**< Shorthand notation for the destination of the interrupt */
		uint32_t rsvd_3 : 12;		/**< Reserved */
		uint32_t dest_field : 32;	/**< Target processor or processors */
	} bits;
};


void early_init_lapic(void);

uint32_t get_cur_lapic_id(void);

void init_lapic(uint16_t pcpu_id);

void send_startup_ipi(enum intr_cpu_startup_shorthand cpu_startup_shorthand, uint16_t dest_pcpu_id,
	uint64_t cpu_startup_start_address);

void send_single_init(uint16_t pcpu_id);

/**
 * @}
 */

#endif /* INTR_LAPIC_H */
