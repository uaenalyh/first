/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>

/**
 * @defgroup lib_util lib.util
 * @ingroup lib
 * @brief Providing utility APIS, error codes, standard types and attribute attached macros.
 *
 * Implementation of utility APIs to do string operating, memory operation, arithmetic operation and alignment
 * checking operation. Also providing error codes, standard types and attribute attached macros.
 *
 * This module is designed to be used by the rest of the hypervisor and does not depend on other modules.
 *
 * This module is decomposed into the following files:
 *
 * - memory.c: Implement memory operation APIs that shall be provided by the lib.util module.
 * - string.c: Implement string operation APIs that shall be provided by the lib.util module.
 * - util.h: Implement utils APIs including arithmetic operation and alignment checking operation.
 * - types.h: Declare external macros and types that shall be provided by the lib.util module.
 * - rtl.h: Declare string operation APIs that shall be provided by the lib.util module.
 * - list.h: Declare external list APIs that shall be provided by the lib.util module.
 * - errno.h: Declare external error codes that shall be provided by the lib.util module.
 *
 * @{
 */

/**
 * @file
 * @brief This file defines and implements memory operation APIs that shall be provided by the lib.util module.
 *
 * This file is decomposed into the following functions:
 *
 * - memset: Set n bytes starting from base to v.
 * - memcpy_s: Copy slen bytes from a source address to a destination address.
 * - memset_erms: Set n bytes starting from base to v by using Enhanced REP MOVSB/STOSB.
 * - memcpy_erms: Copy slen bytes from source address to destination address by using Enhanced REP MOVSB/STOSB.
 */

/**
 * @brief Copy slen bytes from source address to destination address by using Enhanced REP MOVSB/STOSB.
 *
 * @param[out]    d Destination address.
 * @param[in]     s The address which data is copied from.
 * @param[in]     slen The size in byte of the data to be copied.
 *
 * @return None
 *
 * @pre d != NULL
 * @pre s != NULL
 * @pre The host logical address range [\a d ,\a d + \a dmax ) maps to memory with write privilege.
 * @pre The host logical address range [\a s ,\a s + \a slen ) maps to memory with read privilege.
 * @pre The intersection of host physical address range [hva2hpa(\a s ), hva2hpa(\a s + \a slen )) and
 * [hva2hpa(\a d ),hva2hpa(\a d + \a dmax )) shall be empty.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety When the following conditions hold:
 * - The intersection of virtual address range [\a d , \a d + \a slen ) on one physical processor and virtual address
 *   range [\a s , \a s + \a slen ) on another is empty.
 * - The intersection of virtual address range [\a d , \a d + \a slen ) on different physical processors is empty.
 *
 */
static inline void memcpy_erms(void *d, const void *s, size_t slen)
{
	/** Execute rep; movsb in order to copy slen bytes from s address to d address.
	 *  - Input operands: ECX holds slen, RDI holds d, RSI holds s
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("rep; movsb" :: "D"(d), "S"(s), "c"(slen) : "memory");
}

/**
 * @brief  Copy slen bytes from a source address to a destination address.
 *
 * @param[out]    d Destination address
 * @param[in]     dmax The data's maximum copy length of destination address
 * @param[in]     s The address which data is copied from.
 * @param[in]     slen The size in byte of the data to be copied.
 *
 * @return Pointer to destination address.
 *
 * @pre d != NULL
 * @pre s != NULL
 * @pre The host logical address range [\a d ,\a d + \a dmax ) maps to memory with write privilege.
 * @pre The host logical address range [\a s ,\a s + \a slen ) maps to memory with read privilege.
 * @pre The intersection of host physical address range [hva2hpa(\a s ), hva2hpa(\a s + \a slen )) and
 * [hva2hpa(\a d ),hva2hpa(\a d + \a dmax )) shall be empty.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety When the following conditions hold:
 * - The intersection of virtual address range [\a d , \a d + \a slen ) on one physical processor and virtual address
 *   range [\a s , \a s + \a slen ) on another is empty.
 * - The intersection of virtual address range [\a d , \a d + \a slen ) on different physical processors is empty.
 */
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{
	/** If slen is not equal to 0 and
	 *  dmax is not equal to 0 and
	 *  dmax is greater than slen */
	if ((slen != 0U) && (dmax != 0U) && (dmax >= slen)) {
		/* same memory block, no need to copy */
		/** If d is not equal to s */
		if (d != s) {
			/** Call memcpy_erms with the following parameters, in order to copy exactly slen bytes
			 *  from source address to destination address.
			 *  - d
			 *  - s
			 *  - slen */
			memcpy_erms(d, s, slen);
		}
	}
	/** Return destination address */
	return d;
}

/**
 * @brief Set n bytes starting from base to v by using Enhanced REP MOVSB/STOSB
 *
 * @param[inout]    base Address of the memory block to fill
 * @param[in]       v The value to be set to each byte of the specified memory block
 * @param[in]       n The number of bytes to be set
 *
 * @return None
 *
 * @pre base != NULL
 * @pre n! = 0
 * @pre Host logical address [\a base , \a base + \a n ) maps to memory with write privilege
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety When the intersection of any virtual address range [\a base , \a base + \a n )
 * set on different physical processor is null set.
 */
static inline void memset_erms(void *base, uint8_t v, size_t n)
{
	/** Execute rep ; stosb in order to set n bytes starting from base to v.
	 *  - Input operands: EAX holds zero-extended v, RDI holds base, ECX holds n
	 *  - Output operands: None
	 *  - Clobbers: memory */
	asm volatile("rep ; stosb" :: "D"(base), "a"(v), "c"(n) : "memory");
}

/**
 * @brief Set n bytes starting from base to v
 *
 * @param[inout]    base The address of the memory block to fill
 * @param[in]       v The value to be set to each byte of the specified memory block
 * @param[in]       n The number of bytes to be set
 *
 * @return The address of the memory block to fill
 *
 * @pre base != NULL
 * @pre n! = 0
 * @pre Host logical address [\a base , \a base + \a n ) maps to memory with write privilege
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety When the intersection of any virtual address range [\a base , \a base + \a n )
 * set on different physical processor is null set.
 */
void *memset(void *base, uint8_t v, size_t n)
{
	/*
	 * Some CPUs support enhanced REP MOVSB/STOSB feature. It is recommended
	 * to use it when possible.
	 */
	/** Call memset_erms with the following parameters, in order to set the n bytes
	 *  starting from base to v.
	 *  - base
	 *  - v
	 *  - n */
	memset_erms(base, v, n);

	/** Return the address of the memory block to fill */
	return base;
}

/**
 * @}
 */
