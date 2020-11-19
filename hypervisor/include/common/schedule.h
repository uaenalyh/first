/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H

/**
 * @addtogroup hwmgmt_schedule
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the hwmgmt.schedule module.
 *
 * This file declares all external functions, data structures, and macros
 * that shall be provided by the hwmgmt.schedule module.
 *
 */
#include <spinlock.h>

/**
 * @brief The index on behalf of need to reschedule in the CPU schedule context' flags bitmap.
 */
#define NEED_RESCHEDULE (1U)

/**
 * @brief The macro describes the type of INIT IPI to be sent to the target processor.
 */
#define DEL_MODE_INIT (1U)

/**
 * @brief This type is to indicate the states of vCPU thread
 * and these states are used by scheduler .
 *
 * @asinteger disallowed
 */
enum thread_object_state {
	/**
	 * @brief This status indicates the thread is running on a physical processor.
	 */
	THREAD_STS_RUNNING = 1,
	/**
	 * @brief This status indicates the thread can be scheduled on a physical processor.
	 */
	THREAD_STS_RUNNABLE,
	/**
	 * @brief This status indicates the thread is blocked and is waiting for some events.
	 */
	THREAD_STS_BLOCKED,
};

/**
 * @brief This type is to indicate how the target vCPU will be notified by scheduler.
 *
 * @asinteger disallowed
 */
enum sched_notify_mode {
	/**
	 * @brief This notification mode means the scheduler
	 * will use INIT signal to notify the specified target process.
	 */
	SCHED_NOTIFY_INIT,
	/**
	 * @brief This notification mode means the scheduler
	 * will use IPI signal to notify the specified target process.
	 */
	SCHED_NOTIFY_IPI,
};

/** Declare thread_object struct which represents thread.  */
struct thread_object;
/** The function pointer type of the thread entry.  */
typedef void (*thread_entry_t)(struct thread_object *obj);
/** The function pointer type of callback for thread switching out or switching in.  */
typedef void (*switch_t)(struct thread_object *obj);

/**
 * @brief Data structure contains elements for a thread, which is the basic unit for scheduling.
 *
 * @consistency For all struct thread_object p, p.sched_ctl == &(per_cpu_data[p.pcpu_id].sched_ctl)
 * @alignment 8
 *
 * @remark N/A
 */
struct thread_object {
	/**
	 * @brief The ID of the physical CPU which the current thread is running on.
	 */
	uint16_t pcpu_id;
	/**
	 * @brief The scheduler control block pointer which points the scheduler control block,
	 * which contains scheduler control information.
	 */
	struct sched_control *sched_ctl;
	/**
	 * @brief The function entry of this thread.
	 */
	thread_entry_t thread_entry;
	/**
	 * @brief The status of this thread,this status may be THREAD_STS_RUNNING,
	 * or THREAD_STS_RUNNABLE, or THREAD_STS_BLOCKED.
	 */
	volatile enum thread_object_state status;
	/**
	 * @brief Notify mode of this thread.
	 * In the current design, only SCHED_NOTIFY_INIT mode is used.
	 */
	enum sched_notify_mode notify_mode;
	/**
	 * @brief The top pointer of this thread stack.
	 */
	uint64_t host_sp;
	/**
	 * @brief The function pointer which points to the function
	 * and this function will be executed before the current thread switches out.
	 */
	switch_t switch_out;
	/**
	 * @brief The function pointer which points to the function
	 * and this function will be executed before the current thread switches in.
	 */
	switch_t switch_in;
};

/**
 * @brief Data structure to define scheduler control block, which contains scheduler control information.
 *
 * @consistency For all struct sched_control p, p.priv == &(per_cpu_data[p.pcpu_id].sched_noop_ctl)
 * @alignment 8
 *
 * @remark N/A
 */
struct sched_control {
	/**
	 * @brief This field stores the physical CPU ID in the scheduler control block
	 * and this scheduler control block is in the corresponding physical CPU information area .
	 */
	uint16_t pcpu_id;
	/**
	 * @brief The bitmap flags of the scheduler control block.
	 */
	uint64_t flags;
	/**
	 * @brief The thread object pointer which points to the object of the current thread
	 * which is running in the physical CPU.
	 *
	 * Note this field is NULL before the first thread runs on the physical CPU.
	 */
	struct thread_object *curr_obj;
	/**
	 * @brief The spinlock to protect the scheduler control block and thread object.
	 */
	spinlock_t scheduler_lock;
	/**
	 * @brief The scheduler of current schedule control used.
	 */
	struct acrn_scheduler *scheduler;
	/**
	 * @brief The pointer which points to the private data of the scheduler control block.
	 */
	void *priv;
};

