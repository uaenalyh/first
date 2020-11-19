/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MMU_H
#define MMU_H

/**
 * @addtogroup hwmgmt_mmu
 *
 * @{
 */

/**
 * @file
 * @brief This file declares public APIs, macros and structures of hwmgmt.mmu module.
 */

#define MEM_1K 1024U /**< 1-Kbyte memory size */
#define MEM_2K (MEM_1K * 2U) /**< 2-Kbyte memory size */
#define MEM_4K (MEM_1K * 4U) /**< 4-Kbyte memory size */
#define MEM_1M (MEM_1K * 1024U) /**< 1-Mbyte memory size */
#define MEM_2M (MEM_1M * 2U) /**< 2-Mbyte memory size */

#ifndef ASSEMBLER

#include <cpu.h>
#include <page.h>
#include <pgtable.h>
#include <cpu_caps.h>

#define CACHE_LINE_SIZE 64U /**< 64 bytes cache line size */

struct acrn_vcpu;

/**
 * @brief This function rounds \a addr to the nearest page 4-Kbyte boundary
 *        greater than or equal to that addr.
 *
 * @param[in] addr The address whose nearest page 4-Kbyte boundary
 *                 greater than or equal to it is to be calculated.
 *
 * @return The nearest page 4-Kbyte boundary greater than or equal to \a addr.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t round_page_up(uint64_t addr)
{
	/** Return the nearest page 4-Kbyte boundary greater than or equal to \a addr. */
	return (((addr + (uint64_t)PAGE_SIZE) - 1UL) & PAGE_MASK);
}

/**
 * @brief This function rounds \a addr to the nearest page 4-Kbyte boundary
 *        less than or equal to that addr.
 *
 * @param[in] addr The address whose nearest page 4-Kbyte boundary
 *                 less than or equal to it is to be calculated.
 *
 * @return The nearest page 4-Kbyte boundary less than or equal to \a addr.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t round_page_down(uint64_t addr)
{
	/** Return the nearest page 4-Kbyte boundary less than or equal to \a addr. */
	return (addr & PAGE_MASK);
}

/**
 * @brief This function is used to calculate \a given addr's upper page directory address.
 *
 * @param[in] val The address whose upper page directory address to be calculated.
 *
 * @return The upper page directory address of \a val.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint64_t round_pde_up(uint64_t val)
{
	/** Return the upper page directory address of \a val */
	return (((val + (uint64_t)PDE_SIZE) - 1UL) & PDE_MASK);
}

/**
 * @brief This function is used to calculate \a given addr's lower page directory address.
 *
 * @param[in] val The address whose lower page directory address to be calculated.
 *
 * @return The lower page directory address of \a val.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint64_t round_pde_down(uint64_t val)
{
	/** Return the lower page directory address of \a val */
	return (val & PDE_MASK);
}

/**
 * @brief Page tables level in IA32 paging mode.
 */
enum page_table_level {
	/**
	 * @brief The PML4 level in the page tables.
	 *
	 * The value is fixed to 0.
	 */
	IA32E_PML4 = 0,
	/**
	 * @brief The Page-Directory-Pointer-Table level in the page tables.
	 *
	 * The value is fixed to 1.
	 */
	IA32E_PDPT = 1,
	/**
	 * @brief The Page-Directory level in the page tables.
	 *
	 * The value is fixed to 2.
	 */
	IA32E_PD = 2,
	/**
	 * @brief The Page-Table level in the page tables.
	 *
	 * The value is fixed to 3.
	 */
	IA32E_PT = 3,
};

void sanitize_pte_entry(uint64_t *ptep, const struct memory_ops *mem_ops);
void sanitize_pte(uint64_t *pt_page, const struct memory_ops *mem_ops);
void enable_paging(void);
void enable_smep(void);
void enable_smap(void);
void init_paging(void);
void hv_access_memory_region_update(uint64_t base, uint64_t size);
void flush_vpid_global(void);
void flush_address_space(void *addr, uint64_t size);
void invept(const void *eptp);

/**
 * @brief Writes back all modified cache lines in the processor’s internal
 *        cache to main memory and invalidates (flushes) the internal caches.
 *
 * It is a wrap of "wbinvd" inline assembly.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline void cache_flush_invalidate_all(void)
{
	/** Execute inline assembly ("wbinvd") with following parameters,
	 *  in order to write back all modified cache lines in the processor’s
	 *  internal cache to main memory and invalidates (flushes) the internal
	 *  caches.
	 *  - Instruction template: "wbinvd\n"
	 *  - Input operands: None
	 *  - Output operands: None
	 *  - Clobbers: "memory"
	 */
	asm volatile("   wbinvd\n" : : : "memory");
}

/**
 * @brief Flushes cache line containing \a p.
 *
 * Invalidates from every level of the cache hierarchy in the cache
 * coherence domain the cache line that contains the linear address
 * specified with \a p. It is a wrap of "clflush" inline assembly.
 *
 * @param[in] p The linear address to be flushed.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void clflush(const volatile void *p)
{
	/** Execute inline assembly ("clflush") with following parameters,
	 *  in order to invalidate from every level of the cache hierarchy
	 *  in the cache coherence domain the cache line that contains the
	 *  linear address specified with \a p.
	 *  - Instruction template: "clflush (%0)"
	 *  - Input operands:
	 *	- A general register holds the linear address to be flushed.
	 *  - Output operands: None
	 *  - Clobbers: None
	 */
	asm volatile("clflush (%0)" ::"r"(p));
}

/**
 * @brief Optimally flushes cache line containing \a p.
 *
 * Invalidates from every level of the cache hierarchy in the cache
 * coherence domain the cache line that contains the linear address
 * specified with \a p. It is a wrap of "clflushopt" inline assembly.
 * "clflushopt" will provide a performance benefit over "clflush" for
 * cache lines in any coherence states. "clflushopt" is more suitable
 * to flush large buffers (e.g. greater than many KBytes), compared
 * to "clflush".
 *
 * @param[in] p The the linear address to be flushed.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void clflushopt(const volatile void *p)
{
	/** Execute inline assembly ("clflushopt") with following parameters,
	 *  in order to invalidate from every level of the cache hierarchy
	 *  in the cache coherence domain the cache line that contains the
	 *  linear address specified with \a p.
	 *  - Instruction template: "clflushopt (%0)"
	 *  - Input operands:
	 *	- A general register holds the linear address to be flushed.
	 *  - Output operands: None
	 *  - Clobbers: None
	 */
	asm volatile("clflushopt (%0)" ::"r"(p));
}

/**
 * @brief This function gets PDPT address from CR3 value in PAE mode.
 *
 * @param[in] cr3 The cr3 register value.
 *
 * @return The page directory pointer table address in PAE mode.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint64_t get_pae_pdpt_addr(uint64_t cr3)
{
	/** Return (cr3 & 0xFFFFFFE0H) as PDPT address */
	return (cr3 & 0xFFFFFFE0UL);
}

#endif /* ASSEMBLER not defined */

/**
 * @}
 */

#endif /* MMU_H */
