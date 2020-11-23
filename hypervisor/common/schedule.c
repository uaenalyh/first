/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bits.h>
#include <cpu.h>
#include <per_cpu.h>
#include <lapic.h>
#include <schedule.h>

/**
 * @defgroup hwmgmt_schedule hwmgmt.schedule
 * @ingroup hwmgmt
 * @brief The definition and implementation of a scheduler neutral framework.
 *
 * It provides external APIs for manipulating scheduler and thread as well as
 * private helper functions to implement those public APIs.
 *
 * Usage:
 * - 'vp-base.vcpu' depends on this module to get physical CPU ID corresponding to the specified thread.
 * - 'hwmgmt.cpu' depends on this module to manipulate scheduler and thread.
 * - 'vp-base.hv_main' depends on this module to schedule threads.
 *
 * Dependency:
 * - This module depends on hwmgmt.cpu module to operate 'per_cpu_region' and get ID of current physical CPU.
 * - This module depends on lib.lock module to obtain and release spinlock.
 * - This module depends on hwmgmt.apic module to send interrupt signal.
 * - This module depends on lib.bits module to operate 'NEED_RESCHEDULE' bit.
 *
 * @{
 */

/**
 * @file arch/x86/sched.S
 *
 * @brief This file defines the context switch sub-routine
 *
 * This file provides one single function, namely arch_switch_to, that does a switch of thread contexts. Taking a stack
 * pointer of the current thread and another of the target thread as inputs, this function saves the current registers
 * on the stack of the current thread and restores the values on the target thread's stack.
 *
 * For the detailed steps during a context switch, refer to section 5.6 in the Software Architecture Design
 * Specification. For details on the behavior of this function, refer to the control flow diagram in section 11.3.13.5.8
 * in the Software Architecture Design Specification.
 */

/**
 * @file
 * @brief This file contains the data structures and functions for the hwmgmt.schedule module.
 *
 * This file implements all following external APIs that shall be provided
 * by the hwmgmt.schedule module.
 * - sched_get_pcpuid
 * - init_sched
 * - deinit_sched
 * - init_thread_data
 * - need_reschedule
 * - schedule
 * - sleep_thread
 * - wake_thread
 * - kick_thread
 * - run_thread
 *
 * This file also implements following helper functions to help realizing the features of
 * the external APIs.
 * - is_blocked
 * - is_runnable
 * - is_running
 * - set_thread_status
 * - obtain_schedule_lock
 * - release_schedule_lock
 * - get_scheduler
 * - make_reschedule_request
 */

/**
 * @brief The interface checks whether the specified thread is in the blocked state or not.
 *
 * @param[in] obj The specified thread whose state is to be checked.
 *
 * @return A Boolean value indicating whether the specified thread is in the blocked state or not.
 *
 * @retval true The specified thread is in the blocked state.
 * @retval false The specified thread is not in the blocked state.
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_blocked(const struct thread_object *obj)
{
	/** Check the 'status' field of thread \a obj,
	 *  If the value is 'THREAD_STS_BLOCKED', return true.
	 *  Otherwise, return false.
	 */
	return obj->status == THREAD_STS_BLOCKED;
}

/**
 * @brief The interface checks whether the specified thread is in the runnable state or not.
 *
 * @param[in] obj The specified thread whose state is to be checked.
 *
 * @return A Boolean value indicating whether the specified thread is in the runnable state or not.
 *
 * @retval true The specified thread is in the runnable state.
 * @retval false The specified thread is not in the runnable state.
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_runnable(const struct thread_object *obj)
{
	/** Check the 'status' field of thread \a obj,
	 *  If the value is 'THREAD_STS_RUNNABLE', return true.
	 *  Otherwise, return false.
	 */
	return obj->status == THREAD_STS_RUNNABLE;
}

