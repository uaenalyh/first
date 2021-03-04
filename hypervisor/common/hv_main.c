/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vm_reset.h>
#include <vmcs.h>
#include <vmexit.h>
#include <irq.h>
#include <schedule.h>
#include <profiling.h>
#include <trace.h>
#include <logmsg.h>
#include <console.h>

/**
 * @defgroup vp-base_hv-main vp-base.hv-main
 * @ingroup vp-base
 * @brief  The hv_main module provides thread functions when a physical processor has or has not an
 * assigned vCPU to run.
 *
 * The hv_main module mainly contains three c files: hv_mian.c which describes the thread functions,
 * vmexit.c which introduces all
 * these VM-exit handler and vmcs.c which introduces all these VMCS related operations especially
 * VMCS initialization.
 *
 * Usage:
 * - 'vp-base.vcpu' module depends on this module to set up thread entry for the specified vCPU.
 * - 'init' module depends on this module to run the idle thread for primary and secondary physical CPU.
 * - 'vp-base.virq' module depends on this module to initialize the VMCS fields for the specified vCPU.
 * - 'vp-base.vcr' module depends on this module to get the VM-exit qualification related information.
 *
 * Dependency:
 * - This module depends on 'lib.util' module to find out the pointer to the vcpu structure where
 * the given thread_object structure is embedded in.
 * - This module depends on 'hwmgmt.schedule' module to check if the pCPU dedicated on the vCPU needs
 * to be rescheduled and schedule the pCPU if necessary.
 * - This module depends on 'hwmgmt.schedule' module to set the thread to idle.
 * - This module depends on 'hwmgmt.cpu' module to check if the pCPU needs to be offline and halt the
 * pCPU if necessary .
 * - This module depends on 'hwmgmt.cpu' module to enable or disable the IRQ and pause the target
 * pCPU if necessary.
 * - This module depends on 'hwmgmt.cpu' module to get id of the target pCPU.
 * - This module depends on 'hwmgmt.cpu' module to read the target MSRs.
 * - This module depends on 'hwmgmt.cpu' module to write to XCR register.
 * - This module depends on 'vp-base.vm' module to check if the VM dedicated on the pCPU needs
 * to be shutdown and shutdown the vm if necessary .
 * - This module depends on 'vp-base.virq' module to handle the pending request.
 * - This module depends on 'vp-base.virq' module to inject exceptions such as GP, UD to the
 * target vCPU.
 * - This module depends on 'vp-base.virq' module to submit a specified request to the target vCPU.
 * - This module depends on 'debug' module to log some information and trace some events in debug phase.
 * - This module depends on 'vp-base.vcpu' module to get the pcpuid of the target vCPU.
 * - This module depends on 'vp-base.vcpu' module to handle vcpu related operations, such
 * as run vcpu, pause vcpu and retain RIP.
 * - This module depends on 'vp-base.vcpu' module to get or set the general purpose
 * registers of the target vCPU, such as EAX, EBX, ECX, EDX.
 * - This module depends on 'vp-base.vcpuid' module to emulate the CPUID instruction.
 * - This module depends on 'hwmgmt.vmx' module to read from or write to the VMCS fields.
 * - This module depends on 'hwmgmt.vmx' module to execute to the VMCS related operations such as VMPTRLD.
 * - This module depends on 'vp-base.vm' module to check if the specified VM is a safety VM or not.
 * - This module depends on 'vp-base.vm' module to do partial shutdown operation on the vm which
 * vcpu belongs when fatal error happens.
 * - This module depends on 'vp-base.guest_mem' module to flush the cache derived from EPT.
 *
 * @{
 */

/**
 * @file
 * @brief This file describes the hv_main thread relevant operations.
 *
 * These operations implements hypervisor main thread and external interfaces to handle idle
 * thread for init module.
 *
 */

/**
 * @brief This function is used to handle threads of vcpu such as init vmcs, handle pending
 * request, handle softirq, dispatch VM exit handlers and so on. Besides, it will also
 * report information when error happens.
 *
 * @param[in] obj A pointer which points to a thread to be handled.
 *
 * @return None
 *
 * @pre \a obj shall point to the field sched_obj field of an instance of type acrn_vcpu
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy unspecified
 *
 * @threadsafety when \a obj is different among parallel invocation
 */
