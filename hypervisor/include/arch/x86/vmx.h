/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMX_H_
#define VMX_H_

/**
 * @addtogroup hwmgmt_vmx
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external functions and MACROS that shall be provided by the hwmgmt.vmx module.
 */

#include <types.h>

/**
 * @brief Address of guest's ES segment register control field in the VMCS.
 */
#define VMX_GUEST_ES_SEL   0x00000800U
/**
 * @brief Address of guest's CS segment register control field in the VMCS.
 */
#define VMX_GUEST_CS_SEL   0x00000802U
/**
 * @brief Address of guest's SS segment register control field in the VMCS.
 */
#define VMX_GUEST_SS_SEL   0x00000804U
/**
 * @brief Address of guest's DS segment register control field in the VMCS.
 */
#define VMX_GUEST_DS_SEL   0x00000806U
/**
 * @brief Address of guest's FS segment register control field in the VMCS.
 */
#define VMX_GUEST_FS_SEL   0x00000808U
/**
 * @brief Address of guest's GS segment register control field in the VMCS.
 */
#define VMX_GUEST_GS_SEL   0x0000080aU
/**
 * @brief Address of guest's LDTR register control field in the VMCS.
 */
#define VMX_GUEST_LDTR_SEL 0x0000080cU
/**
 * @brief Address of guest's TR register control field in the VMCS.
 */
#define VMX_GUEST_TR_SEL   0x0000080eU
/**
 * @brief VPID VM-execution control field in the VMCS.
 */
#define VMX_VPID 0x00000000U
/**
 * @brief Address of host's ES segment register control field in the VMCS.
 */
#define VMX_HOST_ES_SEL 0x00000c00U
/**
 * @brief Address of host's CS segment register control field in the VMCS.
 */
#define VMX_HOST_CS_SEL 0x00000c02U
/**
 * @brief Address of host's SS segment register control field in the VMCS.
 */
#define VMX_HOST_SS_SEL 0x00000c04U
/**
 * @brief Address of host's DS segment register control field in the VMCS.
 */
#define VMX_HOST_DS_SEL 0x00000c06U
/**
 * @brief Address of host's FS segment register control field in the VMCS.
 */
#define VMX_HOST_FS_SEL 0x00000c08U
/**
 * @brief Address of host's GS segment register control field in the VMCS.
 */
#define VMX_HOST_GS_SEL 0x00000c0aU
/**
 * @brief Address of host's TR segment register control field in the VMCS.
 */
#define VMX_HOST_TR_SEL 0x00000c0cU
/**
 * @brief Address of I/O bitmap A control field in the VMCS for each I/O port in the range 0 through 0x7FFF.
 */
#define VMX_IO_BITMAP_A_FULL         0x00002000U
/**
 * @brief Address of I/O bitmap B control field in the VMCS for each I/O port in the range 0x8000 through 0xFFFF.
 */
#define VMX_IO_BITMAP_B_FULL         0x00002002U
/**
 * @brief Address of MSR bitmaps control field in the VMCS.
 */
#define VMX_MSR_BITMAP_FULL          0x00002004U
/**
 * @brief Address of VM-exit MSR-store address control field in the VMCS.
 */
#define VMX_EXIT_MSR_STORE_ADDR_FULL 0x00002006U
/**
 * @brief Address of VM-exit MSR-load address control field in the VMCS.
 */
#define VMX_EXIT_MSR_LOAD_ADDR_FULL  0x00002008U
/**
 * @brief Address of VM-entry MSR-load address control field in the VMCS.
 */
#define VMX_ENTRY_MSR_LOAD_ADDR_FULL 0x0000200aU
/**
 * @brief Address of Executive-VMCS pointer control field in the VMCS.
 */
#define VMX_EXECUTIVE_VMCS_PTR_FULL  0x0000200cU
/**
 * @brief Address of TSC offset control field in the VMCS.
 */
#define VMX_TSC_OFFSET_FULL          0x00002010U
/**
 * @brief Address of EPT pointer control field in the VMCS.
 */
#define VMX_EPT_POINTER_FULL         0x0000201AU
/**
 * @brief Address of XSAVE/XRESTORE execution control field in the VMCS.
 */
#define VMX_XSS_EXITING_BITMAP_FULL  0x0000202CU

/**
 * @brief Address of Guest physical address control field in the VMCS.
 * This address triggers the EPT violation.
 */
#define VMX_GUEST_PHYSICAL_ADDR_FULL 0x00002400U
/**
 * @brief Address of link pointer control field in the VMCS.
 */
#define VMX_VMS_LINK_PTR_FULL        0x00002800U
/**
 * @brief Address of guest IA32_DEBUGCTL control field in the VMCS.
 */