/**
 * @brief The interface checks whether the specified thread is in the running state or not.
 *
 * @param[in] obj The specified thread whose state is to be checked.
 *
 * @return A Boolean value indicating whether the specified thread is in the running state or not.
 *
 * @retval true The specified thread is in the running state.
 * @retval false The specified thread is not in the running state.
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline bool is_running(const struct thread_object *obj)
{
	/** Check the 'status' field of thread \a obj,
	 *  If the value is 'THREAD_STS_RUNNING', return true.
	 *  Otherwise, return false.
	 */
	return obj->status == THREAD_STS_RUNNING;
}

/**
 * @brief The interface sets the specified thread state.
 *
 * @param[inout] obj The specified thread whose state is to be set.
 * @param[in] status The specified thread state to be set.
 *
 * @return None
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a obj is different among parallel invocation.
 */
static inline void set_thread_status(struct thread_object *obj, enum thread_object_state status)
{
	/** Set the 'status' field of thread \a obj to \a status */
	obj->status = status;
}

/**
 * @brief The interface obtains scheduler lock corresponding to the specified physical CPU.
 *
 * Correspondence between physical CPU and scheduler lock is 1:1.
 *
 * The scheduler lock is used to protect scheduler control block and thread.
 *
 * @param[in] pcpu_id The ID of the physical CPU corresponding to the scheduler lock.
 * @param[out] rflag The pointer which points to an address that saves the value of RFLAGS register.
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void obtain_schedule_lock(uint16_t pcpu_id, uint64_t *rflag)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	/** Call spinlock_irqsave_obtain with the following parameters, in order to
	 *  obtain the scheduler lock corresponding to scheduler control block 'ctl'
	 *  and save RFLAGS register value to pointer \a rflag.
	 *  - &ctl->scheduler_lock
	 *  - rflag
	 */
	spinlock_irqsave_obtain(&ctl->scheduler_lock, rflag);
}

/**
 * @brief The interface releases scheduler lock corresponding to the specified physical CPU.
 *
 * Correspondence between physical CPU and scheduler lock is 1:1.
 *
 * The scheduler lock is used to protect scheduler control block and thread.
 *
 * @param[in] pcpu_id The ID of the physical CPU corresponding to the scheduler lock.
 * @param[in] rflag The value to restore to RFLAGS register.
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void release_schedule_lock(uint16_t pcpu_id, uint64_t rflag)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	/** Call spinlock_irqrestore_release with the following parameters, in order to
	 *  release the scheduler lock corresponding to scheduler control block 'ctl'
	 *  and restore the value of \a rflag to RFLAGS register.
	 *  - &ctl->scheduler_lock
	 *  - rflag
	 */
	spinlock_irqrestore_release(&ctl->scheduler_lock, rflag);
}

/**
 * @brief The interface gets scheduler corresponding to the specified physical CPU.
 *
 * @param[in] pcpu_id The ID of the physical CPU whose scheduler to be obtained.
 *
 * @return A pointer which points to the obtained scheduler.
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static struct acrn_scheduler *get_scheduler(uint16_t pcpu_id)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	/** Return the 'scheduler' field of scheduler control block 'ctl' */
	return ctl->scheduler;
}

/**
 * @brief The interface gets physical CPU ID of the specified thread object.
 *
 * @param[in] obj The thread object pointer which points the specified thread object.
 *
 * @return The physical CPU ID corresponding to thread \a obj.
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by \a obj->pcpu_id.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
uint16_t sched_get_pcpuid(const struct thread_object *obj)
{
	/** Return the 'pcpu_id' field of thread \a obj,
	 *  meaning return the physical CPU ID corresponding to thread \a obj. */
	return obj->pcpu_id;
}

/**
 * @brief Initialize the scheduler control block of the physical CPU specified by \a pcpu_id.
 *
 * @param[in] pcpu_id The ID of the physical CPU whose scheduler control block to be initialized.
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, pcpu_id).scheduler->init != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pcpu_id is different among parallel invocation.
 */
