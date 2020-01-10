/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <ioapic.h>
#include <irq.h>
#include <pgtable.h>
#include <io.h>
#include <mmu.h>
#include <acpi.h>
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_apic
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define NR_MAX_GSI (CONFIG_MAX_IOAPIC_NUM * CONFIG_MAX_IOAPIC_LINES)

static uint32_t ioapic_nr_gsi;
static spinlock_t ioapic_lock;

static void *map_ioapic(uint64_t ioapic_paddr)
{
	/* At some point we may need to translate this paddr to a vaddr.
	 * 1:1 mapping for now.
	 */
	return hpa2hva(ioapic_paddr);
}

static inline uint32_t ioapic_read_reg32(void *ioapic_base, const uint32_t offset)
{
	uint32_t v;
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Read  IOWIN */
	v = mmio_read32(ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
	return v;
}

static inline void ioapic_write_reg32(void *ioapic_base, const uint32_t offset, const uint32_t value)
{
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Write IOWIN */
	mmio_write32(value, ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
}

static inline void ioapic_set_rte_entry(void *ioapic_addr, uint32_t pin, union ioapic_rte rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	ioapic_write_reg32(ioapic_addr, rte_addr, rte.u.lo_32);
	ioapic_write_reg32(ioapic_addr, rte_addr + 1U, rte.u.hi_32);
}

static void ioapic_set_routing(void *addr, uint32_t gsi)
{
	union ioapic_rte rte;

	rte.full = 0UL;
	rte.bits.intr_mask = IOAPIC_RTE_MASK_SET;
	ioapic_set_rte_entry(addr, gsi, rte);
	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx", gsi, gsi, rte.full);
}

bool ioapic_irq_is_gsi(uint32_t irq)
{
	return irq < ioapic_nr_gsi;
}

static uint32_t ioapic_nr_pins(void *ioapic_base)
{
	uint32_t version;
	uint32_t nr_pins;

	version = ioapic_read_reg32(ioapic_base, IOAPIC_VER);
	dev_dbg(ACRN_DBG_IRQ, "IOAPIC version: %x", version);

	/* The 23:16 bits in the version register is the highest entry in the
	 * I/O redirection table, which is 1 smaller than the number of
	 * interrupt input pins. */
	nr_pins = (((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1U);

	return nr_pins;
}

void ioapic_setup_irqs(void)
{
	struct ioapic_info ioapic_array[CONFIG_MAX_IOAPIC_NUM];
	uint16_t ioapic_num;
	uint8_t ioapic_id;
	uint32_t gsi;

	spinlock_init(&ioapic_lock);

	ioapic_num = parse_madt_ioapic(&ioapic_array[0]);
	for (ioapic_id = 0U; ioapic_id < ioapic_num; ioapic_id++) {
		void *addr;
		uint32_t nr_pins;

		addr = map_ioapic(ioapic_array[ioapic_id].addr);
		hv_access_memory_region_update((uint64_t)addr, PAGE_SIZE);

		nr_pins = ioapic_nr_pins(addr);
		for (gsi = 0U; gsi < nr_pins; gsi++) {
			ioapic_set_routing(addr, gsi);
		}
	}

	/* system max gsi numbers */
	ioapic_nr_gsi = gsi;
}

/**
 * @}
 */