#define VMX_GUEST_IA32_DEBUGCTL_FULL 0x00002802U
/**
 * @brief Address of guest IA32_PAT control field in the VMCS.
 */
#define VMX_GUEST_IA32_PAT_FULL      0x00002804U
/**
 * @brief Address of guest IA32_EFER control field in the VMCS.
 */
#define VMX_GUEST_IA32_EFER_FULL     0x00002806U
/**
 * @brief Address of guest PDPTE0 control field in the VMCS.
 */
#define VMX_GUEST_PDPTE0_FULL        0x0000280AU
/**
 * @brief Address of guest PDPTE1 control field in the VMCS.
 */
#define VMX_GUEST_PDPTE1_FULL        0x0000280CU
/**
 * @brief Address of guest PDPTE2 control field in the VMCS.
 */
#define VMX_GUEST_PDPTE2_FULL        0x0000280EU
/**
 * @brief Address of guest PDPTE3 control field in the VMCS.
 */
#define VMX_GUEST_PDPTE3_FULL        0x00002810U
/**
 * @brief Address of host IA32_PAT control field in the VMCS.
 */
#define VMX_HOST_IA32_PAT_FULL  0x00002C00U
/**
 * @brief Address of host IA32_EFER control field in the VMCS.
 */
#define VMX_HOST_IA32_EFER_FULL 0x00002C02U
/**
 * @brief Address of pin-based control field in the VMCS.
 */
#define VMX_PIN_VM_EXEC_CONTROLS       0x00004000U
/**
 * @brief Address of primary processor-based control field in the VMCS.
 */
#define VMX_PROC_VM_EXEC_CONTROLS      0x00004002U
/**
 * @brief Address of exception bitmap control field in the VMCS.
 */
#define VMX_EXCEPTION_BITMAP           0x00004004U
/**
 * @brief Address of page fault error code mask control field in the VMCS.
 */
#define VMX_PF_ERROR_CODE_MASK         0x00004006U
/**
 * @brief Address of page fault error code match control field in the VMCS.
 */
#define VMX_PF_ERROR_CODE_MATCH        0x00004008U
/**
 * @brief Address of CR3 target control field in the VMCS.
 */
#define VMX_CR3_TARGET_COUNT           0x0000400aU
/**
 * @brief Address of VM exit control field in the VMCS.
 */
#define VMX_EXIT_CONTROLS              0x0000400cU
/**
 * @brief Address of VM exit MSR store count control field in the VMCS.
 */
#define VMX_EXIT_MSR_STORE_COUNT       0x0000400eU
/**
 * @brief Address of VM exit MSR load count control field in the VMCS.
 */
#define VMX_EXIT_MSR_LOAD_COUNT        0x00004010U
/**
 * @brief Address of VM entry control field in the VMCS.
 */
#define VMX_ENTRY_CONTROLS             0x00004012U
/**
 * @brief Address of VM entry MSR load count control field in the VMCS.
 */
#define VMX_ENTRY_MSR_LOAD_COUNT       0x00004014U
/**
 * @brief Address of VM entry interruption information field in the VMCS.
 */
#define VMX_ENTRY_INT_INFO_FIELD       0x00004016U
/**
 * @brief Address of VM entry exception error code field in the VMCS.
 */
#define VMX_ENTRY_EXCEPTION_ERROR_CODE 0x00004018U
/**
 * @brief Address of VM entry instruction length field in the VMCS.
 */
#define VMX_ENTRY_INSTR_LENGTH         0x0000401aU
/**
 * @brief Address of TPR threshold control field in the VMCS.
 */
#define VMX_TPR_THRESHOLD              0x0000401cU
/**
 * @brief Address of secondary processor-based control field in the VMCS.
 */
#define VMX_PROC_VM_EXEC_CONTROLS2     0x0000401EU
/**
 * @brief Address of VM instruction error field in the VMCS.
 */
#define VMX_INSTR_ERROR         0x00004400U
/**
 * @brief Address of VM exit reason field in the VMCS.
 */
#define VMX_EXIT_REASON         0x00004402U
/**
 * @brief Address of VM exit interruption information field in the VMCS.
 */
#define VMX_EXIT_INT_INFO       0x00004404U
/**
 * @brief Address of VM exit interruption error code field in the VMCS.
 */
#define VMX_EXIT_INT_ERROR_CODE 0x00004406U
/**
 * @brief Address of IDT vectoring information field in the VMCS.
 */
#define VMX_IDT_VEC_INFO_FIELD  0x00004408U
/**
 * @brief Address of IDT vectoring error code field in the VMCS.
 */
#define VMX_IDT_VEC_ERROR_CODE  0x0000440aU
/**
 * @brief Address of VM exit instruction length field in the VMCS.
 */
