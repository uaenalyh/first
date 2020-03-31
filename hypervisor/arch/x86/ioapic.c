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
 * @brief This file implements the API to mask all IOAPIC interrupt signals.
 *
 * External API:
 * - ioapic_setup_irqs()	This function masks all IOAPIC pins.
 *				Depends on:
 *				 - spinlock_init()
 *				 - map_ioapic()
 *				 - parse_madt_ioapic()
 *				 - ioapic_nr_pins()
 *				 - ioapic_set_routing()
 *				 - hv_access_memory_region_update()
 *
 * Internal functions:
 * - map_ioapic()		This function translates HPA of the physical IOAPIC to HVA.
 *				Depends on:
 *				 - hpa2hva()
 *
 * - ioapic_read_reg32()	This function reads a given IOAPIC register.
 *				Depends on:
 *				 - spinlock_irqsave_obtain()
 *				 - spinlock_irqrestore_release()
 *				 - mmio_write32()
 *				 - mmio_read32()
 *
 * - ioapic_write_reg32()	This function writes a given IOAPIC register.
 *				Depends on:
 *				 - spinlock_irqsave_obtain()
 *				 - spinlock_irqrestore_release()
 *				 - mmio_write32()
 *				 - mmio_read32()
 *
 * - ioapic_set_rte_entry()	This function writes IOAPIC Redirection Table Entry(RTE) register.
 *				Depends on:
 *				 - ioapic_write_reg32()
 *
 * - ioapic_set_routing()	This function masks a specific IOAPIC interrupt signal.
 *				Depends on:
 *				 - ioapic_set_rte_entry()
 *				 - dev_dbg()
 *
 * - ioapic_nr_pins()		This function gets the number of physical IOAPIC pins.
 *				Depends on:
 *				 - ioapic_read_reg32()
 *				 - dev_dbg()
 */

static uint32_t ioapic_nr_gsi;

/**
 * @brief The global spinlock that ACRN hypervisor shall acquire before accessing any
 *	  IOAPIC register and release after the access.
 */
static spinlock_t ioapic_lock;

/**
 * @brief This function translates HPA of the physical IOAPIC to HVA.
 *
 * @param[in]  ioapic_paddr	HPA of physical IOAPIC memory base physical address.
 *
 * @return HVA of physical IOAPIC memory base virtual address.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static void *map_ioapic(uint64_t ioapic_paddr)
{
	/** Return hpa2hva(ioapic_paddr) */
	return hpa2hva(ioapic_paddr);
}

/**
 * @brief This function reads a given IOAPIC register.
 *
 *  This function is used to read a given IOAPIC register.
 *
 * @param[in]  ioapic_base	Physical IOAPIC base virtual address.
 * @param[in]  offset		Address offset of the given IOAPIC register.
 *
 * @return The value of the given IOAPIC register.
 *
 * @pre ioapic_base != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static inline uint32_t ioapic_read_reg32(void *ioapic_base, const uint32_t offset)
{
	/** Declare the following local variables of type uint32_t.
	 *  - v representing value of an IOAPIC register, not initialized. */
	uint32_t v;
	/** Declare the following local variables of type uint64_t
	 *  - rflags representing a variable used to store the CPU RFLAGS register. */
	uint64_t rflags;

	/** Call spinlock_irqsave_obtain() with the following parameters,
	 *  in order to get ioapic_lock.
	 *  - &ioapic_lock
	 *  - &rflags
	 */
	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/** Call mmio_write32() with the following parameters, in order to write value
	 *  'offset' to IOAPIC IOREGSEL register.
	 *  - offset
	 *  - ioapic_base + IOAPIC_REGSEL
	 */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/** Set v to mmio_read32(ioapic_base + IOAPIC_WINDOW). */
	v = mmio_read32(ioapic_base + IOAPIC_WINDOW);

	/** Call spinlock_irqsave_release() with the following parameters,
	 *  in order to release ioapic_lock.
	 *  - &ioapic_lock
	 *  - rflags
	 */
	spinlock_irqrestore_release(&ioapic_lock, rflags);

	/** Return the value of v */
	return v;
}

