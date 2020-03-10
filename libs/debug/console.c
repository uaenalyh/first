/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <shell.h>
#include <timer.h>
#include <irq.h>
#include <timer.h>
#include <logmsg.h>
#include <acrn_hv_defs.h>
#include <vm.h>
#include <vmx.h>
#include <vmexit.h>
#include "vuart.h"
#include "uart16550_priv.h"
#include "lib.h"

#define CONSOLE_KICK_TIMER_TIMEOUT  40UL /* timeout is 40ms*/

#define MSR_IA32_VMX_MISC_VMX_PREEMPTION_DIVISOR   0x1FUL

#define VMX_PINBASED_CTLS_ACTIVATE_VMX_PREEMPTION_TIMER     (1U << 6)

#define VMX_EXIT_CTLS_SAVE_PTMR	(1U << 22U)

#define VMX_GUEST_VMX_PREEMPTION_TIMER_VALUE	0x0000482EU

extern struct vm_exit_dispatch dispatch_table[];

#define CONSOLE_CPU_ID    3
/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */
uint16_t console_vmid = ACRN_INVALID_VMID;

static uint32_t vmx_preemption_timer_value = 0;

void console_init(void)
{
	uart16550_init(false);
}

void console_putc(const char *ch)
{
	(void)uart16550_puts(ch, 1U);
}


size_t console_write(const char *s, size_t len)
{
	return  uart16550_puts(s, len);
}

char console_getc(void)
{
	return uart16550_getc();
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
void vuart_console_rx_chars(struct acrn_vuart *vu)
{
	char ch = -1;

	/* Get data from physical uart */
	ch = uart16550_getc();

	if (ch == GUEST_CONSOLE_TO_HV_SWITCH_KEY) {
		/* Switch the console */
		console_vmid = ACRN_INVALID_VMID;
		printf("\r\n\r\n ---Entering ACRN SHELL---\r\n");
	}
	if (ch != -1) {
		vuart_putchar(vu, ch);
	}

}

/**
 * @pre vu != NULL
 */
void vuart_console_tx_chars(struct acrn_vuart *vu)
{
	char c;

	while ((c = vuart_getchar(vu)) != -1) {
		printf("%c", c);
	}
}

struct acrn_vuart *vuart_console_active(void)
{
	struct acrn_vm *vm = NULL;
	struct acrn_vuart *vu = NULL;

	if (console_vmid < CONFIG_MAX_VM_NUM) {
		vm = get_vm_from_vmid(console_vmid);
		if (vm->state != VM_POWERED_OFF) {
			vu = vm_console_vuart(vm);
		}
	}

	return ((vu != NULL) && vu->active) ? vu : NULL;
}

void console_kick(void)
{
	if (get_pcpu_id() == CONSOLE_CPU_ID) {
		struct acrn_vuart *vu;

		/* Kick HV-Shell and Uart-Console tasks */
		vu = vuart_console_active();
		if (vu != NULL) {
			/* serial Console Rx operation */
			vuart_console_rx_chars(vu);
			/* serial Console Tx operation */
			vuart_console_tx_chars(vu);
		} else {
			shell_kick();
		}
	}
}

int32_t vmx_preemption_timer_expired_handler(struct acrn_vcpu *vcpu)
{
	console_kick();
	exec_vmwrite(VMX_GUEST_VMX_PREEMPTION_TIMER_VALUE, vmx_preemption_timer_value);
	vcpu_retain_rip(vcpu);
	return 0;
}

void console_setup_timer(void)
{
	if (get_pcpu_id() == CONSOLE_CPU_ID) {
		uint64_t ia32_vmx_misc;
		uint32_t exec_ctrl, exit_ctrl, vmx_preemption_divisor;

		ia32_vmx_misc = msr_read(MSR_IA32_VMX_MISC);
		vmx_preemption_divisor = 1U << (ia32_vmx_misc & MSR_IA32_VMX_MISC_VMX_PREEMPTION_DIVISOR);
		vmx_preemption_timer_value = us_to_ticks(CONSOLE_KICK_TIMER_TIMEOUT * 1000) / vmx_preemption_divisor;

		exec_ctrl = exec_vmread32(VMX_PIN_VM_EXEC_CONTROLS);
		exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, exec_ctrl | VMX_PINBASED_CTLS_ACTIVATE_VMX_PREEMPTION_TIMER);
		exit_ctrl = exec_vmread32(VMX_EXIT_CONTROLS);
		exec_vmwrite32(VMX_EXIT_CONTROLS, exit_ctrl | VMX_EXIT_CTLS_SAVE_PTMR);
		exec_vmwrite(VMX_GUEST_VMX_PREEMPTION_TIMER_VALUE, vmx_preemption_timer_value);

		dispatch_table[VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED].handler = vmx_preemption_timer_expired_handler;
	}
}

void suspend_console(void)
{
	msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
}

void resume_console(void)
{
	msr_write(MSR_IA32_TSC_DEADLINE, rdtsc() + us_to_ticks(5000));
}
