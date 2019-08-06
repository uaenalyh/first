/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rtl.h>
#include <bits.h>
#include <cpu.h>
#include <per_cpu.h>
#include <lapic.h>
#include <schedule.h>
#include <sprintf.h>

static uint64_t pcpu_used_bitmap;

void init_scheduler(void)
{
	struct sched_context *ctx;
	uint32_t i;
	uint16_t pcpu_nums = get_pcpu_nums();

	for (i = 0U; i < pcpu_nums; i++) {
		ctx = &per_cpu(sched_ctx, i);

		spinlock_init(&ctx->scheduler_lock);
		ctx->runqueue = NULL;
		ctx->flags = 0UL;
		ctx->curr_obj = NULL;
	}
}

void get_schedule_lock(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	spinlock_obtain(&ctx->scheduler_lock);
}

void release_schedule_lock(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	spinlock_release(&ctx->scheduler_lock);
}

void set_pcpu_used(uint16_t pcpu_id)
{
	bitmap_set_lock(pcpu_id, &pcpu_used_bitmap);
}

void free_pcpu(uint16_t pcpu_id)
{
	bitmap_clear_lock(pcpu_id, &pcpu_used_bitmap);
}

void add_to_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	if (obj->curr_ctx == NULL && ctx->runqueue == NULL) {
		ctx->runqueue = obj;
		obj->curr_ctx = ctx;
	}
}

void remove_from_cpu_runqueue(struct sched_object *obj)
{
	struct sched_context *ctx = obj->curr_ctx;

	if (ctx != NULL) {
		ctx->runqueue = NULL;
		obj->curr_ctx = NULL;
	}
}

static struct sched_object *get_next_sched_obj(struct sched_context *ctx)
{
	struct sched_object *obj = NULL;

	if (ctx->runqueue != NULL) {
		obj = ctx->runqueue;
	} else {
		obj = &get_cpu_var(idle);
	}

	return obj;
}

/**
 * @pre delmode == DEL_MODE_IPI || delmode == DEL_MODE_INIT
 */
void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctx->flags);
	if (get_pcpu_id() != pcpu_id) {
		switch (delmode) {
		case DEL_MODE_IPI:
			send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
			break;
		case DEL_MODE_INIT:
			send_single_init(pcpu_id);
			break;
		default:
			ASSERT(false, "Unknown delivery mode %u for pCPU%u", delmode, pcpu_id);
			break;
		}
	}
}

bool need_reschedule(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test(NEED_RESCHEDULE, &ctx->flags);
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	bitmap_set_lock(NEED_OFFLINE, &ctx->flags);
	if (get_pcpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

bool need_offline(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test_and_clear_lock(NEED_OFFLINE, &ctx->flags);
}

void make_shutdown_vm_request(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	bitmap_set_lock(NEED_SHUTDOWN_VM, &ctx->flags);
	if (get_pcpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

bool need_shutdown_vm(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test_and_clear_lock(NEED_SHUTDOWN_VM, &ctx->flags);
}

static void prepare_switch(struct sched_object *prev, struct sched_object *next)
{
	if ((prev != NULL) && (prev->prepare_switch_out != NULL)) {
		prev->prepare_switch_out(prev);
	}

	/* update current object */
	get_cpu_var(sched_ctx).curr_obj = next;

	if ((next != NULL) && (next->prepare_switch_in != NULL)) {
		next->prepare_switch_in(next);
	}
}

void schedule(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	struct sched_object *next = NULL;
	struct sched_object *prev = ctx->curr_obj;

	get_schedule_lock(pcpu_id);
	next = get_next_sched_obj(ctx);
	bitmap_clear_lock(NEED_RESCHEDULE, &ctx->flags);

	if (prev == next) {
		release_schedule_lock(pcpu_id);
	} else {
		prepare_switch(prev, next);
		release_schedule_lock(pcpu_id);

		arch_switch_to(&prev->host_sp, &next->host_sp);
	}
}

void run_sched_thread(struct sched_object *obj)
{
	if (obj->thread != NULL) {
		obj->thread(obj);
	}

	ASSERT(false, "Shouldn't go here, invalid thread!");
}

void switch_to_idle(run_thread_t idle_thread)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct sched_object *idle = &per_cpu(idle, pcpu_id);
	char idle_name[16];

	idle->thread = idle_thread;
	idle->prepare_switch_out = NULL;
	idle->prepare_switch_in = NULL;
	get_cpu_var(sched_ctx).curr_obj = idle;

	run_sched_thread(idle);
}
