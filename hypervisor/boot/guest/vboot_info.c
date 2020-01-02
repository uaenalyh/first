/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <per_cpu.h>
#include <irq.h>
#include <boot_context.h>
#include <multiboot.h>
#include <pgtable.h>
#include <zeropage.h>
#include <seed.h>
#include <mmu.h>
#include <vm.h>
#include <logmsg.h>
#include <deprivilege_boot.h>
#include <vboot_info.h>

/**
 * @addtogroup vp-base_vboot
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define ACRN_DBG_BOOT 6U

#define INVALID_MOD_IDX 0xFFFFU

/**
 * @pre vm != NULL
 */
static void *get_kernel_load_addr(struct acrn_vm *vm)
{
	void *load_addr = NULL;
	struct vm_sw_info *sw_info = &vm->sw;
	struct zero_page *zeropage;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	switch (sw_info->kernel_type) {
	case KERNEL_BZIMAGE:
		/* According to the explaination for pref_address
		 * in Documentation/x86/boot.txt, a relocating
		 * bootloader should attempt to load kernel at pref_address
		 * if possible. A non-relocatable kernel will unconditionally
		 * move itself and to run at this address, so no need to copy
		 * kernel to perf_address by bootloader, if kernel is
		 * non-relocatable.
		 */
		zeropage = (struct zero_page *)sw_info->kernel_info.kernel_src_addr;
		if (zeropage->hdr.relocatable_kernel != 0U) {
			zeropage = (struct zero_page *)zeropage->hdr.pref_addr;
		}
		load_addr = (void *)zeropage;
		break;
	case KERNEL_ZEPHYR:
		load_addr = (void *)vm_config->os_config.kernel_load_addr;
		break;
	default:
		pr_err("Unsupported Kernel type.");
		break;
	}
	if (load_addr == NULL) {
		pr_err("Could not get kernel load addr of VM %d .", vm->vm_id);
	}
	return load_addr;
}

/**
 * @pre vm != NULL && mod != NULL
 */
static void init_vm_kernel_info(struct acrn_vm *vm, const struct multiboot_module *mod)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	dev_dbg(ACRN_DBG_BOOT, "kernel mod start=0x%x, end=0x%x", mod->mm_mod_start, mod->mm_mod_end);

	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	vm->sw.kernel_info.kernel_src_addr = hpa2hva((uint64_t)mod->mm_mod_start);
	vm->sw.kernel_info.kernel_size = mod->mm_mod_end - mod->mm_mod_start;
	vm->sw.kernel_info.kernel_load_addr = get_kernel_load_addr(vm);
}

/**
 * @pre vm != NULL && mbi != NULL
 */
static void init_vm_bootargs_info(struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	char *bootargs = vm_config->os_config.bootargs;

	vm->sw.bootargs_info.src_addr = bootargs;
	vm->sw.bootargs_info.size = strnlen_s(bootargs, MAX_BOOTARGS_SIZE);

	/* Kernel bootarg and zero page are right before the kernel image */
	if (vm->sw.bootargs_info.size > 0U) {
		vm->sw.bootargs_info.load_addr = vm->sw.kernel_info.kernel_load_addr - (MEM_1K * 8U);
	} else {
		vm->sw.bootargs_info.load_addr = NULL;
	}
}

/* @pre mods != NULL
 */
static uint32_t get_mod_idx_by_tag(const struct multiboot_module *mods, uint32_t mods_count, const char *tag)
{
	uint32_t i, ret = INVALID_MOD_IDX;
	uint32_t tag_len = strnlen_s(tag, MAX_MOD_TAG_LEN);

	for (i = 0U; i < mods_count; i++) {
		const char *mm_string = (char *)hpa2hva((uint64_t)mods[i].mm_string);
		uint32_t mm_str_len = strnlen_s(mm_string, MAX_MOD_TAG_LEN);

		/* when do file stitch by tool, the tag in mm_string might be followed with 0x0d or 0x0a */
		if ((mm_str_len >= tag_len) && (strncmp(mm_string, tag, tag_len) == 0) &&
			((*(mm_string + tag_len) == 0x0d) || (*(mm_string + tag_len) == 0x0a) ||
				(*(mm_string + tag_len) == 0))) {
			ret = i;
			break;
		}
	}
	return ret;
}

/* @pre vm != NULL && mbi != NULL
 */
static void init_vm_sw_load(struct acrn_vm *vm, const struct multiboot_info *mbi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct multiboot_module *mods = (struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr);
	uint32_t mod_idx;
	dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	mod_idx = get_mod_idx_by_tag(mods, mbi->mi_mods_count, vm_config->os_config.kernel_mod_tag);
	init_vm_kernel_info(vm, &mods[mod_idx]);
	init_vm_bootargs_info(vm);
}

/**
 * @pre vm != NULL
 */
static void init_general_vm_boot_info(struct acrn_vm *vm)
{
	struct multiboot_info *mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	stac();
	dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);
	init_vm_sw_load(vm, mbi);
	clac();
}

/**
 * @param[inout] vm pointer to a vm descriptor
 * @pre vm != NULL
 */
void init_vm_boot_info(struct acrn_vm *vm)
{
	init_general_vm_boot_info(vm);
}

/**
 * @}
 */