void vcpu_thread(struct thread_object *obj)
{
	/** Declare the following local variables of type struct acrn_vcpu *.
	 *  - vcpu representing pointer which points to the vcpu structure where \a obj is embedded in,
	 *  initialized as list_entry(obj, struct acrn_vcpu, thread_obj).
	 */
	struct acrn_vcpu *vcpu = list_entry(obj, struct acrn_vcpu, thread_obj);
	/** Declare the following local variables of type uint32_t.
	 *  - basic_exit_reason representing vm exit reason, initialized as 0.
	 */
	uint32_t basic_exit_reason = 0U;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing the error code of the to-be-conducted operations,
	 *  initialized as 0.
	 */
	int32_t ret = 0;
	/** Until true */
	do {
		/** If return value of need_reschedule(pcpuid_from_vcpu(vcpu)) is true */
		if (need_reschedule(pcpuid_from_vcpu(vcpu))) {
			/** Call schedule() to schedule the threads associated
			 *  with the current running physical CPU.
			 */
			schedule();
		}
		/** Set ret to return value of acrn_handle_pending_request(vcpu). */
		ret = acrn_handle_pending_request(vcpu);
		/** If 'ret' is smaller than 0, indicating that error happened during
		 *  handling pending request */
		if (ret < 0) {
			/** If the VM associated with the given \a vcpu is safety vm */
			if (is_safety_vm(vcpu->vm)) {
				/** Call panic with the following parameters, in order to print error information
				 *  and panic the corresponding pcpu.
				 *  - "vcpu handling pending request fail" */
				panic("vcpu handling pending request fail");
			} else {
				/** Logging the following information with a log level of LOG_FATAL.
				 *  - "vcpu handling pending request fail"
				 */
				pr_fatal("vcpu handling pending request fail");

				/** Call fatal_error_shutdown_vm() with the following parameters, in order to
				 *  shutdown the corresponding vm.
				 *  - vcpu
				 */
				fatal_error_shutdown_vm(vcpu);
			}

			/** Continue to next iteration */
			continue;
		}
		/** Call profiling_vmenter_handler() with the following parameters, in order
		 *  to profile the information of \a vcpu before a VM entry.
		 *  - vcpu
		 */
		profiling_vmenter_handler(vcpu);

		/** Call TRACE_2L() with the following parameters, in order to trace
		 *  VM enter event for debug purpose.
		 *  - TRACE_VM_ENTER
		 *  - 0UL
		 *  - 0UL */
		TRACE_2L(TRACE_VM_ENTER, 0UL, 0UL);
		/** Set ret to return value of run_vcpu(vcpu) */
		ret = run_vcpu(vcpu);
		/** If 'ret' is not 0, indicating that error happened when handling run_vcpu()  */
		if (ret != 0) {
			/** If the VM associated with the given \a vcpu is safety vm */
			if (is_safety_vm(vcpu->vm)) {
				/** Call panic with the following parameters, in order to print error information
				 *  and panic the corresponding pcpu.
				 *  - "vcpu resume failed" */
				panic("vcpu resume failed");
			} else {
				/** Logging the following information with a log level of LOG_FATAL.
				 *  - "vcpu resume failed"
				 */
				pr_fatal("vcpu resume failed");

				/** Call fatal_error_shutdown_vm() with the following parameters, in order to
				 *  shutdown the corresponding vm.
				 *  - vcpu
				 */
				fatal_error_shutdown_vm(vcpu);
			}

			/** Continue to next iteration */
			continue;
		}
		/** Set basic_exit_reason to low 16 bits of 'vcpu->arch.exit_reason'. */
		basic_exit_reason = vcpu->arch.exit_reason & 0xFFFFU;
		/** Call TRACE_2L() with the following parameters, in order to trace
		 *  VM-exit event for debug purpose.
		 *  - TRACE_VM_EXIT
		 *  - basic_exit_reason
		 *  - vcpu_get_rip(vcpu) */
		TRACE_2L(TRACE_VM_EXIT, basic_exit_reason, vcpu_get_rip(vcpu));

		/** Increment vcpu->arch.nrexits by 1 */
		vcpu->arch.nrexits++;

		/** Call profiling_pre_vmexit_handler() wth the following parameters, in order
		 *  to profile the information of \a vcpu before a VM exit.
		 *  - vcpu */
		profiling_pre_vmexit_handler(vcpu);
		/** Set ret to return value of vmexit_handler(vcpu). */
		ret = vmexit_handler(vcpu);
		/** If 'ret' is smaller than 0, indicating that error happened during vm exit */
		if (ret < 0) {
			/** Logging the following information with a log level of LOG_FATAL.
			 *  - basic_exit_reason
			 *  - ret */
			pr_fatal("dispatch VM exit handler failed for reason"
				 " %d, ret = %d!",
				basic_exit_reason, ret);
			/** Call vcpu_inject_gp() with the following parameters, in order to inject
			 *  gp to the vcpu with error code 0.
			 *  - vcpu
			 *  - O */
			vcpu_inject_gp(vcpu, 0U);
			/** Continue to next iteration */
			continue;
		}

		/** Call profiling_post_vmexit_handler() with the following parameters, in order
		 *  to profile the information of \a vcpu after a VM exit.
		 *  - vcpu */
		profiling_post_vmexit_handler(vcpu);
	} while (1);
}

