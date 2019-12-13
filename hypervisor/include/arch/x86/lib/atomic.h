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
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */
#include <types.h>

#define BUS_LOCK "lock ; "

#define build_atomic_swap(name, size, type)                                                           \
	static inline type name(type *ptr, type v)                                                    \
	{                                                                                             \
		asm volatile(BUS_LOCK "xchg" size " %1,%0" : "+m"(*ptr), "+r"(v) : : "cc", "memory"); \
		return v;                                                                             \
	}
build_atomic_swap(atomic_swap32, "l", uint32_t)

/*
 * #define atomic_readandclear32(P) \
 * (return (*(uint32_t *)(P)); *(uint32_t *)(P) = 0U;)
 */
static inline uint32_t atomic_readandclear32(uint32_t *p)
{
	return atomic_swap32(p, 0U);
}

/**
 * @}
 */

#endif /* ATOMIC_H*/