#define VMX_EXIT_INSTR_LEN      0x0000440cU
/**
 * @brief Address of guest ES segment limit control field in the VMCS.
 */
#define VMX_GUEST_ES_LIMIT              0x00004800U
/**
 * @brief Address of guest CS segment limit control field in the VMCS.
 */
#define VMX_GUEST_CS_LIMIT              0x00004802U
/**
 * @brief Address of guest SS segment limit control field in the VMCS.
 */
#define VMX_GUEST_SS_LIMIT              0x00004804U
/**
 * @brief Address of guest DS segment limit control field in the VMCS.
 */
#define VMX_GUEST_DS_LIMIT              0x00004806U
/**
 * @brief Address of guest FS segment limit control field in the VMCS.
 */
#define VMX_GUEST_FS_LIMIT              0x00004808U
/**
 * @brief Address of guest GS segment limit control field in the VMCS.
 */
#define VMX_GUEST_GS_LIMIT              0x0000480aU
/**
 * @brief Address of guest LDTR limit control field in the VMCS.
 */
#define VMX_GUEST_LDTR_LIMIT            0x0000480cU
/**
 * @brief Address of guest TR limit control field in the VMCS.
 */
#define VMX_GUEST_TR_LIMIT              0x0000480eU
/**
 * @brief Address of guest GDTR limit control field in the VMCS.
 */
#define VMX_GUEST_GDTR_LIMIT            0x00004810U
/**
 * @brief Address of guest IDTR limit control field in the VMCS.
 */
#define VMX_GUEST_IDTR_LIMIT            0x00004812U
/**
 * @brief Address of guest ES access rights control field in the VMCS.
 */
#define VMX_GUEST_ES_ATTR               0x00004814U
/**
 * @brief Address of guest CS access rights control field in the VMCS.
 */
#define VMX_GUEST_CS_ATTR               0x00004816U
/**
 * @brief Address of guest SS access rights control field in the VMCS.
 */
#define VMX_GUEST_SS_ATTR               0x00004818U
/**
 * @brief Address of guest DS access rights control field in the VMCS.
 */
#define VMX_GUEST_DS_ATTR               0x0000481aU
/**
 * @brief Address of guest FS access rights control field in the VMCS.
 */
#define VMX_GUEST_FS_ATTR               0x0000481cU
/**
 * @brief Address of guest GS access rights control field in the VMCS.
 */
#define VMX_GUEST_GS_ATTR               0x0000481eU
/**
 * @brief Address of guest LDTR access rights control field in the VMCS.
 */
#define VMX_GUEST_LDTR_ATTR             0x00004820U
/**
 * @brief Address of guest TR access rights control field in the VMCS.
 */
#define VMX_GUEST_TR_ATTR               0x00004822U
/**
 * @brief Address of guest interruptibility state control field in the VMCS.
 */
#define VMX_GUEST_INTERRUPTIBILITY_INFO 0x00004824U
/**
 * @brief Address of guest activity state control field in the VMCS.
 */
#define VMX_GUEST_ACTIVITY_STATE        0x00004826U
/**
 * @brief Address of guest SMBASE control field in the VMCS.
 */
#define VMX_GUEST_SMBASE                0x00004828U
/**
 * @brief Address of guest MSR_IA32_SYSENTER_CS control field in the VMCS.
 */
#define VMX_GUEST_IA32_SYSENTER_CS      0x0000482aU
/**
 * @brief Address of host MSR_IA32_SYSENTER_CS control field in the VMCS.
 */
#define VMX_HOST_IA32_SYSENTER_CS 0x00004c00U
/**
 * @brief Address of CR0 guest/host mask control field in the VMCS.
 */
#define VMX_CR0_GUEST_HOST_MASK 0x00006000U
/**
 * @brief Address of CR4 guest/host mask control field in the VMCS.
 */
#define VMX_CR4_GUEST_HOST_MASK 0x00006002U
/**
 * @brief Address of CR0 read shadow field in the VMCS.
 */
#define VMX_CR0_READ_SHADOW     0x00006004U
/**
 * @brief Address of CR4 read shadow field in the VMCS.
 */
#define VMX_CR4_READ_SHADOW     0x00006006U
/**
 * @brief Address of CR3-target value 0 field in the VMCS.
 */
#define VMX_CR3_TARGET_0        0x00006008U
/**
 * @brief Address of CR3-target value 1 field in the VMCS.
 */
#define VMX_CR3_TARGET_1        0x0000600aU
/**
 * @brief Address of CR3-target value 2 field in the VMCS.
 */
#define VMX_CR3_TARGET_2        0x0000600cU
/**
 * @brief Address of CR3-target value 3 field in the VMCS.
 */
