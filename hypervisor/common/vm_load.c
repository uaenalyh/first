/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <e820.h>
#include <zeropage.h>
#include <ept.h>
#include <mmu.h>
#include <multiboot.h>
#include <errno.h>
#include <logmsg.h>

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file implements the functions to preload guest OS images.
 *
 * This file mainly implements the API "direct_boot_sw_loader" which is called by launch_vms.
 * It initializes the guest vCPU registers, copies the guest OS image / boot arguments (if included) into
 * its specified guest physical address, and sets the RIP of the VM's guest BP to the guest OS image entry.
 *
 * It defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: get_guest_gdt_base_gpa, create_zeropage_e820, create_zero_page.
 *
 * Decomposed functions include: prepare_loading_bzimage, prepare_loading_rawimage.
 */


/**
 * @brief Calculate a guest GDT base for the given VM.
 *
 * Hypervisor puts the guest initial GDT in the RAM after the space which loads the kernel and boot arguments. Suppose
 * this is a safe place for guest initial GDT.
 *
 * @param[in] vm Pointer to a VM data structure which contains kernel and boot arguments info.
 *
 * @return A GPA to place the GDT
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by direct_boot_sw_loader.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static uint64_t get_guest_gdt_base_gpa(const struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint64_t.
	 *  - new_guest_gdt_base_gpa representing a GPA of guest GDT, not initialized.
	 *  - guest_kernel_end_gpa representing the ending GPA of guest kernel image load space, not initialized.
	 *  - guest_bootargs_end_gpa representing the ending GPA of guest boot arguments load space, not initialized.
	 */
	uint64_t new_guest_gdt_base_gpa, guest_kernel_end_gpa, guest_bootargs_end_gpa;

	/** Set guest_kernel_end_gpa to kernel load address + its size, then it is the ending address */
	guest_kernel_end_gpa = vm->sw.kernel_info.kernel_load_addr + vm->sw.kernel_info.kernel_size;
	/** Set guest_bootargs_end_gpa to boot arguments load address + its size */
	guest_bootargs_end_gpa = vm->sw.bootargs_info.load_addr + vm->sw.bootargs_info.size;

	/** Set new_guest_gdt_base_gpa to max of kernel end GPA and boot arguments end GPA */
	new_guest_gdt_base_gpa = max(guest_kernel_end_gpa, guest_bootargs_end_gpa);
	/** Set new_guest_gdt_base_gpa to (itself + 7) & ~7, so it will be 8 bytes aligned */
	new_guest_gdt_base_gpa = (new_guest_gdt_base_gpa + 7UL) & ~(8UL - 1UL);

	/** Return the calculated GPA as GDT base GPA */
	return new_guest_gdt_base_gpa;
}

/**
 * @brief Copy the e820 table from the VM to a zeropage.
 *
 * This function is called to copy the e820 table from the VM to a zeropage, which is used when guest OS boots.
 *
 * @param[inout] zp Pointer to a zeropage data structure.
 * @param[in] vm Pointer to a VM data structure whose e820 table is copied to.
 *
 * @return The number of entries in the zeropage's e820 table.
 *
 * @pre zp != NULL
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by create_zero_page.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static uint32_t create_zeropage_e820(struct zero_page *zp, const struct acrn_vm *vm)
{
	/** Declare the following local variables of type uint32_t.
	 *  - entry_num representing the number of the VM's e820 entries, initialized as vm->e820_entry_num.
	 */
	uint32_t entry_num = vm->e820_entry_num;
	/** Declare the following local variables of type 'struct e820_entry *'.
	 *  - zp_e820 representing the pointer to the zeropage's e820 table, initialized as zp->entries.
	 */
	struct e820_entry *zp_e820 = zp->entries;
	/** Declare the following local variables of type 'const struct e820_entry *'.
	 *  - vm_e820 representing the pointer to the VM's e820 table, initialized as vm->e820_entries.
	 */
	const struct e820_entry *vm_e820 = vm->e820_entries;

	/** If either vm_e820 is NULL, or entry_num is 0, or entry_num is larger than E820_MAX_ENTRIES */
	if ((vm_e820 == NULL) || (entry_num == 0U) || (entry_num > E820_MAX_ENTRIES)) {
		/** Logging the following information with a log level of LOG_ERROR.
		 *  - "e820 create error"
		 */
		pr_err("e820 create error");
		/** Set entry_num to 0 */
		entry_num = 0U;
	} else {
		/** Call memcpy_s with the following parameters, in order to copy the e820 table from VM to the
		 *  zeropage, and discard its return value.
		 *  - zp_e820
		 *  - entry_num * sizeof(struct e820_entry)
		 *  - vm_e820
		 *  - entry_num * sizeof(struct e820_entry)
		 */
		(void)memcpy_s((void *)zp_e820, entry_num * sizeof(struct e820_entry), (void *)vm_e820,
			entry_num * sizeof(struct e820_entry));
	}
	/** Return number of the copied e820 table entries */
	return entry_num;
}

