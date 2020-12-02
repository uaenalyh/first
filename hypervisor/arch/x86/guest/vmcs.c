/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains vmcs operations which is vcpu related
 */

#include <types.h>
#include <vmcs.h>
#include <vcpu.h>
#include <vmsr.h>
#include <vm.h>
#include <vmx.h>
#include <gdt.h>
#include <pgtable.h>
#include <per_cpu.h>
#include <cpu_caps.h>
#include <cpufeatures.h>
#include <vmexit.h>
#include <logmsg.h>

/**
 * @addtogroup vp-base_hv-main
 *
 * @{
 */

/**
 * @file
 * @brief This file describes the VMCS initialization relevant operations.
 *
 * These operations implements all these initializations for guest-state fields, host-state fields and control fields
 * of the VMCS.
 *
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * Helper functions include: check_vmx_ctrl.
 *
 * Decomposed functions include: init_guest_vmx, init_guest_state, init_host_state, init_exec_ctrl,
 * init_entry_ctrl, init_exit_ctrl, init_vmcs, load_vmcs and switch_apicv_mode_x2apic.
 */

/**
 * @brief This function is used to initialize the 16-bit, 32-bit, 64-bit and natural-width guest state fields in VMCS.
 *
 * @param[inout] vcpu A pointer which points to a virtual CPU data structure associated
 * with the VMCS to be initialized.
 * @param[in] cr0 The value of guest CR0 to be set to the vCPU.
 * @param[in] cr3 The value of guest CR3 to be set to the VMCS.
 * @param[in] cr4 The value of guest CR4 to be set to the vCPU.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_guest_vmx(struct acrn_vcpu *vcpu, uint64_t cr0, uint64_t cr3, uint64_t cr4)
{
	/** Declare the following local variables of type struct guest_cpu_context *.
	 *  - ctx representing a pointer which points to the guest_cpu_context structure
	 *    associated with \a vcpu, initialized as &vcpu->arch.context.
	 */
	struct guest_cpu_context *ctx = &vcpu->arch.context;
	/** Declare the following local variables of type struct ext_context *.
	 *  - ectx representing the current ext_context structure, initialized as &ctx->ext_ctx. */
	struct ext_context *ectx = &ctx->ext_ctx;
	/** Declare the following local variables of type uint64_t.
	 *  - v representing the value of guest MSR IA32_MISC_ENABLE. */
	uint64_t v;
	/** Call vcpu_set_cr4() with the following parameters, in order to set \a cr4 to virtual CR4 of the \a vcpu.
	 *  - vcpu
	 *  - cr4 */
	vcpu_set_cr4(vcpu, cr4);
	/** Call vcpu_set_cr0() with the following parameters, in order to set \a cr0 to virtual CR0 of the \a vcpu.
	 *  - vcpu
	 *  - cr0 */
	vcpu_set_cr0(vcpu, cr0);
	/** Call exec_vmwrite() with the following parameters, in order to write \a cr3 into the field
	 *  'GUEST CR3' in current VMCS.
	 *  - VMX_GUEST_CR3
	 *  - cr3 */
	exec_vmwrite(VMX_GUEST_CR3, cr3);
	/** Call exec_vmwrite() with the following parameters, in order to write ectx->gdtr.base to
	 *  the field 'Guest GDTR base' in current VMCS.
	 *  - VMX_GUEST_GDTR_BASE
	 *  - ectx->gdtr.base */
	exec_vmwrite(VMX_GUEST_GDTR_BASE, ectx->gdtr.base);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - ectx->gdtr.base */
	pr_dbg("VMX_GUEST_GDTR_BASE: 0x%016lx", ectx->gdtr.base);
	/** Call exec_vmwrite32() with the following parameters, in order to write ectx->gdtr.limit
	 *  to the field 'Guest GDTR limit' in current VMCS.
	 *  - VMX_GUEST_GDTR_LIMIT
	 *  - ectx->gdtr.limit */
	exec_vmwrite32(VMX_GUEST_GDTR_LIMIT, ectx->gdtr.limit);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - ectx->gdtr.limit */
	pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%016lx", ectx->gdtr.limit);
	/** Call exec_vmwrite() with the following parameters, in order to write ectx->idtr.base to
	 *  the field 'Guest IDTR base' in current VMCS.
	 *  - VMX_GUEST_IDTR_BASE
	 *  - ectx->idtr.base */
	exec_vmwrite(VMX_GUEST_IDTR_BASE, ectx->idtr.base);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_GUEST_IDTR_BASE: 0x%016lx"
	 *  - ectx->idtr.base */
	pr_dbg("VMX_GUEST_IDTR_BASE: 0x%016lx", ectx->idtr.base);
	/** Call exec_vmwrite32() with the following parameters, in order to write ectx->idtr.limit
	 *  to the field 'Guest IDTR limit' in current VMCS.
	 *  - VMX_GUEST_IDTR_LIMIT
	 *  - ectx->idtr.limit */
	exec_vmwrite32(VMX_GUEST_IDTR_LIMIT, ectx->idtr.limit);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_GUEST_IDTR_LIMIT: 0x%016lx"
	 *  - ectx->idtr.limit */
	pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%016lx", ectx->idtr.limit);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest CS register.
	 *  - ectx->cs
	 *  - VMX_GUEST_CS */
	load_segment(ectx->cs, VMX_GUEST_CS);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest SS register.
	 *  - ectx->ss
	 *  - VMX_GUEST_SS */
	load_segment(ectx->ss, VMX_GUEST_SS);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest DS register.
	 *  - ectx->ds
	 *  - VMX_GUEST_DS */
	load_segment(ectx->ds, VMX_GUEST_DS);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest ES register.
	 *  - ectx->es
	 *  - VMX_GUEST_ES */
	load_segment(ectx->es, VMX_GUEST_ES);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest FS register.
	 *  - ectx->fs
	 *  - VMX_GUEST_FS */
	load_segment(ectx->fs, VMX_GUEST_FS);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest GS register.
	 *  - ectx->gs
	 *  - VMX_GUEST_GS */
	load_segment(ectx->gs, VMX_GUEST_GS);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest TR register.
	 *  - ectx->tr
	 *  - VMX_GUEST_TR */
	load_segment(ectx->tr, VMX_GUEST_TR);
	/** Call load_segment() with the following parameters, in order to set selector,
	 *  base, limit and attribute of guest LDTR register.
	 *  - ectx->ldtr
	 *  - VMX_GUEST_LDTR */
	load_segment(ectx->ldtr, VMX_GUEST_LDTR);
	/** Call msr_read() with the followin parameters, in order to get the native processor
	 *  information of MSR IA32_MISC_ENABLE.
	 *  - MSR_IA32_MISC_ENABLE
	 * */
	v = msr_read(MSR_IA32_MISC_ENABLE);
	/** Bitwise AND v by ~~(MSR_IA32_MISC_ENABLE_MONITOR_ENA | MSR_IA32_MISC_ENABLE_PMA). */
	v &= ~(MSR_IA32_MISC_ENABLE_MONITOR_ENA | MSR_IA32_MISC_ENABLE_PMA);
	/** Bitwise OR v by MSR_IA32_MISC_BTS_UNAVILABLE | MSR_IA32_MISC_PEBS_UNAVILABLE. */
	v |= MSR_IA32_MISC_BTS_UNAVILABLE | MSR_IA32_MISC_PEBS_UNAVILABLE;
	/** Call vcpu_set_guest_msr with the following parameters, in order to write
	 *  v into the MSR IA32_MISC_ENABLE associated with \a vcpu.
	 *  - vcpu
	 *  - MSR_IA32_MISC_ENABLE
	 *  - v */
	vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, v);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the
	 *  field 'Guest IA32_SYSENTER_CS' in current VMCS.
	 *  - VMX_GUEST_IA32_SYSENTER_CS
	 *  - 0 */
	exec_vmwrite32(VMX_GUEST_IA32_SYSENTER_CS, 0U);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the
	 *  field 'Guest IA32_SYSENTER_ESP' in current VMCS.
	 *  - VMX_GUEST_IA32_SYSENTER_ESP
	 *  - 0 */
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the
	 *  field 'Guest IA32_SYSENTER_EIP' in current VMCS.
	 *  - VMX_GUEST_IA32_SYSENTER_EIP
	 *  - 0UL */
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the
	 *  field 'Guest pending debug exceptions' in current VMCS.
	 *  - VMX_GUEST_PENDING_DEBUG_EXCEPT
	 *  - 0UL */
	exec_vmwrite(VMX_GUEST_PENDING_DEBUG_EXCEPT, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the
	 *  field 'Guest IA32_DEBUGCTL' in current VMCS.
	 *  - VMX_GUEST_IA32_DEBUGCTL_FULL
	 *  - 0UL */
	exec_vmwrite(VMX_GUEST_IA32_DEBUGCTL_FULL, 0UL);
	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the
	 *  field 'Guest interruptibility state' in current VMCS.
	 *  - VMX_GUEST_INTERRUPTIBILITY_INFO
	 *  - 0UL */
	exec_vmwrite32(VMX_GUEST_INTERRUPTIBILITY_INFO, 0U);
	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the
	 *  field 'Guest activity state' in current VMCS.
	 *  - VMX_GUEST_ACTIVITY_STATE
	 *  - 0U */
	exec_vmwrite32(VMX_GUEST_ACTIVITY_STATE, 0U);
	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the
	 *  field 'Guest SMBASE' in current VMCS.
	 *  - VMX_GUEST_SMBASE
	 *  - 0 */
	exec_vmwrite32(VMX_GUEST_SMBASE, 0U);
	/** If vcpu->arch.vcpu_powerup is false. */
	if (vcpu->arch.vcpu_powerup == false) {
		/** Call vcpu_set_guest_msr with the following parameters, in order to write PAT_POWER_ON_VALUE into
		 *  the MSR IA32_PAT associated with \a vcpu.
		 *  - vcpu
		 *  - MSR_IA32_PAT
		 *  - PAT_POWER_ON_VALUE */
		vcpu_set_guest_msr(vcpu, MSR_IA32_PAT, PAT_POWER_ON_VALUE);
		/** Call exec_vmwrite() with the following parameters, in order to write PAT_POWER_ON_VALUE
		 *  to the field 'Guest IA32_PAT' in current VMCS.
		 *  - VMX_GUEST_IA32_PAT_FULL
		 *  - PAT_POWER_ON_VALUE */
		exec_vmwrite(VMX_GUEST_IA32_PAT_FULL, PAT_POWER_ON_VALUE);
	}
	/** Call exec_vmwrite() with the following parameters, in order to write DR7_INIT_VALUE to
	 *  the field 'Guest DR7' in current VMCS.
	 *  - VMX_GUEST_DR7
	 *  - DR7_INIT_VALUE */
	exec_vmwrite(VMX_GUEST_DR7, DR7_INIT_VALUE);

}