#define VMX_CR3_TARGET_3        0x0000600eU
/**
 * @brief Address of VM exit qualification field in the VMCS.
 */
#define VMX_EXIT_QUALIFICATION 0x00006400U
/**
 * @brief Address of guest linear address field in the VMCS.
 */
#define VMX_GUEST_LINEAR_ADDR  0x0000640aU
/**
 * @brief Address of guest CR0 control field in the VMCS.
 */
#define VMX_GUEST_CR0                  0x00006800U
/**
 * @brief Address of guest CR3 control field in the VMCS.
 */
#define VMX_GUEST_CR3                  0x00006802U
/**
 * @brief Address of guest CR4 control field in the VMCS.
 */
#define VMX_GUEST_CR4                  0x00006804U
/**
 * @brief Address of guest ES base control field in the VMCS.
 */
#define VMX_GUEST_ES_BASE              0x00006806U
/**
 * @brief Address of guest CS base control field in the VMCS.
 */
#define VMX_GUEST_CS_BASE              0x00006808U
/**
 * @brief Address of guest SS base control field in the VMCS.
 */
#define VMX_GUEST_SS_BASE              0x0000680aU
/**
 * @brief Address of guest DS base control field in the VMCS.
 */
#define VMX_GUEST_DS_BASE              0x0000680cU
/**
 * @brief Address of guest FS base control field in the VMCS.
 */
#define VMX_GUEST_FS_BASE              0x0000680eU
/**
 * @brief Address of guest GS base control field in the VMCS.
 */
#define VMX_GUEST_GS_BASE              0x00006810U
/**
 * @brief Address of guest LDTR base control field in the VMCS.
 */
#define VMX_GUEST_LDTR_BASE            0x00006812U
/**
 * @brief Address of guest TR base control field in the VMCS.
 */
#define VMX_GUEST_TR_BASE              0x00006814U
/**
 * @brief Address of guest GDTR base control field in the VMCS.
 */
#define VMX_GUEST_GDTR_BASE            0x00006816U
/**
 * @brief Address of guest IDTR base control field in the VMCS.
 */
#define VMX_GUEST_IDTR_BASE            0x00006818U
/**
 * @brief Address of guest DR7 control field in the VMCS.
 */
#define VMX_GUEST_DR7                  0x0000681aU
/**
 * @brief Address of guest RSP control field in the VMCS.
 */
#define VMX_GUEST_RSP                  0x0000681cU
/**
 * @brief Address of guest RIP control field in the VMCS.
 */
#define VMX_GUEST_RIP                  0x0000681eU
/**
 * @brief Address of guest RFLAGS control field in the VMCS.
 */
#define VMX_GUEST_RFLAGS               0x00006820U
/**
 * @brief Address of guest pending debug exceptions control field in the VMCS.
 */
#define VMX_GUEST_PENDING_DEBUG_EXCEPT 0x00006822U
/**
 * @brief Address of guest IA32_SYSENTER_ESP control field in the VMCS.
 */
#define VMX_GUEST_IA32_SYSENTER_ESP    0x00006824U
/**
 * @brief Address of guest IA32_SYSENTER_EIP control field in the VMCS.
 */
#define VMX_GUEST_IA32_SYSENTER_EIP    0x00006826U
/**
 * @brief Address of host CR0 control field in the VMCS.
 */
#define VMX_HOST_CR0               0x00006c00U
/**
 * @brief Address of host CR3 control field in the VMCS.
 */
#define VMX_HOST_CR3               0x00006c02U
/**
 * @brief Address of host CR4 control field in the VMCS.
 */
#define VMX_HOST_CR4               0x00006c04U
/**
 * @brief Address of host FS base control field in the VMCS.
 */
#define VMX_HOST_FS_BASE           0x00006c06U
/**
 * @brief Address of host GS base control field in the VMCS.
 */
#define VMX_HOST_GS_BASE           0x00006c08U
/**
 * @brief Address of host TR base control field in the VMCS.
 */
#define VMX_HOST_TR_BASE           0x00006c0aU
/**
 * @brief Address of host GDTR base control field in the VMCS.
 */
#define VMX_HOST_GDTR_BASE         0x00006c0cU
/**
 * @brief Address of host IDTR base control field in the VMCS.
 */
#define VMX_HOST_IDTR_BASE         0x00006c0eU
/**
 * @brief Address of host IA32_SYSENTER_ESP control field in the VMCS.
 */
#define VMX_HOST_IA32_SYSENTER_ESP 0x00006c10U
/**
 * @brief Address of host IA32_SYSENTER_EIP control field in the VMCS.
 */