/**
 * @brief This function writes a given IOAPIC register.
 *
 *  This function is used to program a given IOAPIC register.
 *
 * @param[in]  ioapic_base	HVA of IOAPIC base virtual address.
 * @param[in]  offset		Address offset of the given IOAPIC register.
 * @param[in]  value		The value to be written.
 *
 * @return None
 *
 * @pre ioapic_base != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static inline void ioapic_write_reg32(void *ioapic_base, const uint32_t offset, const uint32_t value)
{
	/** Declare the following local variables of type uint64_t
	 *  - rflags representing a variable used to store the CPU RFLAGS register. */
	uint64_t rflags;

	/** Call spinlock_irqsave_obtain() with the following parameters,
	 *  in order to get ioapic_lock.
	 *  - &ioapic_lock
	 *  - &rflags
	 */
	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/** Call mmio_write32() with the following parameters, in order to write offset to
	 *  IOAPIC IOREGSEL register.
	 *  - offset
	 *  - ioapic_base + IOAPIC_REGSEL
	 */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/** Call mmio_write32() with the following parameters, in order to write value
	 *  'value' to IOAPIC IOWIN register.
	 *  - value
	 *  - ioapic_base + IOAPIC_WINDOW
	 */
	mmio_write32(value, ioapic_base + IOAPIC_WINDOW);

	/** Call spinlock_irqsave_release() with the following parameters,
	 *  in order to release ioapic_lock.
	 *  - &ioapic_lock
	 *  - rflags
	 */
	spinlock_irqrestore_release(&ioapic_lock, rflags);
}

/**
 * @brief This function writes IOAPIC Redirection Table Entry(RTE) register.
 *
 * This function is used to program specific IOAPIC RTE.
 *
 * @param[in]  ioapic_addr	HVA of IOAPIC base virtual address.
 * @param[in]  pin		Pin number of interrupt input.
 * @param[in]  rte		RTE information to be written.
 *
 * @return None
 *
 * @pre ioapic_base != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static inline void ioapic_set_rte_entry(void *ioapic_addr, uint32_t pin, union ioapic_rte rte)
{
	/** Declare the following local variables of type uint32_t
	 *  - ret_addr representing offset of target RTE register,
	 *    initialized as ((pin * 2H) + 10H). */
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	/** Call ioapic_write_reg32() with the following parameters,
	 *  in order to write 'rte.u.lo_32' to the low 32-bits of RTE register
	 *	whose offset is 'rte_addr'(located in the memory specified by ioapic_addr).
	 *  - ioapic_addr
	 *  - rte_addr
	 *  - ret.u.lo_32
	 */
	ioapic_write_reg32(ioapic_addr, rte_addr, rte.u.lo_32);
	/** Call ioapic_write_reg32() with the following parameters,
	 *  in order to write 'rte.u.hi_32' to the high 32-bits of RTE register
	 *	whose offset is 'rte_addr'(located in the memory specified by ioapic_addr).
	 *  - ioapic_addr
	 *  - rte_addr + 1H
	 *  - ret.u.hi_32
	 */
	ioapic_write_reg32(ioapic_addr, rte_addr + 1U, rte.u.hi_32);
}

/**
 * @brief This function masks specific IOAPIC interrupt signal.
 *
 * @param[in]  addr		HVA of IOAPIC base virtual address.
 * @param[in]  gsi		Interrupt signal number.
 *
 * @return None
 *
 * @pre addr != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static void ioapic_set_routing(void *addr, uint32_t gsi)
{
	/** Declare the following local variables of type union ioapic_rte.
	 *  - ret representing IOAPIC RTE information, not initialized. */
	union ioapic_rte rte;

	/** Set rte.full to 0H */
	rte.full = 0UL;
	/** Set rte.bits.intr_mask to IOAPIC_RTE_MASK_SET */
	rte.bits.intr_mask = IOAPIC_RTE_MASK_SET;
	/** Call ioapic_set_rte_entry() with the following parameters,
	 *  in order to write rte to gsi specific RTE entry.
	 *  - addr
	 *  - gsi
	 *  - rte
	 */
	ioapic_set_rte_entry(addr, gsi, rte);
	/** Logging the following information with a log level of ACRN_DBG_IRQ.
	 *  - gsi
	 *  - gsi
	 *  - rte.full
	 */
	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx", gsi, gsi, rte.full);
}

bool ioapic_irq_is_gsi(uint32_t irq)
{
	/** Return <TBD: meaning of variable> */
	return irq < ioapic_nr_gsi;
}

