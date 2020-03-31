/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <per_cpu.h>
#include <schedule.h>

/**
 * @addtogroup hwmgmt_schedule
 *
 * @{
 */

/**
 * @file
 * @brief The definition and implementation of 'sched_noop' scheduler.
 *
 * 'sched_noop' scheduler forces a fixed 1:1 mapping between a vCPU thread and a physical CPU.
 * Each physical CPU has at most 2 threads pinned on it,
 * namely the idle thread and a vCPU thread running on it.
 * If there is no vCPU thread running on the physical CPU, the idle thread will be scheduled.
 *
 * This file implements the callback functions that shall be provided by 'sched_noop' scheduler.
 * - init
 * - pick_next
 * - sleep
 * - wake
 */

/**
 * @brief The interface initializes the specified scheduler control block.
 *
 * @param[inout] ctl The specified scheduler control block to initialize.
 *
 * @return An error code indicating if the specified scheduler control block initialization succeeds or not.
 *
 * @retval 0 The specified scheduler control block has been successfully initialized.
 *
 * @pre ctl != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a ctl is different among parallel invocation.
 */
static int32_t sched_noop_init(struct sched_control *ctl)
{
	/** Declare the following local variables of type 'struct sched_noop_control*'.
	 *  - noop_ctl representing private data of 'sched_noop' scheduler,
	 *  initialized as &per_cpu(sched_noop_ctl, ctl->pcpu_id).  */
	struct sched_noop_control *noop_ctl = &per_cpu(sched_noop_ctl, ctl->pcpu_id);
	/** Set private data of scheduler control block \a ctl to 'noop_ctl' */
	ctl->priv = noop_ctl;

	/** Return 0 which means the specified scheduler control block has been successfully initialized. */
	return 0;
}

/**
 * @brief The interface picks the next thread that will run on the current physical CPU.
 *
 * @param[in] ctl The specified scheduler control block to
 * pick the next thread that will run on the current physical CPU.
 *
 * @return the thread that will run on the current physical CPU.
 *
 * @pre ctl != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a ctl is different among parallel invocation.
 */
static struct thread_object *sched_noop_pick_next(struct sched_control *ctl)
{
	/** Declare the following local variables of type 'struct sched_noop_control*'.
	 *  - noop_ctl representing the private data of 'sched_noop' scheduler,
	 *  initialized as (struct sched_noop_control *)ctl->priv. */
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)ctl->priv;
	/** Declare the following local variables of type 'struct thread_object*'.
	 *  - next representing the thread that will run on the current physical CPU,
	 *  initialized as NULL. */
	struct thread_object *next = NULL;

	/** If noop_ctl->noop_thread_obj is not NULL,
	 *  meaning there is a vCPU thread corresponding to the current physical CPU */
	if (noop_ctl->noop_thread_obj != NULL) {
		/** Set next to the vCPU thread */
		next = noop_ctl->noop_thread_obj;
	} else {
		/** Set next to the address of get_cpu_var(idle)
		 *  which is the idle thread that corresponding to the current physical CPU */
		next = &get_cpu_var(idle);
	}
	/** Return the thread that will run on the current physical CPU. */
	return next;
}

/**
 * @brief The interface sleeps the specified thread.
 * Do nothing if the thread is already requested to sleep and is not waked up since then.
 *
 * @param[inout] obj The specified thread to block.
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
static void sched_noop_sleep(struct thread_object *obj)
{
	/** Declare the following local variables of type 'struct sched_noop_control*'.
	 *  - noop_ctl representing the private data of 'sched_noop' scheduler,
	 *  initialized as (struct sched_noop_control *)obj->sched_ctl->priv. */
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)obj->sched_ctl->priv;

	/** If noop_ctl->noop_thread_obj is \a obj,
	 *  meaning thread \a obj is the vCPU thread corresponding to the current physical CPU */
	if (noop_ctl->noop_thread_obj == obj) {
		/** Set the vCPU thread corresponding to the current physical CPU to NULL */
		noop_ctl->noop_thread_obj = NULL;
	}
}

/**
 * @brief The interface wakes the specified thread.
 * Do nothing if the thread is already requested to wake and is not sleeping since then.
 *
 * @param[inout] obj The specified thread to wake.
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
static void sched_noop_wake(struct thread_object *obj)
{
	/** Declare the following local variables of type 'struct sched_noop_control*'.
	 *  - noop_ctl representing the private data of 'sched_noop' scheduler,
	 *  initialized as (struct sched_noop_control *)obj->sched_ctl->priv. */
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)obj->sched_ctl->priv;

	/** If noop_ctl->noop_thread_obj is NULL,
	 *  meaning there is no vCPU thread corresponding to the current physical CPU */
	if (noop_ctl->noop_thread_obj == NULL) {
		/** Set the vCPU thread corresponding to the current physical CPU to thread \a obj */
		noop_ctl->noop_thread_obj = obj;
	}
}

/** Declare 'sched_noop' scheduler .  */
struct acrn_scheduler sched_noop = {
	.name = "sched_noop",
	.init = sched_noop_init,
	.pick_next = sched_noop_pick_next,
	.sleep = sched_noop_sleep,
	.wake = sched_noop_wake,
};

/**
 * @}
 */