#define VMX_HOST_IA32_SYSENTER_EIP 0x00006c12U
/**
 * @brief Address of host RIP control field in the VMCS.
 */
#define VMX_HOST_RIP               0x00006c16U

/**
 * @brief VM exit reason field for exception or non-maskable interrupt.
 */
#define VMX_EXIT_REASON_EXCEPTION_OR_NMI                  0x00000000U
/**
 * @brief VM exit reason field for external interrupt.
 */
#define VMX_EXIT_REASON_EXTERNAL_INTERRUPT                0x00000001U
/**
 * @brief VM exit reason field for triple fault.
 */
#define VMX_EXIT_REASON_TRIPLE_FAULT                      0x00000002U
/**
 * @brief VM exit reason field for INIT signal.
 */
#define VMX_EXIT_REASON_INIT_SIGNAL                       0x00000003U
/**
 * @brief VM exit reason field for start-up IPI.
 */
#define VMX_EXIT_REASON_STARTUP_IPI                       0x00000004U
/**
 * @brief VM exit reason field for I/O system-management interrupt.
 */
#define VMX_EXIT_REASON_IO_SMI                            0x00000005U
/**
 * @brief VM exit reason field for other SMI.
 */
#define VMX_EXIT_REASON_OTHER_SMI                         0x00000006U
/**
 * @brief VM exit reason field for interrupt window.
 */
#define VMX_EXIT_REASON_INTERRUPT_WINDOW                  0x00000007U
/**
 * @brief VM exit reason field for NMI window.
 */
#define VMX_EXIT_REASON_NMI_WINDOW                        0x00000008U
/**
 * @brief VM exit reason field for task switch.
 */
#define VMX_EXIT_REASON_TASK_SWITCH                       0x00000009U
/**
 * @brief VM exit reason field for CPUID instruction.
 */
#define VMX_EXIT_REASON_CPUID                             0x0000000AU
/**
 * @brief VM exit reason field for GETSEC instruction.
 */
#define VMX_EXIT_REASON_GETSEC                            0x0000000BU
/**
 * @brief VM exit reason field for HLT instruction.
 */
#define VMX_EXIT_REASON_HLT                               0x0000000CU
/**
 * @brief VM exit reason field for INVD instruction.
 */
#define VMX_EXIT_REASON_INVD                              0x0000000DU
/**
 * @brief VM exit reason field for INVLPG instruction.
 */
#define VMX_EXIT_REASON_INVLPG                            0x0000000EU
/**
 * @brief VM exit reason field for RDPMC instruction.
 */
#define VMX_EXIT_REASON_RDPMC                             0x0000000FU
/**
 * @brief VM exit reason field for RDTSC instruction.
 */
#define VMX_EXIT_REASON_RDTSC                             0x00000010U
/**
 * @brief VM exit reason field for RSM instruction.
 */
#define VMX_EXIT_REASON_RSM                               0x00000011U
/**
 * @brief VM exit reason field for VMCALL instruction.
 */
#define VMX_EXIT_REASON_VMCALL                            0x00000012U
/**
 * @brief VM exit reason field for VMCLEAR instruction.
 */
#define VMX_EXIT_REASON_VMCLEAR                           0x00000013U
/**
 * @brief VM exit reason field for VMLAUNCH instruction.
 */
#define VMX_EXIT_REASON_VMLAUNCH                          0x00000014U
/**
 * @brief VM exit reason field for VMPTRLD instruction.
 */
#define VMX_EXIT_REASON_VMPTRLD                           0x00000015U
/**
 * @brief VM exit reason field for VMPTRST instruction.
 */
#define VMX_EXIT_REASON_VMPTRST                           0x00000016U
/**
 * @brief VM exit reason field for VMREAD instruction.
 */
#define VMX_EXIT_REASON_VMREAD                            0x00000017U
/**
 * @brief VM exit reason field for VMRESUME instruction.
 */
#define VMX_EXIT_REASON_VMRESUME                          0x00000018U
/**
 * @brief VM exit reason field for VMWRITE instruction.
 */
#define VMX_EXIT_REASON_VMWRITE                           0x00000019U
/**
 * @brief VM exit reason field for VMXOFF instruction.
 */
#define VMX_EXIT_REASON_VMXOFF                            0x0000001AU
/**
 * @brief VM exit reason field for VMXON instruction.
 */
#define VMX_EXIT_REASON_VMXON                             0x0000001BU
/**
 * @brief VM exit reason field for control register accesses.
 */
#define VMX_EXIT_REASON_CR_ACCESS                         0x0000001CU
/**
 * @brief VM exit reason field for MOV DR instruction.
 */
#define VMX_EXIT_REASON_DR_ACCESS                         0x0000001DU
/**
 * @brief VM exit reason field for I/O instruction.
 */