void init_sched(uint16_t pcpu_id)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	/** Call spinlock_init with the following parameters, in order to
	 *  initialize the spinlock of scheduler control block 'ctl'.
	 *  - &ctl->scheduler_lock */
	spinlock_init(&ctl->scheduler_lock);
	/** Set ctl->flags to 0H */
	ctl->flags = 0UL;
	/** Set ctl->curr_obj to NULL */
	ctl->curr_obj = NULL;
	/** Set ctl->pcpu_id to the specified physical CPU ID */
	ctl->pcpu_id = pcpu_id;
#ifdef CONFIG_SCHED_NOOP
	/** Set ctl->scheduler to the address of 'sched_noop' which is the default scheduler */
	ctl->scheduler = &sched_noop;
#endif
#ifdef CONFIG_SCHED_IORR
	ctl->scheduler = &sched_iorr;
#endif
	/** If ctl->scheduler->init is not NULL,
	 *  meaning 'ctl->scheduler' 'init' callback function isn't a NULL pointer */
	if (ctl->scheduler->init != NULL) {
		/** Call ctl->scheduler->init with the following parameters, in order to initialize scheduler.
		 *  - ctl */
		ctl->scheduler->init(ctl);
	}
}

/**
 * @brief The interface gets the scheduler control block of the specific CPU,
 * reset the schedule related resource if the deinit callback function in scheduler control block is not NULL.
 *
 * @param[in] pcpu_id The ID of the physical CPU whose scheduler control block to be deinitialized.
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, pcpu_id).scheduler->deinit == NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched have been called once
 * on the processor specified by \a pcpu_id.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pcpu_id is different among parallel invocation.
 */
void deinit_sched(uint16_t pcpu_id)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	/** If ctl->scheduler->deinit is not NULL,
	 *  meaning ctl->scheduler 'deinit' callback function isn't a NULL pointer */
	if (ctl->scheduler->deinit != NULL) {
		/** Call ctl->scheduler->deinit with the following parameters, in order to deinitialize scheduler.
		 *  - ctl */
		ctl->scheduler->deinit(ctl);
	}
}

/**
 * @brief The interface initializes scheduler specific data field
 * and set thread object status as THREAD_STS_BLOCKED.
 *
 * @param[inout] obj The thread object which will be initialized in the function.
 *
 * @return None
 *
 * @pre obj != NULL
 * @pre obj->pcpu_id < MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, obj->pcpu_id).scheduler->init_data == NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a obj is different among parallel invocation.
 */
void init_thread_data(struct thread_object *obj)
{
	/** Declare the following local variables of type 'struct acrn_scheduler*'.
	 *  - scheduler representing the corresponding scheduler of thread \a obj, initialized as
	 *  get_scheduler(obj->pcpu_id). */
	struct acrn_scheduler *scheduler = get_scheduler(obj->pcpu_id);
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing the value of RFLAG register, not initialized. */
	uint64_t rflag;

	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by 'obj->pcpu_id'.
	 *  - obj->pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(obj->pcpu_id, &rflag);
	/** If scheduler->init_data is not NULL,
	 *  meaning scheduler 'init_data' callback function isn't a NULL pointer */
	if (scheduler->init_data != NULL) {
		/** Call scheduler->init_data with the following parameters,
		 *  in order to initialize private data of scheduler.
		 *  - obj */
		scheduler->init_data(obj);
	}
	/** Call set_thread_status with the following parameters, in order to
	 *  set thread \a obj state as BLOCKED.
	 *  - obj
	 *  - THREAD_STS_BLOCKED */
	set_thread_status(obj, THREAD_STS_BLOCKED);
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by 'obj->pcpu_id'.
	 *  - obj->pcpu_id
	 *  - rflag */
	release_schedule_lock(obj->pcpu_id, rflag);
}

/**
 * @brief The interface makes reschedule request to the target physical CPU with specific delivery mode.
 *
 * This function makes reschedule request to the target physical CPU.
 * When sleep or kick the thread, notify thread's pinned physical CPU to reschedule.
 *
 * It is supposed to be called by 'sleep_thread' or 'kick_thread'.
 *
 * @param[in] pcpu_id The ID of the physical CPU to reschedule.
 * @param[in] delmode The reschedule request delivery mode to the target physical CPU.
 *
 * @return None
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 * @pre delmode == DEL_MODE_INIT
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pcpu_id is different among parallel invocation.
 */