/**
 * @brief Create a zeropage to store the VM's guest OS boot information.
 *
 * This function is called to create a zeropage to store the VM's guest OS boot information, which is used
 * when guest OS boots. It mainly includes boot arguments load address/size and e820 table.
 *
 * @param[in] vm Pointer to a structure representing the VM which the zeropage is created for
 *
 * @return The created zeropage's GPA
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by prepare_loading_bzimage.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static uint64_t create_zero_page(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct zero_page *'.
	 *  - zeropage representing a pointer to zeropage data structure for the specified VM, not initialized.
	 */
	struct zero_page *zeropage;
	/** Declare the following local variables of type 'struct sw_kernel_info *'.
	 *  - sw_kernel representing a pointer to the VM's kernel info, initialized as &(vm->sw.kernel_info).
	 */
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	/** Declare the following local variables of type 'struct sw_module_info *'.
	 *  - bootargs_info representing a pointer to the guest OS boot arguments, initialized as
	 *  &(vm->sw.bootargs_info).
	 */
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	/** Declare the following local variables of type 'struct zero_page *'.
	 *  - hva representing a HVA of the zeropage in kernel image, not initialized.
	 */
	struct zero_page *hva;
	/** Declare the following local variables of type uint64_t.
	 *  - gpa representing the GPA of the zeropage that the hypervisor prepares for the VM, not initialized.
	 *  - addr representing a variable for load address of boot arguments, not initialized.
	 */
	uint64_t gpa, addr;

	/** Set gpa to bootargs_info->load_addr + MEM_4K; used to set zeropage in guest RAM region after boot args */
	gpa = bootargs_info->load_addr + MEM_4K;
	/** Set zeropage to gpa2hva(vm, gpa); it is the corresponding HVA of the zeropage */
	zeropage = (struct zero_page *)gpa2hva(vm, gpa);

	/** Call stac in order to allow hypervisor to access guest's memory space protected by SMAP. */
	stac();
	/** Call memset with the following parameters, in order to clear the zeropage, and discard its return value.
	 *  - zeropage
	 *  - 0
	 *  - MEM_2K
	 */
	(void)memset(zeropage, 0U, MEM_2K);

	/** Set hva to gpa2hva(vm, sw_kernel->kernel_load_addr); so it points to the zeropage of the guest image) */
	hva = (struct zero_page *)gpa2hva(vm, sw_kernel->kernel_load_addr);
	/** Call memcpy_s with the following parameters, in order to copy the guest image's zeropage header, and discard
	 *  its return value.
	 *  - &(zeropage->hdr)
	 *  - sizeof(zeropage->hdr)
	 *  - &(hva->hdr)
	 *  - sizeof(hva->hdr)
	 */
	(void)memcpy_s(&(zeropage->hdr), sizeof(zeropage->hdr), &(hva->hdr), sizeof(hva->hdr));

	/** Set addr to bootargs_info->load_addr, the GPA where the boot arguments shall be placed */
	addr = bootargs_info->load_addr;
	/** Set zeropage->hdr.bootargs_addr to addr; in order to copy boot arguments in zeropage header structure */
	zeropage->hdr.bootargs_addr = (uint32_t)addr;

	/** Set zeropage->hdr.version to 0x20c, to indicate that the boot protocol version is 2.12 */
	zeropage->hdr.version = 0x20cU;
	/** Set zeropage->hdr.loader_type to 0xff */
	zeropage->hdr.loader_type = 0xffU;
	/** Set zeropage->hdr.load_flags to 0x20, in order to not output early boot message */
	zeropage->hdr.load_flags = (1U << 5U);

	/** Set zeropage->e820_nentries to the return value of create_zeropage_e820, which copies the e820 table
	 *  from vm to zeropage
	 */
	zeropage->e820_nentries = (uint8_t)create_zeropage_e820(zeropage, vm);
	/** Call clac in order to forbid hypervisor to access guest's memory space protected by SMAP */
	clac();

	/** Return gpa, the GPA of zeropage */
	return gpa;
}