#define VMX_EXIT_REASON_IO_INSTRUCTION                    0x0000001EU
/**
 * @brief VM exit reason field for RDMSR instruction.
 */
#define VMX_EXIT_REASON_RDMSR                             0x0000001FU
/**
 * @brief VM exit reason field for WRMSR instruction.
 */
#define VMX_EXIT_REASON_WRMSR                             0x00000020U
/**
 * @brief VM exit reason field for VM entry failure due to invalid guest state.
 */
#define VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE 0x00000021U
/**
 * @brief VM exit reason field for VM entry failure due to MSR loading.
 */
#define VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING         0x00000022U
/**
 * @brief VM exit reason 35 for reserved.
 */
#define VMX_EXIT_REASON_35_RESERVED         0x00000023U
/**
 * @brief VM exit reason field for MWAIT instruction.
 */
#define VMX_EXIT_REASON_MWAIT        0x00000024U
/**
 * @brief VM exit reason field for monitor trap flag.
 */
#define VMX_EXIT_REASON_MONITOR_TRAP 0x00000025U
/**
 * @brief VM exit reason 38 for reserved.
 */
#define VMX_EXIT_REASON_38_RESERVED         0x00000026U
/**
 * @brief VM exit reason field for MONITOR instruction.
 */
#define VMX_EXIT_REASON_MONITOR                     0x00000027U
/**
 * @brief VM exit reason field for PAUSE instruction.
 */
#define VMX_EXIT_REASON_PAUSE                       0x00000028U
/**
 * @brief VM exit reason field for VM entry failure due to machine-check event.
 */
#define VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK 0x00000029U
/**
 * @brief VM exit reason 42 for reserved.
 */
#define VMX_EXIT_REASON_42_RESERVED         0x0000002AU
/**
 * @brief VM exit reason field for TPR below threshold.
 */
#define VMX_EXIT_REASON_TPR_BELOW_THRESHOLD          0x0000002BU
/**
 * @brief VM exit reason field for APIC access.
 */
#define VMX_EXIT_REASON_APIC_ACCESS                  0x0000002CU
/**
 * @brief VM exit reason field for virtual EOI.
 */
#define VMX_EXIT_REASON_VIRTUALIZED_EOI              0x0000002DU
/**
 * @brief VM exit reason field for access to GDTR or IDTR.
 */
#define VMX_EXIT_REASON_GDTR_IDTR_ACCESS             0x0000002EU
/**
 * @brief VM exit reason field for access to LDTR or TR.
 */
#define VMX_EXIT_REASON_LDTR_TR_ACCESS               0x0000002FU
/**
 * @brief VM exit reason field for EPT violation.
 */
#define VMX_EXIT_REASON_EPT_VIOLATION                0x00000030U
/**
 * @brief VM exit reason field for EPT misconfiguration.
 */
#define VMX_EXIT_REASON_EPT_MISCONFIGURATION         0x00000031U
/**
 * @brief VM exit reason field for INVEPT instruction.
 */
#define VMX_EXIT_REASON_INVEPT                       0x00000032U
/**
 * @brief VM exit reason field for RDTSCP instruction.
 */
#define VMX_EXIT_REASON_RDTSCP                       0x00000033U
/**
 * @brief VM exit reason field for VMX-preemption timer expired.
 */
#define VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED 0x00000034U
/**
 * @brief VM exit reason field for INVVPID instruction.
 */
#define VMX_EXIT_REASON_INVVPID                      0x00000035U
/**
 * @brief VM exit reason field for WBINVD instruction.
 */
#define VMX_EXIT_REASON_WBINVD                       0x00000036U
/**
 * @brief VM exit reason field for XSETBV instruction.
 */
#define VMX_EXIT_REASON_XSETBV                       0x00000037U
/**
 * @brief VM exit reason field for APIC write.
 */
#define VMX_EXIT_REASON_APIC_WRITE                   0x00000038U
/**
 * @brief VM exit reason field for RDRAND instruction.
 */
#define VMX_EXIT_REASON_RDRAND                       0x00000039U
/**
 * @brief VM exit reason field for INVPCID instruction.
 */
#define VMX_EXIT_REASON_INVPCID                      0x0000003AU
/**
 * @brief VM exit reason field for VMFUNC instruction.
 */
#define VMX_EXIT_REASON_VMFUNC                       0x0000003BU
/**
 * @brief VM exit reason field for ENCLS instruction.
 */
#define VMX_EXIT_REASON_ENCLS                        0x0000003CU
/**
 * @brief VM exit reason field for RDSEED instruction.
 */