/**
 * @brief This function is used to initialize guest state of the VMCS.
 *
 * @param[inout] vcpu A pointer which points to a virtual CPU data structure associated
 * with the VMCS to be initialized.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_guest_state(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type struct guest_cpu_context *.
	 *  - ctx representing a pointer which points to the guest_cpu_context structure
	 *    associated with \a vcpu, initialized as &vcpu->arch.context.
	 */
	struct guest_cpu_context *ctx = &vcpu->arch.context;
	/** Call init_guest_vmx() with the following parameters, in order to initialize the guest state fields in VMCS.
	  *  - vcpu
	  *  - ctx->run_ctx.cr0
	  *  - ctx->ext_ctx.cr3
	  *  - ctx->run_ctx.cr4 & ~(CR4_VMXE | CR4_SMXE | CR4_MCE) */
	init_guest_vmx(vcpu, ctx->run_ctx.cr0, ctx->ext_ctx.cr3, ctx->run_ctx.cr4 & ~(CR4_VMXE | CR4_SMXE | CR4_MCE));
}

/**
 * @brief This function is used to initialize the VMCS host state.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static void init_host_state(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - value16 representing a 16 bit temporary variable to store the value read from the physical registers. */
	uint16_t value16;
	/** Declare the following local variables of type uint64_t.
	 *  - value64 representing a 64 bit temporary variable to store the value read from the physical registers. */
	uint64_t value64;
	/** Declare the following local variables of type uint64_t.
	 *  - value representing a 64 bit temporary variable to store the value read from the physical registers. */
	uint64_t value;
	/** Declare the following local variables of type uint64_t.
	 *  - tss_addr representing address of TSS. */
	uint64_t tss_addr;
	/** Declare the following local variables of type uint64_t.
	 *  - gdt_base representing base address of GDT. */
	uint64_t gdt_base;
	/** Declare the following local variables of type uint64_t.
	 *  - idt_base representing base address of IDT. */
	uint64_t idt_base;
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  "*********************" */
	pr_dbg("*********************");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Initialize host state" */
	pr_dbg("Initialize host state");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*********************" */
	pr_dbg("*********************");

	/***************************************************
	 * Move the current ES, CS, SS, DS, FS, GS, TR, LDTR * values to the
	 * corresponding 16-bit host * segment selection (ES, CS, SS, DS, FS,
	 * GS), * Task Register (TR), * Local Descriptor Table Register (LDTR)
	 ***************************************************/

	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical ES
	 *  and store the value into value16.
	 *  - es
	 *  - &value16 */
	CPU_SEG_READ(es, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host ES selector' in current VMCS.
	 *  - VMX_HOST_ES_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_ES_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_ES_SEL: 0x%hx "
	 *  - value16 */
	pr_dbg("VMX_HOST_ES_SEL: 0x%hx ", value16);
	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical CS
	 *  and store the value into value16.
	 *  - cs
	 *  - &value16 */
	CPU_SEG_READ(cs, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host CS selector' in current VMCS.
	 *  - VMX_HOST_CS_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_CS_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_CS_SEL: 0x%hx "
	 *  - value16 */
	pr_dbg("VMX_HOST_CS_SEL: 0x%hx ", value16);
	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical SS
	 *  and store the value into value16.
	 *  - ss
	 *  - &value16 */
	CPU_SEG_READ(ss, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host SS selector' in current VMCS.
	 *  - VMX_HOST_SS_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_SS_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_SS_SEL: 0x%hx "
	 *  - value16 */
	pr_dbg("VMX_HOST_SS_SEL: 0x%hx ", value16);
	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical DS
	 *  and store the value into value16.
	 *  - ds
	 *  - &value16 */
	CPU_SEG_READ(ds, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host DS selector' in current VMCS.
	 *  - VMX_HOST_DS_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_DS_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_DS_SEL: 0x%hx "
	 *  - value16 */
	pr_dbg("VMX_HOST_DS_SEL: 0x%hx ", value16);
	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical FS
	 *  and store the value into value16.
	 *  - fs
	 *  - &value16 */
	CPU_SEG_READ(fs, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host FS selector' in current VMCS.
	 *  - VMX_HOST_FS_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_FS_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_FS_SEL: 0x%hx "
	 *  - value16 */
	pr_dbg("VMX_HOST_FS_SEL: 0x%hx ", value16);
	/** Call CPU_SEG_READ with the following parameters, in order to read the current physical GS
	 *  and store the value into value16.
	 *  - gs
	 *  - &value16 */
	CPU_SEG_READ(gs, &value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write value16 to the field
	 *  'Host GS selector' in current VMCS.
	 *  - VMX_HOST_GS_SEL
	 *  - value16 */
	exec_vmwrite16(VMX_HOST_GS_SEL, value16);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_GS_SEL: 0x%hx "
	 *  - value16  */
	pr_dbg("VMX_HOST_GS_SEL: 0x%hx ", value16);
	/** Call exec_vmwrite16() with the following parameters, in order to write
	 *  HOST_GDT_RING0_CPU_TSS_SEL to the field 'Host TR selector' in current VMCS.
	 *  - VMX_HOST_TR_SEL
	 *  - HOST_GDT_RING0_CPU_TSS_SEL  */
	exec_vmwrite16(VMX_HOST_TR_SEL, HOST_GDT_RING0_CPU_TSS_SEL);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_TR_SEL: 0x%hx "
	 *  - HOST_GDT_RING0_CPU_TSS_SEL */
	pr_dbg("VMX_HOST_TR_SEL: 0x%hx ", HOST_GDT_RING0_CPU_TSS_SEL);

	/******************************************************
	 * 32-bit fields
	 ******************************************************/

	/** Set gdt_base to the return value of sgdt() */
	gdt_base = sgdt();

	/** If bit 47 of 'gdt_base' is set. */
	if (((gdt_base >> 47U) & 0x1UL) != 0UL) {
		/** Bitwise OR gdt_base by ffff000000000000H */
		gdt_base |= 0xffff000000000000UL;
	}
	/** Call exec_vmwrite() with the following parameters, in order to write gdt_base to the field
	 *  'Host GDTR base' in current VMCS.
	 *  - VMX_HOST_GDTR_BASE
	 *  - gdt_base */
	exec_vmwrite(VMX_HOST_GDTR_BASE, gdt_base);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_GDTR_BASE: 0x%x "
	 *  - gdt_base */
	pr_dbg("VMX_HOST_GDTR_BASE: 0x%x ", gdt_base);

	/** Set tss_address to return value of hva2hpa((void *)&get_cpu_var(tss)) */
	tss_addr = hva2hpa((void *)&get_cpu_var(tss));
	/** Call exec_vmwrite() with the following parameters, in order to write tss_addr to the field
	 *  'Host TR base' in current VMCS.
	 *  - VMX_HOST_TR_BASE
	 *  - tss_addr */
	exec_vmwrite(VMX_HOST_TR_BASE, tss_addr);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_TR_BASE: 0x%016lx "
	 *  - tss_addr */
	pr_dbg("VMX_HOST_TR_BASE: 0x%016lx ", tss_addr);
	/* Obtain the current interrupt descriptor table base */
	/** Set idt_base to return value of sidt() */
	idt_base = sidt();
	/** If bit 47 of idt_base is 1H */
	if (((idt_base >> 47U) & 0x1UL) != 0UL) {
		/** Bitwise OR idt_base by 0xffff000000000000UL */
		idt_base |= 0xffff000000000000UL;
	}
	/** Call exec_vmwrite() with the following parameters, in order to write idt_base to the field
	 *  'Host IDTR base' in current VMCS.
	 *  - VMX_HOST_IDTR_BASE
	 *  - idt_base */
	exec_vmwrite(VMX_HOST_IDTR_BASE, idt_base);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_IDTR_BASE: 0x%x "
	 *  - idt_base */
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idt_base);

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "64-bit********" */
	pr_dbg("64-bit********");

	/** Set "value64" to return value of msr_read(MSR_IA32_PAT) */
	value64 = msr_read(MSR_IA32_PAT);
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'Host IA32_PAT ' in current VMCS.
	 *  - VMX_HOST_IA32_PAT_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_HOST_IA32_PAT_FULL, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_IA32_PAT: 0x%016lx "
	 *  - value64 */
	pr_dbg("VMX_HOST_IA32_PAT: 0x%016lx ", value64);

	/** Set "value64" to return value of msr_read(MSR_IA32_EFER) */
	value64 = msr_read(MSR_IA32_EFER);
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'Host IA32_EFER ' in current VMCS.
	 *  - VMX_HOST_IA32_EFER_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_HOST_IA32_EFER_FULL, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_IA32_EFER: 0x%016lx "
	 *  - value64 */
	pr_dbg("VMX_HOST_IA32_EFER: 0x%016lx ", value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Natural-width********" */
	pr_dbg("Natural-width********");
	 /** Call CPU_CR_READ() with the following parameters, in order to save physical CR0 to 'value'.
	 *  - cr0
	 *  - &value */
	CPU_CR_READ(cr0, &value);
	/** Call exec_vmwrite() with the following parameters, in order to write value to the field
	 *  'Host CR0 ' in current VMCS.
	 *  - VMX_HOST_CR0
	 *  - value */
	exec_vmwrite(VMX_HOST_CR0, value);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_CR0: 0x%016lx "
	 *  - value: CR0 value */
	pr_dbg("VMX_HOST_CR0: 0x%016lx ", value);

	 /** Call CPU_CR_READ() with the following parameters, in order to save physical CR3 to 'value'.
	 *  - cr3
	 *  - &value */
	CPU_CR_READ(cr3, &value);
	/** Call exec_vmwrite() with the following parameters, in order to write value to the field
	 *  'Host CR3 ' in current VMCS.
	 *  - VMX_HOST_CR3
	 *  - value */
	exec_vmwrite(VMX_HOST_CR3, value);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_CR3: 0x%016lx "
	 *  - value */
	pr_dbg("VMX_HOST_CR3: 0x%016lx ", value);

	/* Set up host CR4 field */
	 /** Call CPU_CR_READ() with the following parameters, in order to save physical CR4 to 'value'.
	 *  - cr4
	 *  - &value */
	CPU_CR_READ(cr4, &value);
	/** Call exec_vmwrite() with the following parameters, in order to write value to the field
	 *  'Host CR4 ' in current VMCS.
	 *  - VMX_HOST_CR4
	 *  - value */
	exec_vmwrite(VMX_HOST_CR4, value);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - value: CR4 value */
	pr_dbg("VMX_HOST_CR4: 0x%016lx ", value);

	/* Set up host and guest FS base address */
	/** Set "value" to return value of msr_read(MSR_IA32_FS_BASE) */
	value = msr_read(MSR_IA32_FS_BASE);
	/** Call exec_vmwrite() with the following parameters, in order to write value to the field
	 *  'Host FS base' in current VMCS.
	 *  - VMX_HOST_FS_BASE
	 *  - value */
	exec_vmwrite(VMX_HOST_FS_BASE, value);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_FS_BASE: 0x%016lx "
	 *  - value */
	pr_dbg("VMX_HOST_FS_BASE: 0x%016lx ", value);
	/** Set "value" to return value of msr_read(MSR_IA32_GS_BASE) */
	value = msr_read(MSR_IA32_GS_BASE);
	/** Call exec_vmwrite() with the following parameters, in order to write value to the field
	 *  'Host GS base' in current VMCS.
	 *  - VMX_HOST_GS_BASE
	 *  - value */
	exec_vmwrite(VMX_HOST_GS_BASE, value);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_HOST_GS_BASE: 0x%016lx "
	 *  - value */
	pr_dbg("VMX_HOST_GS_BASE: 0x%016lx ", value);
	/** Set "value64" to (uint64_t)&vm_exit */
	value64 = (uint64_t)&vm_exit;
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "HOST RIP on VMExit %016lx "
	 *  - value64 */
	pr_dbg("HOST RIP on VMExit %016lx ", value64);
	/** Call exec_vmwrite() with the following parameters, in order to write value64 to the field
	 *  'Host RIP' in current VMCS.
	 *  - VMX_HOST_RIP
	 *  - value64 */
	exec_vmwrite(VMX_HOST_RIP, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "HOST RIP on VMExit %016lx "
	 *  - value64 */
	pr_dbg("vm exit return address = %016lx ", value64);
	/** Call exec_vmwrite32 with the following parameters, in order to write 0 to the field
	 *  'Host IA32_SYSENTER_CS' in current VMCS.
	 *  - VMX_HOST_IA32_SYSENTER_CS
	 *  - 0 */
	exec_vmwrite32(VMX_HOST_IA32_SYSENTER_CS, 0U);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the field
	 *  'Host IA32_SYSENTER_ESP' in current VMCS.
	 *  - VMX_HOST_IA32_SYSENTER_ESP
	 *  - 0 */
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_ESP, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0 to the field
	 *  'Host IA32_SYSENTER_EIP' in current VMCS.
	 *  - VMX_HOST_IA32_SYSENTER_EIP
	 *  - 0 */
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_EIP, 0UL);
}

/**
 * @brief This function is used to check the capability of these setting bits and return a valid
 *        value that can be set to the VMX control field.
 *
 * @param[in] msr the address of the target MSR
 * @param[in] ctrl_req the expected value of the corresponding control fields
 *
 * @return the checked value which will be finally set to the control fields
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static uint32_t check_vmx_ctrl(uint32_t msr, uint32_t ctrl_req)
{
	/** Declare the following local variables of type uint64_t.
	 *  - vmx_msr representing the value of MSR. */
	uint64_t vmx_msr;
	/** Declare the following local variables of type uint32_t.
	 *  - vmx_msr_low representing low 32 bits of the MSR value.
	 *  - vmx_msr_high representing high 32 bits of the MSR value */
	uint32_t vmx_msr_low, vmx_msr_high;
	/** Declare the following local variables of type uint32_t.
	 *  - ctrl representing the value of control field, initialized as \a ctrl_req */
	uint32_t ctrl = ctrl_req;
	/** Set vmx_msr to return value of msr_read(msr) */
	vmx_msr = msr_read(msr);
	/** Set vmx_msr_low to low 32 bits of vmx_msr */
	vmx_msr_low = (uint32_t)vmx_msr;
	/** Set vmx_msr_high to high 32 bits of vmx_msr */
	vmx_msr_high = (uint32_t)(vmx_msr >> 32U);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "VMX_PIN_VM_EXEC_CONTROLS:low=0x%x, high=0x%x\n"
	 *  - vmx_msr_low
	 *  - vmx_msr_high */
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS:low=0x%x, high=0x%x\n", vmx_msr_low, vmx_msr_high);

	/** Bitwise AND ctrl by vmx_msr_high */
	ctrl &= vmx_msr_high;
	/** Bitwise OR ctrl by vmx_msr_low */
	ctrl |= vmx_msr_low;

	/** If \a ctrl_req is different with ctrl */
	if ((ctrl_req & ~ctrl) != 0U) {
		/** Logging the following information with a log level of LOG_ERROR.
		 *  - "VMX ctrl 0x%x not fully enabled: "
		 *  - "request 0x%x but get 0x%x\n"
		 *  - msr
		 *  - ctrl_req
		 *  - ctrl */
		pr_err("VMX ctrl 0x%x not fully enabled: "
		       "request 0x%x but get 0x%x\n",
			msr, ctrl_req, ctrl);
	}

	/** Return ctrl */
	return ctrl;
}

/**
 * @brief This function is used to initialize the VMCS execution control fields.
 *
 * @param[inout] vcpu A pointer which points to a virtual CPU data structure
 * associated with the VMCS to be initialized.
 *
 * @return None
 *
 * @pre vcpu != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_exec_ctrl(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - value32 representing a variable to store 32 bits value. */
	uint32_t value32;
	/** Declare the following local variables of type uint32_t.
	 *  - value64 representing a variable to store 64 bits value. */
	uint64_t value64;
	/** Declare the following local variables of type struct acrn_vm *.
	 *  - vm representing the acrn_vm structure associated with the given \a vcpu,
	 *    initialized as vcpu->vm. */
	struct acrn_vm *vm = vcpu->vm;

	/* Log messages to show initializing VMX execution controls */
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************" */
	pr_dbg("*****************************");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Initialize execution control" */
	pr_dbg("Initialize execution control ");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************" */
	pr_dbg("*****************************");

	/* Set up VM Execution control to enable Set VM-exits on external
	 * interrupts preemption timer - pg 2899 24.6.1
	 */
	/* enable external interrupt VM Exit */
	/** Set "value32" to return value of check_vmx_ctrl(MSR_IA32_VMX_PINBASED_CTLS, VMX_PINBASED_CTLS_IRQ_EXIT) */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PINBASED_CTLS, VMX_PINBASED_CTLS_IRQ_EXIT);

	/** Call exec_vmwrite32() with the following parameters, in order to write value32 to the field
	 *  'Pin-based VM-execution controls' in current VMCS.
	 *  - VMX_PIN_VM_EXEC_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  "VMX_PIN_VM_EXEC_CONTROLS: 0x%x "
	 *  - value32 */
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS: 0x%x ", value32);

	/** Set "value32" to return value of check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS,
	 *  VMX_PROCBASED_CTLS_TSC_OFF | VMX_PROCBASED_CTLS_TPR_SHADOW | VMX_PROCBASED_CTLS_IO_BITMAP |
	 *  VMX_PROCBASED_CTLS_MSR_BITMAP | VMX_PROCBASED_CTLS_SECONDARY) */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS,
		VMX_PROCBASED_CTLS_TSC_OFF | VMX_PROCBASED_CTLS_TPR_SHADOW | VMX_PROCBASED_CTLS_IO_BITMAP |
			VMX_PROCBASED_CTLS_MSR_BITMAP | VMX_PROCBASED_CTLS_SECONDARY);

	/** Bitwise AND value32 by ~(VMX_PROCBASED_CTLS_CR3_LOAD | VMX_PROCBASED_CTLS_CR3_STORE) */
	value32 &= ~(VMX_PROCBASED_CTLS_CR3_LOAD | VMX_PROCBASED_CTLS_CR3_STORE);
	/** Bitwise AND value32 by ~(VMX_PROCBASED_CTLS_CR8_LOAD | VMX_PROCBASED_CTLS_CR8_STORE) */
	value32 &= ~(VMX_PROCBASED_CTLS_CR8_LOAD | VMX_PROCBASED_CTLS_CR8_STORE);

	/** Bitwise AND value32 by ~VMX_PROCBASED_CTLS_INVLPG */
	value32 &= ~VMX_PROCBASED_CTLS_INVLPG;

	/*
	 * Enable VM_EXIT for rdpmc execution.
	 */
	/** Bitwise OR value32 by VMX_PROCBASED_CTLS_RDPMC */
	value32 |= VMX_PROCBASED_CTLS_RDPMC;

	/** Bitwise OR value32 by VMX_PROCBASED_CTLS_MWAIT */
	value32 |= VMX_PROCBASED_CTLS_MWAIT;
	/** Bitwise OR value32 by VMX_PROCBASED_CTLS_MOV_DR */
	value32 |= VMX_PROCBASED_CTLS_MOV_DR;
	/** Bitwise OR value32 by VMX_PROCBASED_CTLS_MONITOR */
	value32 |= VMX_PROCBASED_CTLS_MONITOR;

	/** Call exec_vmwrite32() with the following parameters, in order to write value32 to the field
	 *  'Primary processor-based VM-execution controls' in current VMCS.
	 *  - VMX_PROC_VM_EXEC_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - value32 */
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS: 0x%x ", value32);

	/** Set "value32" to return value of check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS2,
	 *	VMX_PROCBASED_CTLS2_VAPIC | VMX_PROCBASED_CTLS2_EPT | VMX_PROCBASED_CTLS2_RDTSCP |
	 *		VMX_PROCBASED_CTLS2_UNRESTRICT) */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS2,
		VMX_PROCBASED_CTLS2_VAPIC | VMX_PROCBASED_CTLS2_EPT | VMX_PROCBASED_CTLS2_RDTSCP |
			VMX_PROCBASED_CTLS2_UNRESTRICT);

	/** Bitwise OR value32 by VMX_PROCBASED_CTLS2_VPID */
	value32 |= VMX_PROCBASED_CTLS2_VPID;

	/*
	 * This field exists only on processors that support
	 * the 1-setting  of the "use TPR shadow"
	 * VM-execution control.
	 *
	 * Set up TPR threshold for virtual interrupt delivery
	 * - pg 2904 24.6.8
	 */
	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the field
	 *  'TPR threshold' in current VMCS.
	 *  - VMX_TPR_THRESHOLD
	 *  - 0 */
	exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);

	/*
	 * Attempts to execute XRSTORS instructions in VMX non-root operation are handled
	 * by hardware directly by injection #UD. The VM exit is configured by setting
	 * the 'Enable XSAVES/XRSTORS' bit in secondary processor-based VM execution
	 * control to 0.
	 */

	/** Bitwise AND value32 by ~VMX_PROCBASED_CTLS2_XSVE_XRSTR */
	value32 &= ~VMX_PROCBASED_CTLS2_XSVE_XRSTR;

	/** Bitwise OR value32 by VMX_PROCBASED_CTLS2_WBINVD */
	value32 |= VMX_PROCBASED_CTLS2_WBINVD;

	/** Call exec_vmwrite32() with the following parameters, in order to write value32 to the field
	 *  'Secondary processor-based VM-execution controls' in current VMCS.
	 *  - VMX_PROC_VM_EXEC_CONTROLS2
	 *  - value32: */
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  "VMX_PROC_VM_EXEC_CONTROLS2: 0x%x "
	 *  - value32 */
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS2: 0x%x ", value32);

	/** Set "value64" to return value of hva2hpa(vm->arch_vm.nworld_eptp) bitwise OR
	 * ((3UL << 3U) | 6UL). */
	value64 = hva2hpa(vm->arch_vm.nworld_eptp) | (3UL << 3U) | 6UL;
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'EPT pointer' in current VMCS.
	 *  - VMX_EPT_POINTER_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_EPT_POINTER_FULL, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  "VMX_PIN_VM_EXEC_CONTROLS: 0x%x "
	 *  - value64 */
	pr_dbg("VMX_EPT_POINTER: 0x%016lx ", value64);

	/* Set up guest exception mask bitmap setting a bit * causes a VM exit
	 * on corresponding guest * exception - pg 2902 24.6.3
	 * enable VM exit on DB only
	 */
	/** Set "value32" to (1U << IDT_DB) */
	value32 = (1U << IDT_DB);
	/** Call exec_vmwrite32 with the following parameters, in order to write value32 to the field
	 *  'Exception bitmap' in current VMCS.
	 *  - VMX_EXCEPTION_BITMAP
	 *  - value32 */
	exec_vmwrite32(VMX_EXCEPTION_BITMAP, value32);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the field
	 *  'Page-fault error-code mask' in current VMCS.
	 *  - VMX_PF_ERROR_CODE_MASK
	 *  - 0 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MASK, 0U);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the field
	 *  'Page-fault error-code match' in current VMCS.
	 *  - VMX_PF_ERROR_CODE_MATCH
	 *  - 0 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MATCH, 0U);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to the field
	 *  'CR3-target count' in current VMCS.
	 *  - VMX_CR3_TARGET_COUNT
	 *  - 0 */
	exec_vmwrite32(VMX_CR3_TARGET_COUNT, 0U);

	/* Set up IO bitmap register A and B - pg 2902 24.6.4 */
	/** Set "value64" to return value of hva2hpa(vm->arch_vm.io_bitmap) */
	value64 = hva2hpa(vm->arch_vm.io_bitmap);
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'Address of I/O bitmap A' in current VMCS.
	 *  - VMX_IO_BITMAP_A_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - value64 */
	pr_dbg("VMX_IO_BITMAP_A: 0x%016lx ", value64);
	/** Set "value64" to return value of address of hva2hpa((void *)&(vm->arch_vm.io_bitmap[PAGE_SIZE])) */
	value64 = hva2hpa((void *)&(vm->arch_vm.io_bitmap[PAGE_SIZE]));
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'Address of I/O bitmap B' in current VMCS.
	 *  - VMX_IO_BITMAP_B_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_IO_BITMAP_B_FULL, value64);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - value64: the value of address of I/O bitmap B field */
	pr_dbg("VMX_IO_BITMAP_B: 0x%016lx ", value64);

	/** Call init_msr_emulation() with the following parameters, in order to  initializes and sets
	 *  up the MSR bitmap in VMexecution control fields, and initializes MSR store and
	 *  load area of the vcpu.
	 *  - vcpu */
	init_msr_emulation(vcpu);

	/** Call exec_vmwrite64() with the following parameters, in order to write 0 to the field
	 *  'executive-VMCS pointer' in current VMCS.
	 *  - VMX_EXECUTIVE_VMCS_PTR_FULL
	 *  - 0 */
	exec_vmwrite64(VMX_EXECUTIVE_VMCS_PTR_FULL, 0UL);

	/** Set "value64" to return value of vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST) -
	 *  return value of msr_read(MSR_IA32_TSC_ADJUST) */
	value64 = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST) - msr_read(MSR_IA32_TSC_ADJUST);
	/** Call exec_vmwrite64() with the following parameters, in order to write value64 to the field
	 *  'TSC offset' in current VMCS.
	 *  - VMX_TSC_OFFSET_FULL
	 *  - value64 */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, value64);

	/** Call exec_vmwrite64() with the following parameters, in order to write FFFFFFFFFFFFFFFFH
	 *  to the field 'VMCS link pointer' in current VMCS.
	 *  - VMX_VMS_LINK_PTR_FULL
	 *  - 0xFFFFFFFFFFFFFFFFUL */
	exec_vmwrite64(VMX_VMS_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFFUL);

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Natural-width*********": information */
	pr_dbg("Natural-width*********");

	/** Call init_cr0_cr4_host_mask() to initialize the
	 *  CR0 Guest/Host Masks and CR4 Guest/Host Masks in the current VMCS. */
	init_cr0_cr4_host_mask();

	/** Call exec_vmwrite() with the following parameters, in order to write 0
	 *  to the field 'CR3-target value 0' in current VMCS.
	 *  - VMX_CR3_TARGET_0
	 *  - 0UL */
	exec_vmwrite(VMX_CR3_TARGET_0, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0
	 *  to the field 'CR3-target value 1' in current VMCS.
	 *  - VMX_CR3_TARGET_1
	 *  - 0UL */
	exec_vmwrite(VMX_CR3_TARGET_1, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0
	 *  to the field 'CR3-target value 2' in current VMCS.
	 *  - VMX_CR3_TARGET_2
	 *  - 0UL */
	exec_vmwrite(VMX_CR3_TARGET_2, 0UL);
	/** Call exec_vmwrite() with the following parameters, in order to write 0
	 *  to the field 'CR3-target value 3' in current VMCS.
	 *  - VMX_CR3_TARGET_3
	 *  - 0UL */
	exec_vmwrite(VMX_CR3_TARGET_3, 0UL);
}

/**
 * @brief This function is used to initialize the VM-entry control fields in the VMCS.
 *
 * @param[in] vcpu A pointer which points to a virtual CPU data structure associated
 * with the VMCS to be initialized.
 *
 * @return None
 *
 * @pre vcpu!= NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_entry_ctrl(const struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - value32 representing a variable to store 32 bits value. */
	uint32_t value32;

	/* Log messages to show initializing VMX entry controls */
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************": information */
	pr_dbg("*************************");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Initialize Entry control": information */
	pr_dbg("Initialize Entry control ");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************": information */
	pr_dbg("*************************");

	/* Set up VMX entry controls - pg 2908 24.8.1 * Set IA32e guest mode -
	 * on VM entry processor is in IA32e 64 bit mode * Start guest with host
	 * IA32_PAT and IA32_EFER
	 */
	/** Set "value32" to (VMX_ENTRY_CTLS_LOAD_EFER | VMX_ENTRY_CTLS_LOAD_PAT) */
	value32 = (VMX_ENTRY_CTLS_LOAD_EFER | VMX_ENTRY_CTLS_LOAD_PAT);
	/** Set "value32" to return value of check_vmx_ctrl(MSR_IA32_VMX_ENTRY_CTLS, value32) */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_ENTRY_CTLS, value32);

	/** Bitwise AND value32 by ~VMX_ENTRY_CTLS_LOAD_DEBUGCTL */
	value32 &= ~VMX_ENTRY_CTLS_LOAD_DEBUGCTL;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to the field 'VM-entry controls' in current VMCS.
	 *  - VMX_ENTRY_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_ENTRY_CONTROLS, value32);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - value32: VM-entry controls field in the VMCS */
	pr_dbg("VMX_ENTRY_CONTROLS: 0x%x ", value32);

	/** Call exec_vmwrite32() with the following parameters, in order to write MSR_AREA_COUNT
	 *  to the field 'VM-entry MSR-load count' in current VMCS.
	 *  - VMX_ENTRY_MSR_LOAD_COUNT
	 *  - MSR_AREA_COUNT */
	exec_vmwrite32(VMX_ENTRY_MSR_LOAD_COUNT, MSR_AREA_COUNT);
	/** Call exec_vmwrite64() with the following parameters, in order to write
	 *  hva2hpa((void *)vcpu->arch.msr_area.guest) to VM-entry MSR-load address field in current VMCS.
	 *  - VMX_ENTRY_MSR_LOAD_ADDR_FULL
	 *  - hva2hpa((void *)vcpu->arch.msr_area.guest) */
	exec_vmwrite64(VMX_ENTRY_MSR_LOAD_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.guest));

	/** Call exec_vmwrite32() with the following parameters, in order to write 0
	 *  to VM-entry interruption-information field in current VMCS.
	 *  - VMX_ENTRY_INT_INFO_FIELD
	 *  - 0U */
	exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, 0U);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0
	 *  to VM-entry exception error code field in current VMCS.
	 *  - VMX_ENTRY_INT_INFO_FIELD
	 *  - 0U */
	exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, 0U);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0
	 *  to VM-entry instruction length field in current VMCS.
	 *  - VMX_ENTRY_INT_INFO_FIELD
	 *  - 0U */
	exec_vmwrite32(VMX_ENTRY_INSTR_LENGTH, 0U);
}