void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	/** Call bitmap_set_lock with the following parameters, in order to
	 *  set 'NEED_RESCHEDULE' bit in the 'flags' field of scheduler control block 'ctl'.
	 *  - NEED_RESCHEDULE
	 *  - &ctl->flags */
	bitmap_set_lock(NEED_RESCHEDULE, &ctl->flags);
	/** If get_pcpu_id() is not pcpu_id,
	 *  meaning current physical CPU ID isn't target physical CPU ID */
	if (get_pcpu_id() != pcpu_id) {
		/** Depending on request delivery mode */
		switch (delmode) {
		/** Delivery mode is INIT signal */
		case DEL_MODE_INIT:
			/** Call send_single_init with the following parameters,
			 *  in order to notify target physical CPU.
			 *  - pcpu_id */
			send_single_init(pcpu_id);
			/** Terminate the loop */
			break;
		/** Otherwise */
		default:
			/* Only DEL_MODE_INIT is supported */

			/** Print a error message */
			pr_err("Err: Delivery mode %u for pCPU%u is not support.", delmode, pcpu_id);
			/** Terminate the loop */
			break;
		}
	}
}

/**
 * @brief The interface checks whether the specified physical CPU need reschedule or not.
 *
 * @param[in] pcpu_id The ID of the physical cpu, which schedule context's
 * flags NEED_OFFLINE will be tested and cleared.
 *
 * @return A Boolean value indicating whether the specified physical CPU need reschedule or not.
 *
 * @retval true The specified physical CPU need reschedule.
 * @retval false The specified physical needn't reschedule.
 *
 * @pre pcpu_id < CONFIG_MAX_PCPU_NUM
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by \a pcpu_id.
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pcpu_id is different among parallel invocation.
 */
bool need_reschedule(uint16_t pcpu_id)
{
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	/** Check whether 'NEED_RESCHEDULE' bit is set in the 'flags' field
	 *  of scheduler control block 'ctl'.
	 *  If yes, return true.
	 *  Otherwise, return false */
	return bitmap_test(NEED_RESCHEDULE, &ctl->flags);
}

