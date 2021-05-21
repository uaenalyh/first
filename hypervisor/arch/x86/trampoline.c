/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <e820.h>
#include <mmu.h>
#include <per_cpu.h>
#include <trampoline.h>
#include <reloc.h>
#include <ld_sym.h>
#include <acrn_hv_defs.h>

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief Define variables and implement trampoline APIs to initialize the environment before start APs.
 *
 * 1.Define variables used to boot APs.
 * 2.Implementation of trampoline APIs to initialize the environment before start APs.
 *
 * This file is decomposed into the following functions:
 *
 * - get_ap_trampoline_buf: Get the start address of the relocated trampoline section.
 * - trampoline_relo_addr: Calculate the offset of a trampoline symbol based on the start address of the trampoline
     code.
 * - write_trampoline_stack_sym: Prepare the stack to be used for the given AP.
 * - update_trampoline_code_refs: Update trampoline data lived in trampoline code.
 * - prepare_trampoline: Prepare trampoline code.
 */

/**
 * @brief An integer value representing the AP startup address.
 */
static uint64_t trampoline_start16_paddr;

/**
 * @brief Get the start address of the relocated trampoline section.
 *
 * @return Trampoline address
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static uint64_t get_ap_trampoline_buf(void)
{
	/** Declare the following local variables of type struct multiboot_info *.
	 *  - mbi representing the multiboot information structure given from the boot_regs which saves multiboot
	 *    information pointer, not initialized. */
	struct multiboot_info *mbi;
	/** Declare the following local variables of type uint32_t.
	 *  - size representing the size of the memory to be allocated for trampoline code, initialized as
	 *    CONFIG_LOW_RAM_SIZE. */
	uint32_t size = CONFIG_LOW_RAM_SIZE;
	/** Declare the following local variables of type uint64_t.
	 *  - ret representing start address of the relocated trampoline section (allocated from low 1MB physical
	 *    memory),
	 *  initialized as e820_alloc_low_memory(size). */
	uint64_t ret = e820_alloc_low_memory(size);
	/** Declare the following local variables of type uint64_t.
	 *  - end representing the end address of the relocated trampoline section, initialized as ret plus size. */
	uint64_t end = ret + size;

	/** If ret is equal to ACRN_INVALID_HPA. */
	if (ret == ACRN_INVALID_HPA) {
		/** Call panic in order to enter safety state */
		panic("invalid hpa");
	}

	/** Set mbi to to the host virtual address translated from the boot_regs[1]. */
	mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	/** If end is greater than mi_mmap_addr field of mbi and
	 *     ret is less than the value, where the value is calculated by adding mi_mmap_addr
	 *     field of mbi and mi_mmap_length field of mbi. */
	if ((end > mbi->mi_mmap_addr) && (ret < (mbi->mi_mmap_addr + mbi->mi_mmap_length))) {
		/** Call panic in order to enter safety state. */
		panic("overlaped with memroy map");
	}

	/** If end is greater than the address of mbi and
	 *  ret is less than the value, where the value is calculated by adding the address of mbi
	 *  and the size of structure multiboot_info. */
	if ((end > (uint64_t)mbi) && (ret < ((uint64_t)mbi + sizeof(struct multiboot_info)))) {
		/** Call panic in order to enter safety state. */
		panic("overlaped with multiboot information");
	}

	/** If end is greater than the mi_mods_addr field of mbi and
	 *  ret is smaller than the value, where the value is calculated by plusing mi_mods_addr field
	 *  of mbi and the x, where the x is calculated by mi_mods_count field of mbi multiplying the
	 *  size of structure multiboot_module. */
	if ((end > mbi->mi_mods_addr) &&
		(ret < (mbi->mi_mods_addr + (mbi->mi_mods_count * sizeof(struct multiboot_module))))) {
		/** Call panic in order to enter safety state. */
		panic("overlaped with module address");
	}

	/** Return 'ret'. */
	return ret;
}

/*
 * Because trampoline code is relocated in different way, if HV code
 * accesses trampoline using relative addressing, it needs to take
 * out the HV relocation delta
 *
 * This function is valid if:
 *  - The hpa of HV code is always higher than trampoline code
 *  - The HV code is always relocated to higher address, compared
 *    with CONFIG_HV_RAM_START
 */

