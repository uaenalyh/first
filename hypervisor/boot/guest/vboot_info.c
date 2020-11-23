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
#include <mmu.h>
#include <vm.h>
#include <logmsg.h>
#include <vboot_info.h>

/**
 * @defgroup vp-base_vboot vp-base.vboot
 * @ingroup vp-base
 * @brief Implementation of vboot releated utility functions and APIs to collect the base addresses where the OS
 * kernel images and boot-time arguments of each guest VM is currently placed, sizes of these images and the guest
 * physical addresses where the OS kernel images shall be loaded.
 *
 * This module interacts with with the following modules:
 *
 *    - hwmgmt.page
 *
 *      Use the hwmgmt.base module to do address translation between host physical address and host virtual address
 *
 *      All the APIs used in hwmgmt.page module listing below:
 *
 *          + hpa2hva
 *
 *    - hwmgmt.cpu
 *
 *      Use the hwmgmt.cpu module to allow the hypervisor to access memory regions allocated to VMs.
 *
 *          + stac
 *          + clac
 *
 *    - hwmgmt.configs
 *
 *      Use the hwmgmt.configs module to get the vm configuration data.
 *      All the APIs used in hwmgmt.configs module listing below:
 *
 *          + get_vm_config
 *
 *    - lib.util
 *
 *      Use the lib.util module to calculate string lengths and compare strings.
 *
 *      All the APIs used in lib.util module listing below:
 *          + strnlen_s
 *          + strncmp
 *
 *    - debug
 *
 *      Use the debug module to log a message in debug version, do nothing in release version.
 *      All the APIs used in debug module listing below:
 *
 *          + dev_dbg
 *
 * This module is decomposed into the following files:
 *
 *     vboot_info.c        -Implements all APIs that shall be provided by the vp-base.vboot
 *     vboot_info.h        -Declares all APIs that shall be provided by the vp-base.vboot
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all APIs that shall be provided by the vp-base.vboot
 *
 * This file is decomposed into the following functions:
 *
 *     init_vm_boot_info         -Initialize virtual platform boot info
 *     get_mod_idx_by_tag        -Using tag to find the matching module embedded in module structure array
 *     init_vm_bootargs_info     -Initialize virtual machine bootargs_info field.
 *     init_vm_kernel_info       -Initialize the virtual machine kernel_info field of the given argument \a vm
 *     get_kernel_load_addr      -Get guest physical address where the kernel image will be placed to
 */

/**
 * @brief The severity of console log level
 *
 * Specify the console level of debugging messages in this module
 */
#define ACRN_DBG_BOOT 6U

/**
 * @brief The invalid index of module structure array
 */
#define INVALID_MOD_IDX 0xFFFFU

/**
 * @brief Get guest physical address where the kernel image will be placed to
 *
 * This function returns the guest physical address where the OS kernel of this VM shall be placed to
 *
 * @param[in] vm a pointer to VM whose guest physical address where the kernel image will be placed to, will be
 * collected and return.
 *
 * @return a value representing kernel loading address
 *
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_BZIMAGE ||
 * get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_ZEPHYR
 * @pre get_vm_config(0)->os_config.kernel_load_addr !=NULL
 * @pre get_vm_config(0)->os_config.kernel_entry_addr != NULL
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy unspecified
 * @threadsafety \a vm is different among parallel invocation
 */