/**
 * @brief The interface schedules threads that are pinned to the current physical CPU.
 *
 * Get the current and next schedule object of the current CPU,
 * if the current schedule is the same as the next schedule object do nothing,
 * otherwise schedule the next object and meanwhile clear the NEED_RESCHEDULE bit of the schedule control block's flags.
 * When doing the operations we should guarantee there is only one core can operate the schedule context.
 *
 * @return None
 *
 * @pre per_cpu(sched_ctl, get_pcpu_id()).curr_obj != NULL
 * @pre get_pcpu_id() < MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, get_pcpu_id()).scheduler->pick_next != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by get_pcpu_id().
 * @remark The API can only be called after per_cpu(sched_ctl, get_pcpu_id())->curr_obj
 * have been initialized by init_thread_data().
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void schedule(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing the ID of physical CPU, initialized as
	 *  get_pcpu_id.
	 *  Meaning pcpu_id is the ID of current physical CPU */
	uint16_t pcpu_id = get_pcpu_id();
	/** Declare the following local variables of type 'struct sched_control*'.
	 *  - ctl representing scheduler control block, initialized as
	 *  &per_cpu(sched_ctl, pcpu_id). */
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	/** Declare the following local variables of type 'struct thread_object*'.
	 *  - next representing the thread to switche in, initialized as
	 *  &per_cpu(idle, pcpu_id). */
	struct thread_object *next = &per_cpu(idle, pcpu_id);
	/** Declare the following local variables of type 'struct thread_object*'.
	 *  - prev representing the thread to switche out, initialized as
	 *  ctl->curr_obj. */
	struct thread_object *prev = ctl->curr_obj;
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing RFLAG register value of the current physical CPU, not initialized. */
	uint64_t rflag;

	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(pcpu_id, &rflag);

	/** Set next to the thread picked by scheduler */
	next = ctl->scheduler->pick_next(ctl);

	/** Call bitmap_clear_lock with the following parameters, in order to
	 *  clear 'NEED_RESCHEDULE' bit in the 'flags' field of scheduler control block 'ctl'.
	 *  - NEED_RESCHEDULE
	 *  - &ctl->flags */
	bitmap_clear_lock(NEED_RESCHEDULE, &ctl->flags);

	/* Don't change prev object's status if it's not running */
	/** If is_running(prev) is TRUE,
	 *  meaning thread 'prev' is in the running state */
	if (is_running(prev)) {
		/** Call set_thread_status with the following parameters, in order to
		 *  set thread 'prev' state as 'THREAD_STS_RUNNABLE'.
		 *  - prev
		 *  - THREAD_STS_RUNNABLE */
		set_thread_status(prev, THREAD_STS_RUNNABLE);
	}
	/** Call set_thread_status with the following parameters, in order
	 *  set thread 'next' state as 'THREAD_STS_RUNNING'.
	 *  - next
	 *  - THREAD_STS_RUNNING */
	set_thread_status(next, THREAD_STS_RUNNING);
	/** Set the 'curr_obj' field of scheduler control block 'ctl' to 'next',
	 *  meaning schedule thread 'next' to run on the current physical CPU */
	ctl->curr_obj = next;
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - rflag */
	release_schedule_lock(pcpu_id, rflag);

	/* If we picked different sched object, switch context */
	/** If prev is not next,
	 *  meaning thread 'prev' and thread 'next' isn't identical */
	if (prev != next) {
		/** If prev->switch_out is not NULL,
		 *  meaning 'switch_out' of thread 'prev' isn't a NULL pointer. */
		if (prev->switch_out != NULL) {
			/** Call prev->switch_out with the following parameters, in order to switch out thread 'prev'.
			 *  - prev */
			prev->switch_out(prev);
		}

		/** If next->switch_in is not NULL,
		 *  meaning 'switch_in' of thread 'next' isn't a NULL pointer. */
		if (next->switch_in != NULL) {
			/** Call next->switch_in with the following parameters, in order to switch in thread 'next'.
			 *  - next */
			next->switch_in(next);
		}
		/** Call arch_switch_to with the following parameters, in order to
		 *  save thread 'prev' context and restore thread 'next' context.
		 *  - &prev->host_sp
		 *  - &next->host_sp */
		arch_switch_to(&prev->host_sp, &next->host_sp);
	}
}

/**
 * @brief The interface sleeps the specified thread.
 *
 * Set the thread object status to THREAD_STS_BLOCKED,
 * if the status of the thread specified by \a obj is THREAD_STS_RUNNING,
 * this interface will notify the target processor specified by \a obj->pcpu_id.
 *
 * @param[inout] obj The pointer of the thread object which will sleep.
 *
 * @return None
 *
 * @pre obj != NULL
 * @pre obj->pcpu_id < MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, obj->pcpu_id).scheduler->sleep != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by obj->pcpu_id.
 * @remark The API can only be called after \a obj have been initialized by init_thread_data().
 *
 * @reentrancy Unspecified
 * @threadsafety Yes.
 */