/**
 * @brief This function is used to initialize the VM-exit control fields in the VMCS.
 *
 * @param[in] vcpu A pointer which points to a virtual CPU data structure associated
 * with the VMCS to be initialized.
 *
 * @return None
 *
 * @pre vcpu!= NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
static void init_exit_ctrl(const struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - value32 representing a variable to store 32 bits value. */
	uint32_t value32;

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************": information */
	pr_dbg("************************");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Initialize Exit control ": information */
	pr_dbg("Initialize Exit control ");
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "*****************************": information */
	pr_dbg("************************");

	/** Set "value32" to return value of check_vmx_ctrl(MSR_IA32_VMX_EXIT_CTLS,
	 *	VMX_EXIT_CTLS_ACK_IRQ | VMX_EXIT_CTLS_SAVE_PAT | VMX_EXIT_CTLS_LOAD_PAT | VMX_EXIT_CTLS_LOAD_EFER |
	 *		VMX_EXIT_CTLS_SAVE_EFER | VMX_EXIT_CTLS_HOST_ADDR64) */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_EXIT_CTLS,
		VMX_EXIT_CTLS_ACK_IRQ | VMX_EXIT_CTLS_SAVE_PAT | VMX_EXIT_CTLS_LOAD_PAT | VMX_EXIT_CTLS_LOAD_EFER |
			VMX_EXIT_CTLS_SAVE_EFER | VMX_EXIT_CTLS_HOST_ADDR64);

	/** Bitwise AND value32 by ~VMX_EXIT_CTLS_SAVE_DEBUGCTL */
	value32 &= ~VMX_EXIT_CTLS_SAVE_DEBUGCTL;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to VM-exit controls field in the VMCS.
	 *  - VMX_EXIT_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_EXIT_CONTROLS, value32);
	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "value32" */
	pr_dbg("VMX_EXIT_CONTROL: 0x%x ", value32);

	/** Call exec_vmwrite32() with the following parameters, in order to write MSR_AREA_COUNT
	 *  to VM-exit MSR-store count field in the VMCS.
	 *  - VMX_EXIT_MSR_STORE_COUNT
	 *  - MSR_AREA_COUNT */
	exec_vmwrite32(VMX_EXIT_MSR_STORE_COUNT, MSR_AREA_COUNT);
	/** Call exec_vmwrite32() with the following parameters, in order to write MSR_AREA_COUNT
	 *  to VM-exit MSR-load count field in the VMCS.
	 *  - VMX_EXIT_MSR_LOAD_COUNT
	 *  - MSR_AREA_COUNT */
	exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT, MSR_AREA_COUNT);
	/** Call exec_vmwrite64() with the following parameters, in order to write
	 *  hva2hpa((void *)vcpu->arch.msr_area.guest) to VMX_EXIT_MSR_STORE_ADDR_FULL field in the VMCS.
	 *  - VMX_EXIT_MSR_STORE_ADDR_FULL
	 *  - hva2hpa((void *)vcpu->arch.msr_area.guest) */
	exec_vmwrite64(VMX_EXIT_MSR_STORE_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.guest));
	/** Call exec_vmwrite64() with the following parameters, in order to write
	 *  hva2hpa((void *)vcpu->arch.msr_area.host) to VM-exit MSR-load address field in the VMCS.
	 *  - VMX_EXIT_MSR_LOAD_ADDR_FULL
	 *  - hva2hpa((void *)vcpu->arch.msr_area.host) */
	exec_vmwrite64(VMX_EXIT_MSR_LOAD_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.host));
}