static uint64_t get_kernel_load_addr(struct acrn_vm *vm)
{
	/** Declare the following local variables of type void *.
	 *  - load_addr representing the guest physical address where the kernel image will be placed to, initialized
	 *    as 0UL. */
	uint64_t load_addr = 0UL;
	/** Declare the following local variables of type struct vm_sw_info *.
	 *  - sw_info representing the VM software information structure given by vm, initialized as &vm->sw. */
	struct vm_sw_info *sw_info = &vm->sw;
	/** Declare the following local variables of type struct zero_page *.
	 *  - zeropage representing the kernel header of the OS kernel image of the given VM, where the kernel header
	 *    is in compliance with linux kernel boot protocol */
	struct zero_page *zeropage;
	/** Declare the following local variables of type struct acrn_vm_config *.
	 *  - vm_config representing the virtual platform configuration structure of the given VM, initialized as
	 *    get_vm_config(vm->vm_id). */
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/** Depending on the kenrel_type field of sw_info */
	switch (sw_info->kernel_type) {
	/** kernel type is KERNEL_BZIMAGE */
	case KERNEL_BZIMAGE:
		/* According to the explaination for pref_address
		 * in Documentation/x86/boot.txt, a relocating
		 * bootloader should attempt to load kernel at pref_address
		 * if possible. A non-relocatable kernel will unconditionally
		 * move itself and to run at this address, so no need to copy
		 * kernel to perf_address by bootloader, if kernel is
		 * non-relocatable.
		 */
		/** Set zeropage to the kernel_src_addr field of kernel_info of sw_info, where zeropage structure is a
		 *  linux kernel boot protocol header in the beginning of the guest OS kernel image */
		zeropage = (struct zero_page *)sw_info->kernel_info.kernel_src_addr;
		/** Set load_addr to the pref_addr field in the header of zeropage */
		load_addr = zeropage->hdr.pref_addr;
		/** End of case */
		break;
	/** kernel type is KERNEL_ZEPHYR */
	case KERNEL_ZEPHYR:
		/** Set load_addr to the kernel_load_addr field of os_config of vm_config */
		load_addr = vm_config->os_config.kernel_load_addr;
		/** End of case */
		break;
	/** Otherwise */
	default:
		/* kernel type is either KERNEL_BZIMAGE or KERNEL_ZEPHYR, do nothing here */
		/** Unexpected case. Break gracefully without changing load_addr as a defensive action. */
		break;
	}

	/** Return the guest physical address where the kernel image will be placed to */
	return load_addr;
}

/**
 * @brief Initialize the virtual machine kernel_info field of the given argument \a vm
 *
 * @param[inout] vm a pointer to VM whose kernel information will be initialized, where the kernel information
 * including list below:
 * - the base address where the OS kernel image is
 * - the size of kernel image
 * - the guest physical address where the OS kernel image shall be loaded
 * @param[in] mod pointer to multiboot module structure
 *
 * @return None
 *
 * @pre vm != NULL && mod != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_BZIMAGE ||
 * get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_ZEPHYR
 * @pre get_vm_config(0)->os_config.kernel_load_addr !=NULL
 * @pre get_vm_config(0)->os_config.kernel_entry_addr != NULL
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy unspecified
 * @threadsafety \a vm is different among parallel invocation
 */
static void init_vm_kernel_info(struct acrn_vm *vm, const struct multiboot_module *mod)
{
	/** Declare the following local variables of type struct acrn_vm_config *.
	 *  - vm_config representing the virtual platform configuration structure of the given VM, initialized as
	 *    get_vm_config(vm->vm_id). */
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/** Logging the following information with a log level of ACRN_DBG_BOOT.
	 *  - mod->mm_mod_start
	 *  - mod->mm_mod_end */
	dev_dbg(ACRN_DBG_BOOT, "kernel mod start=0x%x, end=0x%x", mod->mm_mod_start, mod->mm_mod_end);

	/** Set the kernel_type field of sw field of vm to the kernel_type field of os_config of vm_config */
	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	/** Set the kernel_src_addr field of kernel_info of vm's sw field to the host virtual address translated
	 *  from the module start address in mod */
	vm->sw.kernel_info.kernel_src_addr = hpa2hva((uint64_t)mod->mm_mod_start);
	/** Set the kernel_size field of kernel_info of vm's sw field to the value calculated by subtracting the
	 *  mm_mod_start field from the mm_mod_end field in mod */
	vm->sw.kernel_info.kernel_size = mod->mm_mod_end - mod->mm_mod_start;
	/** Set the kernel_load_addr field of kernel_info of vm's sw field to get_kernel_load_addr(vm) */
	vm->sw.kernel_info.kernel_load_addr = get_kernel_load_addr(vm);
}

/**
 * @brief Initialize virtual machine bootargs_info field.
 *
 * @param[inout] vm a pointer to VM whose boot-time information will be initialized, where boot-time information
 * including list below:
 * - the host virtual address where placed boot-time arguments of the given VM
 - - the size of the boot-time arguments of the given VM
 * - the guest physical address where boot-time arguments will be placed.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy unspecified
 * @threadsafety \a vm is different among parallel invocation
 */
