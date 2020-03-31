/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOAPIC_H
#define IOAPIC_H

/**
 * @addtogroup hwmgmt_apic
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the external data structures and API to mask all IOAPIC interrupts in hwmgmt.apic module.
 */

#include <apicreg.h>

#define NR_LEGACY_IRQ 16U

/**
 * @brief Definition of IOAPIC information
 *
 * @consistency N/A
 *
 * @alignment 1
 *
 * @remark N/A
 */
struct ioapic_info {
	uint8_t id; /**< IOAPIC ID as indicated in ACPI MADT */
	uint32_t addr; /**< IOAPIC Register address */
	uint32_t gsi_base; /**< Global System Interrupt where this IO-APIC's interrupt input start */
	uint32_t nr_pins; /**< Number of interrupt inputs as determined by Max. Redirection Entry Register */
};


void ioapic_setup_irqs(void);

bool ioapic_irq_is_gsi(uint32_t irq);

/**
 * @}
 */

#endif /* IOAPIC_H */