/**
 * @brief This function is to run vcpu in idle state.
 *
 * @param[in] obj A pointer which points to a thread to be handled.
 * This unused parameter is required to be present as the prototype of this function shall
 * comply with the type 'thread_entry_t'.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark The function shall be called after run_idle_thread().
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
void default_idle(__unused struct thread_object *obj)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing the ID of the current running physical CPU,
	 *  initialized as get_pcpu_id().
	 */
	uint16_t pcpu_id = get_pcpu_id();

	/** while true */
	while (1) {
		/** If return value of need_reschedule(pcpu_id) is true */
		if (need_reschedule(pcpu_id)) {
			/** Call schedule() to schedule the threads associated
			 *  with the current running physical CPU.
			 */
			schedule();
		/** If return value of need_offline(pcpu_id) is true. */
		} else if (need_offline(pcpu_id)) {
			/** Call cpu_dead() to halt the current running physical CPU. */
			cpu_dead();
		/** If return value of need_shutdown_vm(pcpu_id) is true. */
		} else if (need_shutdown_vm(pcpu_id)) {
			/** Call shutdown_vm_from_idle() with the following parameters, in order
			 *  to shutdown the VM whose vm id equals to the value of shutdown_vm_id field of the
			 *  per-CPU region of physical CPU whose id is pcpu_id.
			 *  - pcpu_id
			 */
			shutdown_vm_from_idle(pcpu_id);
		} else {
			/** Call cpu_do_idle() to pause the current running physical CPU. */
			cpu_do_idle();
			/** Call console_kick() to kick the console tasks. */
			console_kick();
		}
	}
}

/**
 * @brief This function is to run the idle thread on the current running physical CPU.
 *
 * @return None
 *
 * @pre None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark None
 *
 * @reentrancy Unspecified
 *
 * @threadsafety When the return value of get_pcpu_id() is different among parallel invocation.
 */
void run_idle_thread(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing id of the current physical CPU, initialized as get_pcpu_id(). */
	uint16_t pcpu_id = get_pcpu_id();
	/** Declare the following local variables of type struct thread_object *.
	 *  - idle representing pointer which points to the idle thread, initialized as
	 *  &per_cpu(idle, pcpu_id). */
	struct thread_object *idle = &per_cpu(idle, pcpu_id);

	/** Set idle->pcpu_id to pcpu_id. */
	idle->pcpu_id = pcpu_id;
	/** Set idle->thread_entry to default_idle. */
	idle->thread_entry = default_idle;
	/** Set idle->switch_out to NULL. */
	idle->switch_out = NULL;
	/** Set idle->switch_in to NULL. */
	idle->switch_in = NULL;

	/** Call run_thread() with the following parameters, in order to run the idle thread pointed by 'idle'.
	 *  - idle */
	run_thread(idle);

	/** Call cpu_dead() to halt the current running physical CPU. */
	cpu_dead();
}

/**
 * @}
 */