#define VMX_EXIT_REASON_RDSEED                       0x0000003DU
/**
 * @brief VM exit reason field for page modification log is full.
 */
#define VMX_EXIT_REASON_PAGE_MODIFICATION_LOG_FULL   0x0000003EU
/**
 * @brief VM exit reason field for XSAVES instruction.
 */
#define VMX_EXIT_REASON_XSAVES                       0x0000003FU
/**
 * @brief VM exit reason field for XRSTORS instruction.
 */
#define VMX_EXIT_REASON_XRSTORS                      0x00000040U

/**
 * @brief Bit field in the pin-based VM execution controls that indicates external interrupts cause VM exits.
 */
#define VMX_PINBASED_CTLS_IRQ_EXIT     (1U << 0U)
/**
 * @brief Bit field in the pin-based VM execution controls whether a VM exit occurs
 * at the beginning of any instruction if RFLAGS.IF = 1 and there are no other blocking
 * of interrupts.
 */
#define VMX_PROCBASED_CTLS_IRQ_WIN     (1U << 2U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of RDTSC, executions of RDTSCP and executions of RDMSR that read from
 * the IA32_TIME_STAMP_COUNTER MSR return a value modified by the TSC offset field.
 */
#define VMX_PROCBASED_CTLS_TSC_OFF     (1U << 3U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of INVLPG cause VM exits.
 */
#define VMX_PROCBASED_CTLS_INVLPG      (1U << 9U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MWAIT cause VM exits.
 */
#define VMX_PROCBASED_CTLS_MWAIT       (1U << 10U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of RDPMC cause VM exits.
 */
#define VMX_PROCBASED_CTLS_RDPMC       (1U << 11U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MOV to CR3 cause VM exits.
 */
#define VMX_PROCBASED_CTLS_CR3_LOAD    (1U << 15U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MOV from CR3 cause VM exits.
 */
#define VMX_PROCBASED_CTLS_CR3_STORE   (1U << 16U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MOV to CR8 cause VM exits.
 */
#define VMX_PROCBASED_CTLS_CR8_LOAD    (1U << 19U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MOV from CR8 cause VM exits.
 */
#define VMX_PROCBASED_CTLS_CR8_STORE   (1U << 20U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that enables
 * TPR virtualization and other APIC virtualization features.
 */
#define VMX_PROCBASED_CTLS_TPR_SHADOW  (1U << 21U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MOV DR cause VM exits.
 */
#define VMX_PROCBASED_CTLS_MOV_DR      (1U << 23U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether I/O bitmaps are used to restrict executions of I/O instruction.
 */
#define VMX_PROCBASED_CTLS_IO_BITMAP   (1U << 25U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether MSR bitmaps are used to control execution of the RDMSR and WRMSR instructions.
 */
#define VMX_PROCBASED_CTLS_MSR_BITMAP  (1U << 28U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether executions of MONITOR cause VM exits.
 */
#define VMX_PROCBASED_CTLS_MONITOR     (1U << 29U)
/**
 * @brief Bit field in the primary processor-based VM execution controls that determines
 * whether the secondary processor-based VM execution controls are used.
 */
#define VMX_PROCBASED_CTLS_SECONDARY   (1U << 31U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that lets
 * the logical processor treats specially accesses to the page with the APIC access address.
 */
#define VMX_PROCBASED_CTLS2_VAPIC      (1U << 0U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that enables EPT.
 */
#define VMX_PROCBASED_CTLS2_EPT        (1U << 1U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that determines
 * whether any execution of RDTSCP causes an invalid opcode exception.
 */
#define VMX_PROCBASED_CTLS2_RDTSCP     (1U << 3U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that determines
 * whether cached translations of linear addresses are associated with a VPID.
 */
#define VMX_PROCBASED_CTLS2_VPID       (1U << 5U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that determines
 * whether executions of WBIND cause VM exits.
 */
#define VMX_PROCBASED_CTLS2_WBINVD     (1U << 6U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that determines
 * whether guest software may run in unpaged protected or in real address mode.
 */
#define VMX_PROCBASED_CTLS2_UNRESTRICT (1U << 7U)
/**
 * @brief Bit field in the secondary processor-based VM execution controls that determines
 * whether execution of XSAVES or XRSTORS causes a \# UD.
 */
#define VMX_PROCBASED_CTLS2_XSVE_XRSTR (1U << 20U)

/**
 * @brief Bit field in the IA32_VMX_EPT_VPID_CAP MSR that determines
 * whether the single context INVEPT type is supported.
 */
#define VMX_EPT_INVEPT_SINGLE_CONTEXT (1U << 25U)
/**
 * @brief Bit field in the IA32_VMX_EPT_VPID_CAP MSR that determines
 * whether the all context INVEPT type is supported.
 */
