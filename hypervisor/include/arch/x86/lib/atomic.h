/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef ATOMIC_H
#define ATOMIC_H

/**
 * @defgroup lib_lock lib.lock
 * @ingroup lib
 * @brief The definition and implementation of atomic and spinlock infrastructures.
 *
 * Usage:
 * - 'vp-dm.vperipheral' depends on this module for atomic operation and spinlock operation.
 * - 'hwmgmt.irq' depends on this module for spinlock operation.
 * - 'hwmgmt.vtd' depends on this module for spinlock operation.
 * - 'hwmgmt.pci' depends on this module for spinlock operation.
 * - 'hwmgmt.schedule' depends on this module for spinlock operation.
 * - 'hwmgmt.apic' depends on this module for spinlock operation.
 *
 * Dependency:
 * - This module depends on 'lib.utils' module to use memset.
 * - This module depends on 'hwmgmt.cpu' module to disable/restore CPU interrupts.
 *
 * @{
 */

/**
 * @file
 * @brief The definition and implementation of atomic infrastructures.
 *
 * It provides external APIs for atomically exchanging register/memory with register.
 */
#include <types.h>

#define BUS_LOCK "lock ; " /**< Lock prefix for instructions */

#define build_atomic_swap(name, size, type)                                                           \
	static inline type name(type *ptr, type v)                                                    \
	{                                                                                             \
		asm volatile(BUS_LOCK "xchg" size " %1,%0" : "+m"(*ptr), "+r"(v) : : "cc", "memory"); \
		return v;                                                                             \
	}
/**
 * @brief Declare a function named atomic_swap32 by using build_atomic_swap.
 *        This function writes the \a v into the specified address \a ptr and
 *        returns the original content in the address pointed by \a ptr.
 *
 * It does following things:
 *
 * Execute inline assembly ("xchg")
 *  with following parameters, in order to exchanging a memory with a register
 *  - Instruction template: BUS_LOCK "xchg" size " %1,%0".
 *  - Input operands: None
 *  - Output operands:
 *	- Memory pointed to by ptr holds a value to be exchanged.
 *	- A general register holds a value to be exchanged.
 *  - Clobbers: "cc", "memory"
 *
 * Return the original content in the address pointed by \a ptr.
 *
 * @param[inout] ptr The address where to be written.
 * @param[in] v The value to be written to the address ptr.
 *
 * @return The original content in the address pointed by \a ptr.
 *
 * @pre ptr != NULL
 *
 * @post N/A
 *
 * @mode N/A
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
build_atomic_swap(atomic_swap32, "l", uint32_t)

/**
 * @}
 */

#endif /* ATOMIC_H*/
