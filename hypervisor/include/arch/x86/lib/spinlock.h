/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

/**
 * @addtogroup lib_lock
 *
 * @{
 */

/**
 * @file
 * @brief The definition and implementation of spinlock infrastructures.
 *
 * It provides external structures and APIs for spinlock operation.
 * - spinlock_irqsave_obtain
 * - spinlock_irqrestore_release
 * - spinlock_init
 * - spinlock_obtain
 * - spinlock_release
 */

#ifndef ASSEMBLER

#include <types.h>
#include <rtl.h>

/**
 * @brief The spinlock definiation.
 *
 * This is an architecture dependent spinlock type.
 *
 * @consistency N/A
 * @alignment 4
 *
 * @remark N/A
 */
typedef struct _spinlock {
	uint32_t head; /**< The head field of spinlock, increase 1 when the spinlock is obtained */
	uint32_t tail; /**< The tail field of spinlock, increase 1 when the spinlock is released */

} spinlock_t;

/**
 * @brief This function initializes a given spinlock.
 *
 * @param[out] lock The spinlock to be initialized.
 *
 * @return None
 *
 * @pre lock != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark This function shall be called once and only once to each spinlock.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline void spinlock_init(spinlock_t *lock)
{
	/** Call memset with following parameters, in order to
	 *  set the all fields of lock to be 0H.
	 *  - lock
	 *  - 0H
	 *  - sizeof(spinlock_t)
	 */
	(void)memset(lock, 0U, sizeof(spinlock_t));
}

/**
 * @brief This function obtains a given spinlock.
 *
 * @param[inout] lock The spinlock to be obtained.
 *
 * @return None
 *
 * @pre lock != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT
 *
 * @remark When call spinlock_obtain(lock), lock must be initialized by spinlock_init(lock).
 *
 * @reentrancy Unspecified
 * @threadsafety yes
 */
static inline void spinlock_obtain(spinlock_t *lock)
{

	/** Execute following inline assembly
	 *  with following parameters, in order to obtain a given spinlock.
	 *  The function atomically increments and exchanges the head
	 *  counter of the queue. If the old head of the queue is equal to the
	 *  tail, we have locked the spinlock. Otherwise we have to wait.
	 *  - Instruction template:
	 *	"   movl $0x1,%%eax\n"
	 *	"   lock xaddl %%eax,%[head]\n"
	 *	"   cmpl %%eax,%[tail]\n"
	 *	"   jz 1f\n"
	 *	"2: pause\n"
	 *	"   cmpl %%eax,%[tail]\n"
	 *	"   jnz 2b\n"
	 *	"1:\n"
	 *  - Input operands:
	 *	- Memory pointed to by lock->head holds a spinlock head field.
	 *	- Memory pointed to by lock->tail holds a spinlock tail field.
	 *  - Output operands: None
	 *  - Clobbers: "cc", "memory", "eax"
	 */
	asm volatile("   movl $0x1,%%eax\n"
		     "   lock xaddl %%eax,%[head]\n"
		     "   cmpl %%eax,%[tail]\n"
		     "   jz 1f\n"
		     "2: pause\n"
		     "   cmpl %%eax,%[tail]\n"
		     "   jnz 2b\n"
		     "1:\n"
		     :
		     : [ head ] "m"(lock->head), [ tail ] "m"(lock->tail)
		     : "cc", "memory", "eax");
}

/**
 * @brief This function releases a given spinlock.
 *
 * @param[inout] lock The spinlock to be released.
 *
 * @return None
 *
 * @pre lock != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT
 *
 * @remark This function shall be used to end critical sections started by calling spinlock_obtain().
 *
 * @reentrancy Unspecified
 * @threadsafety yes
 */
static inline void spinlock_release(spinlock_t *lock)
{
	/** Execute following inline assembly
	 *  with following parameters, in order to increment tail of queue.
	 *  - Instruction template: "   lock incl %[tail]\n"
	 *  - Input operands:
	 *  - Memory pointed to by lock->tail field holds a spinlock tail field.
	 *  - Output operands: None
	 *  - Clobbers: "cc", "memory"
	 */
	asm volatile("   lock incl %[tail]\n" : : [ tail ] "m"(lock->tail) : "cc", "memory");
}

#else

#define SYNC_SPINLOCK_HEAD_OFFSET 0 /**< The offset of the head element. */
#define SYNC_SPINLOCK_TAIL_OFFSET 4 /**< The offset of the tail element. */

.macro spinlock_obtain lock
	movl $1, % eax
	lea \lock, % rbx
	lock xaddl % eax, SYNC_SPINLOCK_HEAD_OFFSET(%rbx)
	cmpl % eax, SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jz 1f
2 :
	pause cmpl % eax,
	SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jnz 2b
1 :
.endm

/**
 * @brief Define the spinlock_obtain(x) by using assembly implementation of spinlock_obtain.
 */
#define spinlock_obtain(x) spinlock_obtain lock = (x)

.macro spinlock_release lock
	lea \lock, % rbx
	lock incl SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
.endm

/**
 * @brief Define the spinlock_release(x) by using assembly implementation of spinlock_release.
 */
#define spinlock_release(x) spinlock_release lock = (x)

#endif

/**
 * @brief This macro disables the CPU interrupts, saves the rflags
 *         and then obtain the lock.
 *
 * It does two things:
 *
 * Call CPU_INT_ALL_DISABLE with following parameters, in order to
 * disable the CPU interrupts and save the RFLAGS register to the memory
 * pointed by \a p_rflags.
 * - p_rflags
 *
 * Call spinlock_obtain with following parameters, in order to
 * obtain a given lock.
 * - lock
 *
 * @param[inout] lock The lock to be obtained.
 * @param[out] p_rflags A pointer to an integer which is used to store the value of the RFLAGS register.
 *
 * @return None
 *
 * @pre lock != NULL
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety yes
 */
#define spinlock_irqsave_obtain(lock, p_rflags) \
	do {                                    \
		CPU_INT_ALL_DISABLE(p_rflags);  \
		spinlock_obtain(lock);          \
	} while (0)

/**
 * @brief This macro releases the lock and restores the CPU interrupts and rflags.
 *
 * It does two things:
 *
 * Call spinlock_release with following parameters, in order to
 * release a given lock.
 * - lock
 *
 * Call CPU_INT_ALL_RESTORE with following parameters, in order to
 * restore the RFLAGS register based on the value \a rflags.
 * - rflags
 *
 * @param[inout] lock The lock to be released.
 * @param[in] rflags The value of the RFLAGS register to be restored.
 *
 * @return None
 *
 * @pre lock != NULL
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety yes
 */
#define spinlock_irqrestore_release(lock, rflags) \
	do {                                      \
		spinlock_release(lock);           \
		CPU_INT_ALL_RESTORE(rflags);      \
	} while (0)

/**
 * @}
 */

#endif /* SPINLOCK_H */
