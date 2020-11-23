/*-
 * Copyright (C) 2005-2011 HighPoint Technologies, Inc.
 * Copyright (c) 2017 Intel Corporation
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
#ifndef LIST_H_
#define LIST_H_

/**
 * @addtogroup lib_util
 *
 * @{
 */

/**
 * @file
 * @brief This file declares external list APIs that shall be provided by the lib.util module.
 */

/**
 * @brief Return the structure where \a ptr is embedded in.
 *
 * @param[in]    ptr An instance which is embedded.
 * @param[in]    type A struct or union type having \a member as it's field.
 * @param[in]    member The name of the embedded field in \a type.
 *
 * @return A pointer to the instance where the structure pointed to by \a ptr is embedded in.
 *
 * @pre \a ptr shall point to the field \a member of an instance of type \a type.
 * @pre \a ptr != NULL
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to a pointer calculated by subtracting the offset in byte of \a member within a \a type structure from
 * \a ptr. The returned pointer points to a structure of \a type.
 */
#define list_entry(ptr, type, member) ((type *)((char *)(ptr) - (uint64_t)(&((type *)0)->member)))

/**
 * @}
 */

#endif /* LIST_H_ */