/**
 * @brief This function is used to initialize the VMCS fields.
 *
 * @param[inout] vcpu A pointer which points to a virtual CPU data structure
 * associated with the VMCS to be initialized.
 *
 * @return None
 *
 * @pre vcpu!= NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
void init_vmcs(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint64_t.
	 *  - vmx_rev_id representing a variable to store VMCS revision identifier. */
	uint64_t vmx_rev_id;
	/** Declare the following local variables of type uint64_t.
	 *  - vmcs_pa representing a variable to store physical address of the VMCS. */
	uint64_t vmcs_pa;
	/** Declare the following local variables of type 'void **'.
	 *  - vmcs_ptr representing a pointer which points to the field 'vmcs_run'in per_cpu_region
	 *  associated with the current physical CPU, initialized as &get_cpu_var(vmcs_run). */
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	/** Logging the following information with a log level of LOG_DEBUG.
	 *  - "Initializing VMCS" */
	pr_dbg("Initializing VMCS");

	/** Set vmx_rev_id to return value of msr_read(MSR_IA32_VMX_BASIC) */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	/** Call memcpy_s with the following parameters, in order to copy VMCS
	 *  revision identifier from MSR IA32_VMX_BASIC to the VMCS field.
	 *  - vcpu->arch.vmcs
	 *  - 4U
	 *  - &vmx_rev_id
	 *  - 4U */
	(void)memcpy_s(vcpu->arch.vmcs, 4U, (void *)&vmx_rev_id, 4U);

	/** Set vmcs_pa to return value of hva2hpa(vcpu->arch.vmcs) */
	vmcs_pa = hva2hpa(vcpu->arch.vmcs);
	/** Call exec_vmclear with the following parameters, in order to clear VMCS field of the vCPU.
	 *  - &vmcs_pa */
	exec_vmclear((void *)&vmcs_pa);
	/** Call exec_vmptrld() with the following parameters, in order to load VMCS pointer from
	 *  memory pointed by '&vmcs_pa'.
	 *  - &vmcs_pa
	 */
	exec_vmptrld((void *)&vmcs_pa);
	/** Set *vmcs_ptr to vcpu->arch.vmcs */
	*vmcs_ptr = (void *)vcpu->arch.vmcs;

	/** Call init_host_state() to initialize the VMCS host state fields.*/
	init_host_state();
	/** Call init_exec_ctrl() with the following parameters, in order to initialize execution control fields
	 *  of the VMCS.
	 *  - vcpu */
	init_exec_ctrl(vcpu);
	/** Call init_guest_state() with the following parameters, in order to initialize guest state fields
	 *  of the VMCS.
	 *  - vcpu */
	init_guest_state(vcpu);
	/** Call init_entry_ctrl() with the following parameters, in order to initialize VM-entry control fields
	 *  of the VMCS.
	 *  - vcpu */
	init_entry_ctrl(vcpu);
	/** Call init_exit_ctrl() with the following parameters, in order to initialize VM-exit control fields
	 *  of the VMCS.
	 *  - vcpu */
	init_exit_ctrl(vcpu);
	/** Call switch_apicv_mode_x2apic() with the following parameters, in order to switch to X2APIC mode.
	 *  - vcpu */
	switch_apicv_mode_x2apic(vcpu);
}