/**
 * @brief Data structure to define scheduler.
 *
 * Data structure to define scheduler, which contains scheduler name and
 * a series of callback functions that are used by scheduler.
 *
 * @consistency N/A
 * @alignment 8
 *
 * @remark N/A
 */
struct acrn_scheduler {
	/**
	 * @brief The name of the scheduler.
	 */
	char name[16];

	/**
	 * @brief The init callback function of the scheduler.
	 *
	 * It's used to initialize scheduler.
	 */
	int32_t (*init)(struct sched_control *ctl);

	/**
	 * @brief The init_data callback function of the scheduler.
	 *
	 * It's used to initialize private data of scheduler.
	 */
	void (*init_data)(struct thread_object *obj);

	/**
	 * @brief The pick_next callback function of the scheduler.
	 *
	 * It's used to pick the next thread that will run on the physical CPU
	 * corresponding to scheduler control block 'ctl'.
	 */
	struct thread_object *(*pick_next)(struct sched_control *ctl);

	/**
	 * @brief The sleep callback function of the scheduler.
	 *
	 * It's used to block the specified thread.
	 */
	void (*sleep)(struct thread_object *obj);

	/**
	 * @brief The wake callback function of the scheduler.
	 *
	 * It's used to wake up the specified thread from the blocked state.
	 */
	void (*wake)(struct thread_object *obj);

	/**
	 * @brief The yield callback function of the scheduler.
	 *
	 * It's used to yield current thread that is running on physical CPU
	 * corresponding to scheduler control block 'ctl'.
	 */
	void (*yield)(struct sched_control *ctl);

	/**
	 * @brief The prioritize callback function of the scheduler.
	 *
	 * It's used to prioritize the specified thread.
	 */
	void (*prioritize)(struct thread_object *obj);

	/**
	 * @brief The deinit_data callback function of the scheduler.
	 *
	 * It's used to deinit private data of scheduler.
	 */
	void (*deinit_data)(struct thread_object *obj);

	/**
	 * @brief The deinit callback function of the scheduler.
	 *
	 * It's used to deinit scheduler.
	 */
	void (*deinit)(struct sched_control *ctl);
};

/**
 * @brief Global 'acrn_scheduler' variable that is used as the default scheduler.
 *
 * A vCPU thread is always pinned on a fixed physical CPU.
 *
 * In this scheduler, each physical CPU has at most 2 threads pinned on it,
 * namely the idle thread and a vCPU thread running on it.
 * If there is no vCPU thread running on the physical CPU, the idle thread
 * will be scheduled.
 *
 */
extern struct acrn_scheduler sched_noop;

/**
 * @brief Data structure to store private data of the scheduler control block
 * that is used by sched_noop scheduler.
 *
 * @consistency N/A
 * @alignment 8
 *
 * @remark N/A
 */
struct sched_noop_control {
	/**
	 * @brief The thread object pointer which points to the thread
	 * that will be scheduled on the physical CPU to run by noop scheduler.
	 */
	struct thread_object *noop_thread_obj;
};

uint16_t sched_get_pcpuid(const struct thread_object *obj);

void init_sched(uint16_t pcpu_id);

void deinit_sched(uint16_t pcpu_id);

void obtain_schedule_lock(uint16_t pcpu_id, uint64_t *rflag);

void release_schedule_lock(uint16_t pcpu_id, uint64_t rflag);

void init_thread_data(struct thread_object *obj);

void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode);

bool need_reschedule(uint16_t pcpu_id);

void run_thread(struct thread_object *obj);

void sleep_thread(struct thread_object *obj);

void wake_thread(struct thread_object *obj);

void kick_thread(const struct thread_object *obj);

void schedule(void);

void arch_switch_to(void *prev_sp, void *next_sp);

void run_idle_thread(void);

/**
 * @}
 */

#endif /* SCHEDULE_H */