static void init_vm_bootargs_info(struct acrn_vm *vm)
{
	/** Declare the following local variables of type struct acrn_vm_config *.
	 *  - vm_config representing the virtual platform configuration structure of the given VM, initialized as
	 *    get_vm_config(vm->vm_id). */
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	/** Declare the following local variables of type char *.
	 *  - bootargs representing the boot argument of virtual machine, initialized as
	 *    vm_config->os_config.bootargs;*/
	char *bootargs = vm_config->os_config.bootargs;

	/** Set the src_addr field of bootargs_info of vm's sw field to bootargs */
	vm->sw.bootargs_info.src_addr = bootargs;
	/** Set the size field of bootargs_info of vm's sw field to strnlen_s(bootargs, MAX_BOOTARGS_SIZE) */
	vm->sw.bootargs_info.size = strnlen_s(bootargs, MAX_BOOTARGS_SIZE);

	/* Kernel bootarg and zero page are right before the kernel image */
	/** If the size field of bootargs of vm's sw is greater than 0 */
	if (vm->sw.bootargs_info.size > 0U) {
		/** Set the load_addr field of bootargs_info of vm's sw field to the address that calculated by
		 *  subtracting 8K from the kernel_load_addr field of kernel_info of vm's sw field */
		vm->sw.bootargs_info.load_addr = vm->sw.kernel_info.kernel_load_addr - (MEM_1K * 8U);
	} else {
		/** Set the load_addr field of bootargs_info of vm's sw field to NULL */
		vm->sw.bootargs_info.load_addr = 0UL;
	}
}

/**
 * @brief Using \a tag to find the matching module embedded in module structure array
 *
 * Finding the matching module by iterating module embedded in module structure array by comparing the string
 * field of each module with the \a tag string.
 *
 * @param[in] mods pointer to multiboot module structure array
 * @param[in] mods_count an integer value indicating the amount of module structures that embedded in the module
 * structure array.
 * @param[in] tag a string used to find the matching module.
 *
 * @return the found index by using \a tag to find the matching module embedded in \a mods.
 *
 * @pre mods != NULL
 * @pre tag != NULL
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy unspecified
 * @threadsafety yes
 */
static uint32_t get_mod_idx_by_tag(const struct multiboot_module *mods, uint32_t mods_count, const char *tag)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing iteration number.
	 *  - ret representing the return value of get_mod_idx_by_tag, initialized as INVALID_MOD_IDX. */
	uint32_t i, ret = INVALID_MOD_IDX;
	/** Declare the following local variables of type uint32_t.
	 *  - tag_len representing the string length of argument tag, initialized as
	 *    strlen_s(tag, MAX_MOD_TAG_LEN). */
	uint32_t tag_len = strnlen_s(tag, MAX_MOD_TAG_LEN);

	/** For each i ranging from 0 to mods_count [with a step of 1] */
	for (i = 0U; i < mods_count; i++) {
		/** Declare the following local variables of type const char *.
		 *  - mm_string representing the mm_string field of the ith entry in mods, initialized as
		 *    hpa2hva(mods[i].mm_string). */
		const char *mm_string = (char *)hpa2hva((uint64_t)mods[i].mm_string);
		/** Declare the following local variables of type uint32_t.
		 *  - mm_str_len representing the string length of mm_string field of the ith entry in mods,
		 *    initialized as strnlen_s(mm_string, MAX_MOD_TAG_LEN). */
		uint32_t mm_str_len = strnlen_s(mm_string, MAX_MOD_TAG_LEN);

		/* when do file stitch by tool, the tag in mm_string might be followed with 0x0d or 0x0a */
		/** If mm_str_len is greater than tag_len
		 *  and the first tag_len characters in mm_string are the same as tag
		 *  and the content of mm_string[tag_len] is equal to 0x0d/0x0a/0 */
		if ((mm_str_len >= tag_len) && (strncmp(mm_string, tag, tag_len) == 0) &&
			((*(mm_string + tag_len) == 0x0d) || (*(mm_string + tag_len) == 0x0a) ||
				(*(mm_string + tag_len) == 0))) {
			/** Set ret to i */
			ret = i;
			/** Terminate the loop */
			break;
		}
	}
	/** Return the found index of module structure array */
	return ret;
}