#define VMX_EPT_INVEPT_GLOBAL_CONTEXT (1U << 26U)

/**
 * @brief Single context INVVPID type which indicates only specified VPID's mappings will be invalidated.
 */
#define VMX_VPID_TYPE_SINGLE_CONTEXT 1UL
/**
 * @brief Single context INVVPID type which indicates all VPIDs' mappings will be invalidated.
 */
#define VMX_VPID_TYPE_ALL_CONTEXT    2UL

/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * DR7 and the IA32_DEBUGCTL MSR are saved on VM exit.
 */
#define VMX_EXIT_CTLS_SAVE_DEBUGCTL (1U << 2U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * a logical processor is in 64-bit mode after the next VM exit.
 */
#define VMX_EXIT_CTLS_HOST_ADDR64 (1U << 9U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * a logical processor acknowledge the external interrupt on VM exit.
 */
#define VMX_EXIT_CTLS_ACK_IRQ     (1U << 15U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * IA32_PAT MSR is saved on VM exit.
 */
#define VMX_EXIT_CTLS_SAVE_PAT    (1U << 18U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * IA32_PAT MSR is loaded on VM exit.
 */
#define VMX_EXIT_CTLS_LOAD_PAT    (1U << 19U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * IA32_EFER MSR is saved on VM exit.
 */
#define VMX_EXIT_CTLS_SAVE_EFER   (1U << 20U)
/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * IA32_EFER MSR is loaded on VM exit.
 */
#define VMX_EXIT_CTLS_LOAD_EFER   (1U << 21U)

/**
 * @brief Bit field in the IA32_VMX_EXIT_CTLS MSR that determines whether
 * DR7 and the IA32_DEBUGCTL MSR are loaded on VM entry.
 */
#define VMX_ENTRY_CTLS_LOAD_DEBUGCTL (1U << 2U)
/**
 * @brief Bit field in the IA32_VMX_ENTRY_CTLS MSR that determines whether
 * a logical processor is in IA-32e mode after VM entry.
 */
#define VMX_ENTRY_CTLS_IA32E_MODE (1U << 9U)
/**
 * @brief Bit field in the IA32_VMX_ENTRY_CTLS MSR that determines whether
 * the IA32_PAT MSR is loaded on VM entry.
 */
#define VMX_ENTRY_CTLS_LOAD_PAT   (1U << 14U)
/**
 * @brief Bit field in the IA32_VMX_ENTRY_CTLS MSR that determines whether
 * the IA32_EFER MSR is loaded on VM entry.
 */
#define VMX_ENTRY_CTLS_LOAD_EFER  (1U << 15U)

/**
 * @brief Bit field in VM exit interrupt information that indicates
 * the VM exit is caused by a hardware exception that would have delivered
 * an error code on the stack.
 */
#define VMX_INT_INFO_ERR_CODE_VALID (1U << 11U)
/**
 * @brief Bit field in VM exit interrupt information that indicates
 * this interrupt information is valid.
 */
#define VMX_INT_INFO_VALID          (1U << 31U)
/**
 * @brief Bit mask in VM exit interrupt information that indicates the interrupt type.
 */
#define VMX_INT_TYPE_MASK           (0x700U)
/**
 * @brief Interrupt type in VM exit interrupt information indicates this is an external interrupt.
 */
#define VMX_INT_TYPE_EXT_INT        0U
/**
 * @brief Interrupt type in VM exit interrupt information indicates this is an NMI.
 */
#define VMX_INT_TYPE_NMI            2U
/**
 * @brief Interrupt type in VM exit interrupt information indicates this is a hardware exception.
 */
#define VMX_INT_TYPE_HW_EXP         3U

void vmx_on(void);
void vmx_off(void);

uint32_t exec_vmread32(uint32_t field);
uint64_t exec_vmread64(uint32_t field_full);
/**
 * @brief Alias for exec_vmread64
 *
 * The input field parameter shall be a valid natural-width VMCS component field encoding
 * defined in Appendix B, Vol.3, SDM.
 */
#define exec_vmread exec_vmread64

void exec_vmwrite16(uint32_t field, uint16_t value);
void exec_vmwrite32(uint32_t field, uint32_t value);
void exec_vmwrite64(uint32_t field_full, uint64_t value);
/**
 * @brief Alias for exec_vmwrite64
 *
 * The input field parameter shall be a valid natural-width VMCS component field encoding
 * defined in Appendix B, Vol.3, SDM.
 */
#define exec_vmwrite exec_vmwrite64

void exec_vmclear(uint64_t addr);
void exec_vmptrld(uint64_t addr);

/**
 * @}
 */

#endif /* VMX_H_ */