/**
 * @brief Prepare to load a kernel which is bzImage format.
 *
 * This function is called to prepare the work to load bzImage. It will set the kernel entry address, clear all general
 * purpose registers, and call other internal function to create the zeropage, and set the virtual BP RSI to the GPA
 * of the zeropage.
 *
 * @param[inout] vm Pointer to the structure representing the VM with a kernel in bzImage format
 * @param[inout] vcpu Pointer to the VM's BP.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by direct_boot_sw_loader.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static void prepare_loading_bzimage(struct acrn_vm *vm, struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - i representing a loop counter, not initialized.
	 */
	uint32_t i;
	/** Declare the following local variables of type uint32_t.
	 *  - kernel_entry_offset representing the offset to the kernel entry against the base of the kernel image,
	 *  not initialized.
	 */
	uint32_t kernel_entry_offset;
	/** Declare the following local variables of type 'struct zero_page *'.
	 *  - zeropage representing the zeropage of the kernel image used by the given VM, not initialized.
	 */
	struct zero_page *zeropage;
	/** Declare the following local variables of type 'struct sw_kernel_info *'.
	 *  - sw_kernel representing a pointor to the VM's kernel info, initialized as &(vm->sw.kernel_info).
	 */
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);

	/** Set zeropage to sw_kernel->kernel_src_addr, which is the HVA of the original kernel image of the given VM */
	zeropage = (struct zero_page *)sw_kernel->kernel_src_addr;
	/** Call stac in order to allow hypervisor to access guest's memory space protected by SMAP. */
	stac();
	/** Set kernel_entry_offset to (zeropage->hdr.setup_sects + 1) * 512; the entry of a bzImage format kernel lies
	 *  right after the boot sector and setup sectors each of which consists of 512 bytes.
	 */
	kernel_entry_offset = (uint32_t)(zeropage->hdr.setup_sects + 1U) * 512U;
	/** Call clac in order to forbid hypervisor to access guest's memory space protected by SMAP */
	clac();
	/** Set sw_kernel->kernel_entry_addr to sw_kernel->kernel_load_addr + kernel_entry_offset, the entry to run */
	sw_kernel->kernel_entry_addr = sw_kernel->kernel_load_addr + kernel_entry_offset;

	/** For each i ranging from 0 to NUM_GPRS - 1 [with a step of 1] */
	for (i = 0U; i < NUM_GPRS; i++) {
		/** Call vcpu_set_gpreg with the following parameters, in order to clear each general purpose register.
		 *  - vcpu
		 *  - i
		 *  - 0
		 */
		vcpu_set_gpreg(vcpu, i, 0UL);
	}

	/** Call vcpu_set_gpreg with the following parameters, in order to set RSI to the GPA of the created zeropage.
	 *  - vcpu
	 *  - CPU_REG_RSI
	 *  - create_zero_page(vm)
	 */
	vcpu_set_gpreg(vcpu, CPU_REG_RSI, create_zero_page(vm));
	/** Logging the following information with a log level of LOG_INFO.
	 *  - __func__
	 *  - vm->vm_id
	 *  - vcpu_get_gpreg(vcpu, CPU_REG_RSI)
	 */
	pr_info("%s, RSI pointing to zero page for VM %d at GPA %X", __func__, vm->vm_id,
		vcpu_get_gpreg(vcpu, CPU_REG_RSI));
}

/**
 * @brief Prepare to load a raw image
 *
 * This function is called to prepare the work to load a raw image for guest VM. It gets the kernel entry address
 * from the VM configuration. The entry address is where the BP starts execution.
 *
 * @param[inout] vm Pointer to the VM data structure which contains the kernel info.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is an internal function called by direct_boot_sw_loader.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Unspecified
 */
static void prepare_loading_rawimage(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct sw_kernel_info *'.
	 *  - sw_kernel representing a pointer to the given VM's kernel info, initialized as &(vm->sw.kernel_info).
	 */
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	/** Declare the following local variables of type 'const struct acrn_vm_config *'.
	 *  - vm_config representing a pointer to the configuration data of the given VM, initialized as the return
	 *  value of get_vm_config(vm->vm_id).
	 */
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/** Set sw_kernel->kernel_entry_addr to vm_config->os_config.kernel_entry_addr */
	sw_kernel->kernel_entry_addr = vm_config->os_config.kernel_entry_addr;
}