/**
 * @brief Calculate the offset of a trampoline symbol based on the start address of the trampoline code.
 *
 * @param[in]    addr The address of symbol in trampoline code.
 *
 * @return The offset of the symbol address relative to the address of trampoline code.
 *
 * @pre addr != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static uint64_t trampoline_relo_addr(const void *addr)
{
	/** Return the value that is calculated by subtracting the return value of get_hv_image_delta() from addr. */
	return (uint64_t)addr - get_hv_image_delta();
}

/**
 * @brief Prepare the stack to be used for the given AP.
 *
 *  Prepare the stack to be used for the given AP whose id is \a pcpu_id.
 *
 * @param[in]    pcpu_id The CPU id of AP whose stack is to be setup.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark As global variable trampoline_start16_paddr (which is set up in prepare_trampoline)
 * is used inside, this function shall be called after prepare_trampoline.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void write_trampoline_stack_sym(uint16_t pcpu_id)
{
	/** Declare the following local variables of type uint64_t *.
	 *  - hva representing the stack symbol address of secondary_cpu_stack that will be updated, not initialized. */
	uint64_t *hva;
	/** Declare the following local variables of type uint64_t.
	 *  - stack_sym_addr representing stack start address of the physical CPU, where the physical CPU id is
	 *  \a pcpu_id, not initialized. */
	uint64_t stack_sym_addr;
	/** Set hva to the host virtual address translated from the value, where the value is calculated
	 *  by plusing trampoline_start16_paddr and trampoline_relo_addr(secondary_cpu_stack). */
	hva = (uint64_t *)(hpa2hva(trampoline_start16_paddr + trampoline_relo_addr(secondary_cpu_stack)));

	/** Set stack_sym_addr to the address of the stack field of the pcpu_id-th element in the per-CPU region, where
	 *  pcpu_id is calculated by subtracting 1 from PCPU_STACK_SIZE, where the CPU's id is pcpu_id.  */
	stack_sym_addr = (uint64_t)&per_cpu(stack, pcpu_id)[PCPU_STACK_SIZE - 1U];
	/** Bitwise AND the complement value by stack_sym_addr, where the value is calculated by subtracting 1 from
	 *  CPU_STACK_ALIGN.
	 *  Clear the lower bits of stack_sym_addr to make it align with CPU_STACK_ALIGN.
	 *  If bit x is 1 in 'CPU_STACK_ALIGN - 1', then, bit x needs to be cleared in stack_sym_addr. */
	stack_sym_addr &= ~(CPU_STACK_ALIGN - 1UL);
	/** Set *hva to stack_sym_addr. */
	*hva = stack_sym_addr;

	/** Call clflush with the following parameters, in order to flush cache line associated with 'hva'.
	 *  - hva */
	clflush(hva);
}