/**
 * @brief initialize virtual platform boot info
 *
 * @param[inout] vm a pointer to VM whose kernel loading address(guest physical address) and boot-time arguments will
 * be initialized.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_BZIMAGE ||
 * get_vm_config(vm->vm_id)->os_config.kernel_type == KERNEL_ZEPHYR
 * @pre (strutct multiboot_module *)hpa2hva(mbi->mi_mods_addr)->mm_mod_start > 100000H
 * @pre (strutct multiboot_module *)((unsigned int)hpa2hva(mbi->mi_mods_addr)+16H)->mm_mod_start > 100000H
 * @pre get_vm_config(0)->os_config.kernel_load_addr !=NULL
 * @pre get_vm_config(0)->os_config.kernel_entry_addr != NULL
 * @pre there exists 0 <= i <= 1 such that
 * ((struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr))[i].mm_string = "linux" and
 * ((struct zero_page *)((struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr))[i])->hdr.pref_addr != NULL
 * @pre hpa2hva(boot_regs[0]) == MULTIBOOT_INFO_MAGIC
 * @pre hpa2hva((uint64_t)boot_regs[1]) != NULL
 * @pre ((struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1])->mi_flags & MULTIBOOT_INFO_HAS_MODS) != 0
 * @pre get_vm_config(vm->vm_id)->os_config.kernel_mod_tag != NULL
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy unspecified
 * @threadsafety \a vm is different among parallel invocation
 */
void init_vm_boot_info(struct acrn_vm *vm)
{
	/** Declare the following local variables of type struct multiboot_info *.
	 *  - mbi representing the multiboot information structure given from the boot_regs which saves multiboot
	 *  information pointer, initialized as hpa2hva(boot_regs[1]). */
	struct multiboot_info *mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	/** Declare the following local variables of type struct acrn_vm_config *.
	 *  - vm_config representing the virtual platform configuration structure of the given VM. */
	struct acrn_vm_config *vm_config;
	/** Declare the following local variables of type struct multiboot_module *.
	 *  - mods representing the module structure array whose elements comply with multiboot protocol. */
	struct multiboot_module *mods;
	/** Declare the following local variables of type uint32_t.
	 *  - mod_idx representing the index of the module structure array. */
	uint32_t mod_idx;

	/** Call stac in order to set the AC flag bit in the EFLAGS register and make the supervisor-mode data have
	 *  rights to access the user-mode pages even if the SMAP bit is set in the CR4 register.*/
	stac();
	/** Logging the following information with a log level of ACRN_DBG_BOOT.
	 *  - mbi->mi_flags */
	dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);

	/** Set vm_config to the virtual platform configuration structure of the given VM */
	vm_config = get_vm_config(vm->vm_id);
	/** set mods to the host virtual address translated from the mi_mods_addr field of mbi.*/
	mods = (struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr);

	/** Logging the following information with a log level of ACRN_DBG_BOOT.
	 *  - mbi->mi_mods_count */
	dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	/** Set mod_idx to the index into the module structure array of mbi that matches the module tag specified
	 *  in the VM configuration for vm. */
	mod_idx = get_mod_idx_by_tag(mods, mbi->mi_mods_count, vm_config->os_config.kernel_mod_tag);
	/** Call init_vm_kernel_info with the following parameters, in order to initialize the kernel_info field and
	 *  the kernel_type field of vm->sw field.
	 *  - vm
	 *  - &mods[mod_idx] */
	init_vm_kernel_info(vm, &mods[mod_idx]);
	/** Call init_vm_bootargs_info with the following parameters, in order to initialize the bootargs_info field
	 *  of vm->sw field.
	 *  - vm */
	init_vm_bootargs_info(vm);
	/** Call clac in order to clear the AC bit in the EFLAGS register and make the supervisor-mode data do not
	 *  have rights to access the user-mode pages*/
	clac();
}

/**
 * @}
 */