void sleep_thread(struct thread_object *obj)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing ID of the physical CPU that corresponding to thread \a obj,
	 *  initialized as obj->pcpu_id. */
	uint16_t pcpu_id = obj->pcpu_id;
	/** Declare the following local variables of type 'struct acrn_scheduler*'.
	 *  - scheduler representing scheduler corresponding to the physical CPU that specified by pcpu_id,
	 *  initialized as get_scheduler(pcpu_id). */
	struct acrn_scheduler *scheduler = get_scheduler(pcpu_id);
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing RFLAG register value of the physical CPU, not initialized. */
	uint64_t rflag;

	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(pcpu_id, &rflag);
	/** If scheduler->sleep is not NULL,
	 *  meaning scheduler 'sleep' callback function isn't a NULL pointer */
	if (scheduler->sleep != NULL) {
		/** Call scheduler->sleep with the following parameters, in order to block thread \a obj.
		 *  - obj */
		scheduler->sleep(obj);
	}
	/** If is_running(obj) is TRUE,
	 *  meaning thread \a obj is in the running state */
	if (is_running(obj)) {
		/** Call make_reschedule_request with the following parameters,
		 *  in order to make reschedule request to specified physical CPU.
		 *  - pcpu_id
		 *  - DEL_MODE_INIT */
		make_reschedule_request(pcpu_id, DEL_MODE_INIT);
	}
	/** Call set_thread_status with the following parameters, in order to
	 *  set thread \a obj state as THREAD_STS_BLOCKED.
	 *  - obj
	 *  - THREAD_STS_BLOCKED */
	set_thread_status(obj, THREAD_STS_BLOCKED);
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - rflag */
	release_schedule_lock(pcpu_id, rflag);
}

/**
 * @brief The interface wakes the specified thread.
 *
 * If the status of the thread specified by \a obj is THREAD_STS_BLOCKED,
 * this interface will set the thread object status to THREAD_STS_RUNNABLE
 * and notify the target processor specified by \a obj->pcpu_id.
 *
 * @param[inout] obj The thread object of the thread which will be woken up.
 *
 * @return None
 *
 * @pre obj != NULL
 * @pre obj->pcpu_id < MAX_PCPU_NUM
 * @pre per_cpu(sched_ctl, obj->pcpu_id).scheduler->wake != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by \a obj->pcpu_id.
 * @remark The API can only be called after \a obj have been initialized by init_thread_data().
 *
 * @reentrancy Unspecified
 * @threadsafety Yes.
 */
void wake_thread(struct thread_object *obj)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing ID of the physical CPU that corresponding to thread \a obj,
	 *  initialized as obj->pcpu_id. */
	uint16_t pcpu_id = obj->pcpu_id;
	/** Declare the following local variables of type 'struct acrn_scheduler*'.
	 *  - scheduler representing scheduler corresponding to the physical CPU, not initialized. */
	struct acrn_scheduler *scheduler;
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing RFLAG register value of the physical CPU, not initialized. */
	uint64_t rflag;

	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(pcpu_id, &rflag);
	/** If is_blocked(obj) is TRUE,
	 *  meaning thread \a obj is in the blocked state */
	if (is_blocked(obj)) {
		/** Set 'scheduler' to the scheduler corresponding to the physical CPU specified by pcpu_id */
		scheduler = get_scheduler(pcpu_id);
		/** If scheduler->wake is not NULL,
		 *  meaning scheduler \a wake callback function isn't a NULL pointer */
		if (scheduler->wake != NULL) {
			/** Call scheduler->wake with the following parameters, in order to wake thread \a obj.
			 *  - obj */
			scheduler->wake(obj);
		}
		/** Call set_thread_status with the following parameters, in order to
		 *  set thread \a obj state as THREAD_STS_RUNNABLE.
		 *  - obj
		 *  - THREAD_STS_RUNNABLE */
		set_thread_status(obj, THREAD_STS_RUNNABLE);
		/** Call make_reschedule_request with the following parameters,
		 *  in order to make reschedule request to the physical CPU specified by pcpu_id.
		 *  - pcpu_id
		 *  - DEL_MODE_INIT */
		make_reschedule_request(pcpu_id, DEL_MODE_INIT);
	}
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - rflag */
	release_schedule_lock(pcpu_id, rflag);
}

/**
 * @brief The interface is to set NEED_RESCHEDULE in the scheduler control block flag
 * of the current physical CPU or send INIT signal to target physical CPU.
 *
 * @param[in] obj The thread object of the thread which will be notified for some requests.
 *
 * @return None
 *
 * @pre obj != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched
 * have been called once on the processor specified by \a obj->pcpu_id.
 * @remark The API can only be called after \a obj have been initialized by init_thread_data().
 *
 * @reentrancy Unspecified
 * @threadsafety Yes.
 */