/**
 * @brief Update trampoline data lived in trampoline code.
 *
 * @param[in]    dest_pa The start address of the relocated trampoline section.
 *
 * @return None
 *
 * @pre N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void update_trampoline_code_refs(uint64_t dest_pa)
{
	/** Declare the following local variables of type void *.
	 *  - ptr representing the pointer that point to any type variable lived in trampoline code, not initialized. */
	void *ptr;
	/** Declare the following local variables of type uint64_t.
	 *  - val representing the address of the counterpart of trampoline_fixup_target
	 *  in the relocated trampoline section, not initialized. */
	uint64_t val;

	/*
	 * calculate the fixup CS:IP according to fixup target address
	 * dynamically.
	 *
	 * trampoline code starts in real mode,
	 * so the target addres is HPA
	 */
	/** Set val to 'dest_pa + trampoline_relo_addr(&trampoline_fixup_target)'. */
	val = dest_pa + trampoline_relo_addr(&trampoline_fixup_target);

	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and dest_pa and trampoline_relo_addr(&trampoline_fixup_cs). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_cs));
	/** Set the contents in the memory region(16-bit) pointed by ptr to bits 15:4 of val */
	*(uint16_t *)(ptr) = (uint16_t)((val >> 4U) & 0xFFFFU);

	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(&trampoline_fixup_ip). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_ip));
	/** Set the contents in the memory region(16-bit) pointed by ptr to bits 3:0 of val */
	*(uint16_t *)(ptr) = (uint16_t)(val & 0xfU);

	/* Update temporary page tables */
	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(&cpu_boot_page_tables_ptr). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_ptr));
	/** Increase the contents in the memory region(32-bit) pointed by ptr by bits 31:0 of dest_pa */
	*(uint32_t *)(ptr) += (uint32_t)dest_pa;

	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(&cpu_boot_page_tables_start). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_start));
	/** Increase the contents in the memory region(64-bit) pointed by ptr by dest_pa */
	*(uint64_t *)(ptr) += dest_pa;

	/* update the GDT base pointer with relocated offset */
	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(&trampoline_gdt_ptr). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_gdt_ptr));
	/** Increase the contents in the memory region(64-bit) pointed by 'ptr + 2' by dest_pa */
	*(uint64_t *)(ptr + 2) += dest_pa;

	/* update trampoline jump pointer with relocated offset */
	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(&trampoline_start64_fixup). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_start64_fixup));
	/** Increase the contents in the memory region(32-bit) pointed by ptr by dest_pa */
	*(uint32_t *)ptr += (uint32_t)dest_pa;

	/* update trampoline's main entry pointer */
	/** Set ptr to the host virtual address translated from the value, where the value is calculated
	 *  by plusing dest_pa and trampoline_relo_addr(main_entry). */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(main_entry));
	/** Increase the contents in the memory region(64-bit) pointed by ptr
	 *  by the return value of get_hv_image_delta() */
	*(uint64_t *)ptr += get_hv_image_delta();
}

/**
 * @brief Prepare trampoline code.
 *
 * @return The start address of the relocated trampoline section, which is used to start up APs.
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
uint64_t prepare_trampoline(void)
{
	/** Declare the following local variables of type uint64_t.
	 *  - size representing the size of trampoline code, not initialized.
	 *  - dest_pa representing the start address of trampoline code, not initialized.
	 *  - i representing the iteration of cache-line aligned integers from 0 to size used as
	 *    offset of the start address of trampoline code, not initialized. */
	uint64_t size, dest_pa, i;

	/** Set size to the value calculated by subtract &ld_trampoline_start from &ld_trampoline_end. */
	size = (uint64_t)(&ld_trampoline_end - &ld_trampoline_start);
	/** Set dest_pa to the return value of get_ap_trampoline_buf. */
	dest_pa = get_ap_trampoline_buf();

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - dest_pa
	 *  - size */
	pr_dbg("trampoline code: %lx size %x", dest_pa, size);

	/** Call memcpy_s with the following parameters, in order to copy trampoline section content from
	 *  &ld_trampoline_load to the virtual address translated from guest physical address dest_pa.
	 *  - hpa2hva(dest_pa)
	 *  - size
	 *  - &ld_trampoline_load
	 *  - size */
	(void)memcpy_s(hpa2hva(dest_pa), (size_t)size, &ld_trampoline_load, (size_t)size);
	/** Call update_trampoline_code_refs with the following parameters, in order to update trampoline data lived in
	 *  trampoline code.
	 *  - dest_pa */
	update_trampoline_code_refs(dest_pa);

	/** For each i ranging from 0 to val [with a step of CACHE_LINE_SIZE], where the val is calculated from the
	 * expression < ((size + CACHE_LINE_SIZE - 1)/CACHE_LINE_SIZE) * CACHE_LINE_SIZE >. */
	for (i = 0UL; i < size; i = i + CACHE_LINE_SIZE) {
		/** Call clflush with the following parameters, in order to flush cache line.
		 *  - hpa2hva(dest_pa + i) */
		clflush(hpa2hva(dest_pa + i));
	}

	/** Set trampoline_start16_paddr to dest_pa */
	trampoline_start16_paddr = dest_pa;

	/** Return 'dest_pa'. */
	return dest_pa;
}

/**
 * @}
 */