/**
 * @brief This function is used to load the VMCS of the given \a vcpu on the current physical processor.
 *
 * @param[in] vcpu A pointer which points to a virtual CPU data structure associated
 * with the VMCS to be loaded.
 *
 * @return None
 *
 * @pre vcpu->launched==true
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
void load_vmcs(const struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint64_t.
	 *  - vmcs_pa representing a variable to store physical address of the VMCS. */
	uint64_t vmcs_pa;
	/** Declare the following local variables of type 'void **'.
	 *  - vmcs_ptr representing a pointer which points to the field 'vmcs_run'in per_cpu_region
	 *  associated with the current physical CPU, initialized as &get_cpu_var(vmcs_run). */
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	/** If *vmcs_ptr is different from vcpu->arch.vmcs */
	if (*vmcs_ptr != (void *)vcpu->arch.vmcs) {
		/** Set vmcs_pa to return value of hva2hpa(vcpu->arch.vmcs) */
		vmcs_pa = hva2hpa(vcpu->arch.vmcs);
		/** Call exec_vmptrld() with the following parameters, in order to load VMCS pointer from
		 *  memory pointed by '&vmcs_pa'.
		 *  - &vmcs_pa
		 */
		exec_vmptrld((void *)&vmcs_pa);
		/** Set *vmcs_ptr to vcpu->arch.vmcs */
		*vmcs_ptr = (void *)vcpu->arch.vmcs;
	}
}