void kick_thread(const struct thread_object *obj)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing ID of the physical CPU that corresponding to thread \a obj,
	 *  initialized as obj->pcpu_id. */
	uint16_t pcpu_id = obj->pcpu_id;
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing RFLAG register value of the physical CPU, not initialized. */
	uint64_t rflag;

	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(pcpu_id, &rflag);
	/** If is_running(obj) is TRUE,
	 *  meaning thread \a obj is in the running state */
	if (is_running(obj)) {
		/** If get_pcpu_id() is not pcpu_id,
		 *  meaning current physical CPU ID isn't targeted physical CPU ID */
		if (get_pcpu_id() != pcpu_id) {
			/** Call send_single_init with the following parameters, in order to
			 *  notify the physical CPU specified by pcpu_id.
			 *  - pcpu_id */
			send_single_init(pcpu_id);
		}
	/** If is_runnable(obj) is TRUE,
	 *  meaning thread \a obj is in the runnable state */
	} else if (is_runnable(obj)) {
		/** Call make_reschedule_request with the following parameters,
		 *  in order to make reschedule request to the physical CPU specified by pcpu_id.
		 *  - pcpu_id
		 *  - DEL_MODE_INIT */
		make_reschedule_request(pcpu_id, DEL_MODE_INIT);
	} else {
		/* do nothing */
	}
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - rflag */
	release_schedule_lock(pcpu_id, rflag);
}

/**
 * @brief The interface runs the specified thread.
 *
 * This interface is to update the curr_obj field of scheduler control block
 * to record the running thread object, and set thread object status as THREAD_STS_RUNNING,
 * then jump to the entry point of the thread specified by \a obj.
 *
 * @param[inout] obj The schedule object which will be scheduled..
 *
 * @return None
 *
 * @pre obj != NULL
 * @pre obj->thread_entry != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_sched have been
 * called once on the processor specified by \a obj->pcpu_id.
 * @remark The API can only be called after \a obj have been initialized by init_thread_data().
 *
 * @reentrancy Unspecified
 * @threadsafety When \a obj is different among parallel invocation.
 */
void run_thread(struct thread_object *obj)
{
	/** Declare the following local variables of type uint64_t.
	 *  - rflag representing RFLAG register value of the physical CPU, not initialized. */
	uint64_t rflag;

	/** Call init_thread_data with the following parameters, in order to
	 *  initialize data of thread \a obj.
	 *  - obj */
	init_thread_data(obj);
	/** Call obtain_schedule_lock with the following parameters, in order to
	 *  obtain scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - &rflag */
	obtain_schedule_lock(obj->pcpu_id, &rflag);
	/** Set the 'curr_obj' field of get_cpu_var(sched_ctl) to \a obj,
	 *  Meaning schedule thread \a obj to run on the physical CPU
	 *  corresponding to scheduler control block 'sched_ctl' */
	get_cpu_var(sched_ctl).curr_obj = obj;
	/** Call set_thread_status with the following parameters, in order to
	 *  set thread \a obj state as THREAD_STS_RUNNING.
	 *  - obj
	 *  - THREAD_STS_RUNNING */
	set_thread_status(obj, THREAD_STS_RUNNING);
	/** Call release_schedule_lock with the following parameters, in order to
	 *  release scheduler lock corresponding to the physical CPU specified by pcpu_id.
	 *  - pcpu_id
	 *  - rflag */
	release_schedule_lock(obj->pcpu_id, rflag);

	/** If obj->thread_entry is not NULL,
	 *  meaning entry function of thread \a obj isn't a NULL pointer */
	if (obj->thread_entry != NULL) {
		/** Call obj->thread_entry with the following parameters,
		 *  in order to call entry function of thread \a obj.
		 *  - obj */
		obj->thread_entry(obj);
	}
}

/**
 * @}
 */