/**
 * @brief This function gets the number of physical IOAPIC pins.
 *
 *  This function get total interrupt pin number for specific
 *  physical IOAPIC.
 *
 * @param[in]  ioapic_base HVA of Base address to physical IOAPIC
 *
 * @return Pin number of target physical IOAPIC.
 *
 * @pre ioapic_base != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static uint32_t ioapic_nr_pins(void *ioapic_base)
{
	/** Declare the following local variables of type uint32_t.
	 *  - version representing value of IOAPIC Version register, not initialized. */
	uint32_t version;
	/** Declare the following local variables of type uint32_t.
	 *  - nr_pins representing number of IOAPIC pins, not initialized. */
	uint32_t nr_pins;

	/** Set version to ioapic_read_reg32(ioapic_base, IOAPIC_VER) */
	version = ioapic_read_reg32(ioapic_base, IOAPIC_VER);
	/** Logging the following information with a log level of ACRN_DBG_IRQ.
	 *  -  version
	 */
	dev_dbg(ACRN_DBG_IRQ, "IOAPIC version: %x", version);

	/** Set nr_pins to (((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1H) */
	nr_pins = (((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1U);

	/** Return nr_pins */
	return nr_pins;
}

/**
 * @brief This function masks all IOAPIC pins.
 *
 * This function initializes physical IOAPIC on current platform and masks all
 * interrupt pins in IOAPIC RTE registers.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
void ioapic_setup_irqs(void)
{
	/** Declare the following local variables of struct ioapic_info[CONFIG_MAX_IOAPIC_NUM].
	 *  - ioapic_array representing information of physical IOAPICs on platform,
	 *    not initialized. */
	struct ioapic_info ioapic_array[CONFIG_MAX_IOAPIC_NUM];
	/** Declare the following local variables of type uint16_t.
	 *  - ioapic_num representing the number of physical IOAPICs, not initialized. */
	uint16_t ioapic_num;
	/** Declare the following local variables of type uint8_t.
	 *  - ioapic_id representing physical IOAPIC ID, not initialized. */
	uint8_t ioapic_id;
	/** Declare the following local variables of type uint32_t.
	 *  - gsi representing an interrupt pin number of IOAPIC, not initialized. */
	uint32_t gsi;

	/** Call spinlock_init() with the following parameters,
	 *  in order to initialize ioapic_lock.
	 *  - &ioapic_lock
	 */
	spinlock_init(&ioapic_lock);

	/** Set ioapic_num to parse_madt_ioapic(&ioapic_array[0]) */
	ioapic_num = parse_madt_ioapic(&ioapic_array[0]);
	/** For each ioapic_id ranging from 0H to ioapic_num - 1 with a step of 1H */
	for (ioapic_id = 0U; ioapic_id < ioapic_num; ioapic_id++) {
		/** Declare the following local variables of type void *.
		 *  - addr representing HVA of IOAPIC memory base address, not initialized. */
		void *addr;
		/** Declare the following local variables of type uint32_t.
		 *  - nr_pins representing number of IOAPIC pins, not initialized. */
		uint32_t nr_pins;

		/** Set addr to map_ioapic(ioapic_array[ioapic_id].addr) */
		addr = map_ioapic(ioapic_array[ioapic_id].addr);
		/** Call hv_access_memory_region_update() with the following parameters,
		 *  in order to update hypervisor page table to grant hypervisor access
		 *  the memory space of this IOAPIC.
		 *  - (uint64_t)addr
		 *  - PAGE_SIZE
		 */
		hv_access_memory_region_update((uint64_t)addr, PAGE_SIZE);

		/** Set nr_pins to ioapic_nr_pins(addr) */
		nr_pins = ioapic_nr_pins(addr);
		/** For each gsi ranging from 0H to nr_pins - 1 with a step of 1H */
		for (gsi = 0U; gsi < nr_pins; gsi++) {
			/** Call ioapic_set_routing() with the following parameters,
			 *  in order to mask this interrupt signal.
			 *  - addr
			 *  - gsi
			 */
			ioapic_set_routing(addr, gsi);
		}
	}

	/** Set ioapic_nr_gsi to gsi, which means the system maximum GSI number. */
	ioapic_nr_gsi = gsi;
}

/**
 * @}
 */