/**
 * @brief This function is used to switch to x2apic mode.
 *
 * @param[inout] vcpu A pointer which points to a structure representing the VCPU whose notification
 * mode is to be updated
 *
 * @return None
 *
 * @pre vcpu!= NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When \a vcpu is different among parallel invocation.
 */
void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu)
{
	/** Declare the following local variables of type uint32_t.
	 *  - value32 representing a temporary variable to store value to be set to VMCS. */
	uint32_t value32;
	/** Call dev_dbg() with the following parameters, in order to print the debug log.
	 *  - ACRN_DBG_LAPICPT
	 *  - "%s: switching to x2apic and passthru"
	 *  - __func__ */
	dev_dbg(ACRN_DBG_LAPICPT, "%s: switching to x2apic and passthru", __func__);

	/** Set "value32" to return value of exec_vmread32(VMX_PIN_VM_EXEC_CONTROLS) */
	value32 = exec_vmread32(VMX_PIN_VM_EXEC_CONTROLS);
	/** Bitwise AND value32 by ~VMX_PINBASED_CTLS_IRQ_EXIT */
	value32 &= ~VMX_PINBASED_CTLS_IRQ_EXIT;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to Pin-based VM-execution controls field in current VMCS.
	 *  - VMX_PIN_VM_EXEC_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);

	/** Set "value32" to return value of exec_vmread32(VMX_EXIT_CONTROLS) */
	value32 = exec_vmread32(VMX_EXIT_CONTROLS);
	/** Bitwise AND value32 by ~VMX_EXIT_CTLS_ACK_IRQ */
	value32 &= ~VMX_EXIT_CTLS_ACK_IRQ;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to VM-exit controls field in current VMCS.
	 *  - VMX_EXIT_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_EXIT_CONTROLS, value32);

	/** Set "value32" to return value of exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS) */
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	/** Bitwise AND value32 by ~VMX_PROCBASED_CTLS_TPR_SHADOW */
	value32 &= ~VMX_PROCBASED_CTLS_TPR_SHADOW;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to Primary processor-based VM-execution controls field in current VMCS.
	 *  - VMX_PROC_VM_EXEC_CONTROLS
	 *  - value32 */
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);

	/** Call exec_vmwrite32() with the following parameters, in order to write 0 to
	 *  TPR threshold field in current VMCS.
	 *  - VMX_TPR_THRESHOLD
	 *  - 0 */
	exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);

	/** Set "value32" to return value of exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2) */
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
	/** Bitwise AND value32 by ~VMX_PROCBASED_CTLS2_VAPIC */
	value32 &= ~VMX_PROCBASED_CTLS2_VAPIC;
	/** Call exec_vmwrite32() with the following parameters, in order to write value32
	 *  to secondary processor-based VM-execution controls field in current VMCS.
	 *  - VMX_PROC_VM_EXEC_CONTROLS2
	 *  - value32 */
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);

	/** Set notify mode of the vcpu thread to SCHED_NOTIFY_INIT */
	vcpu->thread_obj.notify_mode = SCHED_NOTIFY_INIT;
}

/**
 * @}
 */
