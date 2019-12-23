/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <mmu.h>
#include <per_cpu.h>
#include <trampoline.h>
#include <reloc.h>
#include <vboot.h>
#include <ld_sym.h>

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

static uint64_t trampoline_start16_paddr;

uint64_t get_ap_trampoline_buf(void)
{
        struct multiboot_info *mbi;
        uint32_t size = CONFIG_LOW_RAM_SIZE;
        uint64_t ret = e820_alloc_low_memory(size);
        uint64_t end = ret + size;

        mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
        if ((end > mbi->mi_mmap_addr) && (ret < (mbi->mi_mmap_addr + mbi->mi_mmap_length))) {
                panic("overlaped with memroy map");
        }

        if ((end > (uint64_t)mbi) && (ret < ((uint64_t)mbi + sizeof(struct multiboot_info)))) {
                panic("overlaped with multiboot information");
        }

        if ((end > mbi->mi_mods_addr) &&
                (ret < (mbi->mi_mods_addr + mbi->mi_mods_count * sizeof(struct multiboot_module)))) {
                panic("overlaped with module address");
        }

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
static uint64_t trampoline_relo_addr(const void *addr)
{
	return (uint64_t)addr - get_hv_image_delta();
}

void write_trampoline_stack_sym(uint16_t pcpu_id)
{
	uint64_t *hva, stack_sym_addr;
	hva = (uint64_t *)(hpa2hva(trampoline_start16_paddr) + trampoline_relo_addr(secondary_cpu_stack));

	stack_sym_addr = (uint64_t)&per_cpu(stack, pcpu_id)[CONFIG_STACK_SIZE - 1];
	stack_sym_addr &= ~(CPU_STACK_ALIGN - 1UL);
	*hva = stack_sym_addr;

	clflush(hva);
}

/*
 * @pre pcpu_has_cap(X86_FEATURE_PAGE1GB) == true
 */
static void update_trampoline_code_refs(uint64_t dest_pa)
{
	void *ptr;
	uint64_t val;

	/*
	 * calculate the fixup CS:IP according to fixup target address
	 * dynamically.
	 *
	 * trampoline code starts in real mode,
	 * so the target addres is HPA
	 */
	val = dest_pa + trampoline_relo_addr(&trampoline_fixup_target);

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_cs));
	*(uint16_t *)(ptr) = (uint16_t)((val >> 4U) & 0xFFFFU);

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_ip));
	*(uint16_t *)(ptr) = (uint16_t)(val & 0xfU);

	/* Update temporary page tables */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_ptr));
	*(uint32_t *)(ptr) += (uint32_t)dest_pa;

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_start));
	*(uint64_t *)(ptr) += dest_pa;

	/* update the gdt base pointer with relocated offset */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_gdt_ptr));
	*(uint64_t *)(ptr + 2) += dest_pa;

	/* update trampoline jump pointer with relocated offset */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_start64_fixup));
	*(uint32_t *)ptr += (uint32_t)dest_pa;

	/* update trampoline's main entry pointer */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(main_entry));
	*(uint64_t *)ptr += get_hv_image_delta();
}

uint64_t prepare_trampoline(void)
{
	uint64_t size, dest_pa, i;

	size = (uint64_t)(&ld_trampoline_end - &ld_trampoline_start);
	dest_pa = get_ap_trampoline_buf();

	pr_dbg("trampoline code: %lx size %x", dest_pa, size);

	/* Copy segment for AP initialization code below 1MB */
	(void)memcpy_s(hpa2hva(dest_pa), (size_t)size, &ld_trampoline_load, (size_t)size);
	update_trampoline_code_refs(dest_pa);

	for (i = 0UL; i < size; i = i + CACHE_LINE_SIZE) {
		clflush(hpa2hva(dest_pa + i));
	}

	trampoline_start16_paddr = dest_pa;

	return dest_pa;
}

/**
 * @}
 */