/**
 * @brief Do the work of a bootloader to make the guest OS ready to run
 *
 * It is called to load the guest OS image to its target loading address, and set the virtual BP RIP to the entry
 * of its kernel image. If the guest OS is Linux, it also prepares the zeropage, which is used by kernel.
 *
 * @param[inout] vm Pointer to the VM which is to be configured to run.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark It is called by prepare_vm.
 *
 * @reentrancy Unspecified
 *
 * @threadsafety true
 */
void direct_boot_sw_loader(struct acrn_vm *vm)
{
	/** Declare the following local variables of type 'struct sw_kernel_info *'.
	 *  - sw_kernel representing a pointer to the VM's kernel info, initialized as &(vm->sw.kernel_info).
	 */
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	/** Declare the following local variables of type 'struct sw_module_info *'.
	 *  - bootargs_info representing a pointer to the VM's boot arguments info, initialized as
	 *  &(vm->sw.bootargs_info).
	 */
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	/** Declare the following local variables of type 'struct acrn_vcpu *'.
	 *  - vcpu representing a pointer to the virtual BP (boot vCPU) of the given VM, initialized as
	 *  vcpu_from_vid(vm, BOOT_CPU_ID).
	 */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Loading guest to run-time location"
	 */
	pr_dbg("Loading guest to run-time location");

	/** Call init_vcpu_protect_mode_regs with the following parameters, in order to initialize the BP registers
	 *  and GDT as protected mode. The GDT is got by calling get_guest_gdt_base_gpa.
	 *  - vcpu
	 *  - get_guest_gdt_base_gpa(vcpu->vm)
	 */
	init_vcpu_protect_mode_regs(vcpu, get_guest_gdt_base_gpa(vcpu->vm));

	/** Call copy_to_gpa with the following parameters, in order to copy the guest kernel image to its run-time
	 *  location.
	 *  - vm
	 *  - sw_kernel->kernel_src_addr
	 *  - sw_kernel->kernel_load_addr
	 *  - sw_kernel->kernel_size
	 */
	copy_to_gpa(vm, sw_kernel->kernel_src_addr, sw_kernel->kernel_load_addr, sw_kernel->kernel_size);

	/** If bootargs_info->size is not 0, which means the guest OS contains boot arguments info */
	if (bootargs_info->size != 0U) {
		/** Call copy_to_gpa with the following parameters, in order to copy guest OS boot arguments to its
		 *  loading location.
		 *  - vm
		 *  - bootargs_info->src_addr
		 *  - bootargs_info->load_addr
		 *  - strnlen_s((char *)bootargs_info->src_addr, MAX_BOOTARGS_SIZE) + 1
		 */
		copy_to_gpa(vm, bootargs_info->src_addr, bootargs_info->load_addr,
			(strnlen_s((char *)bootargs_info->src_addr, MAX_BOOTARGS_SIZE) + 1U));
	}
	/** Depending on vm->sw.kernel_type */
	switch (vm->sw.kernel_type) {
	/** The type of Linux kernel is KERNEL_BZIMAGE */
	case KERNEL_BZIMAGE:
		/** Call prepare_loading_bzimage with the following parameters, in order to do the configure work to
		 *  run the VM which kernel's format is bzImage.
		 *  - vm
		 *  - vcpu
		 */
		prepare_loading_bzimage(vm, vcpu);
		/** Terminate the loop */
		break;
	/** The type of zephyr kernel is KERNEL_ZEPHYR */
	case KERNEL_ZEPHYR:
		/** Call prepare_loading_rawimage with the following parameters, in order to do the configure work to
		 *  run the VM which kernel format is raw image.
		 *  - vm
		 */
		prepare_loading_rawimage(vm);
		/** Terminate the loop */
		break;
	/** Otherwise */
	default:
		/** Logging the following information with a log level of LOG_ERROR.
		 *  - __func__
		 */
		pr_err("%s, Loading VM SW failed", __func__);
		/** Terminate the loop */
		break;
	}

	/** Call vcpu_set_rip with the following parameters, in order to set the BP's RIP to kernel entry point.
	 *  - vm
	 *  - sw_kernel->kernel_entry_addr
	 */
	vcpu_set_rip(vcpu, sw_kernel->kernel_entry_addr);
	/** Logging the following information with a log level of LOG_INFO.
	 *  - __func__
	 *  - vm->vm_id
	 *  - vcpu->vcpu_id
	 *  - sw_kernel->kernel_entry_addr
	 */
	pr_info("%s, VM %hu VCPU %hu Entry: 0x%016lx ", __func__, vm->vm_id, vcpu->vcpu_id,
		sw_kernel->kernel_entry_addr);
}

/**
 * @}
 */
