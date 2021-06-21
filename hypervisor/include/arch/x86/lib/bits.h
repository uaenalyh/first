/*-
 * Copyright (c) 1998 Doug Rabson
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

#ifndef BITS_H
#define BITS_H

/**
 * @defgroup lib_bits lib.bits
 * @ingroup lib
 * @brief Providing bitwise operation APIs and bitwise macros.
 *
 * Implement bitwise operation APIs to find the most/least significant bit, set bit, clear bit, and test bit. And
 * provide a INVALID_BIT_INDEX macro to express the invalid bit index.
 *
 * This module could be used by all the other modules and does not depend on other modules.
 *
 * This module is decomposed into the following files:
 *
 * bits.h          - Providing bitwise operation APIs and bitwise macros.
 *
 * @{
 */

/**
 * @file
 * @brief Providing bitwise operation APIs and bitwise macros.
 *
 * Implementation of bitwise operation APIs to find the most significant bit, set bit, clear bit, and test bit. Also
 * provide a INVALID_BIT_INDEX macro to express the invalid bit index.
 *
 * This file is decomposed into the following functions:
 *
 * - fls32(value)                             Find the most significant bit set in a 32-bit value and return the index
 *                                            of that bit.
 * - ffs64(value)                             Find the least significant bit set in a 64-bit value and return the
 *                                            index of that bit.
 * - bitmap_set_nolock(nr_arg, addr)          Set the \a nr_arg-th bit in the integer pointed by \a addr. This
 *                                            operation is not protected by the bus lock.
 * - bitmap_set_lock(nr_arg, addr)            Set the \a nr_arg-th bit atomically in the integer pointed by \a addr.
 * - bitmap_clear_nolock(nr_arg, addr)        Clear the \a nr_arg-th bit in the the integer pointed by \a addr.
 *                                            This operation is not protected by the bus lock.
 * - bitmap_clear_lock(nr_arg, addr)          Clear the \a nr_arg-th bit atomically in the the integer pointed by
 *                                            \a addr.
 * - bitmap_test(nr, addr)                    Test whether the \a nr-th bit of the integer pointed by \a addr is set.
 * - bitmap_test_and_set_lock(nr_arg, addr)   Test and set the \a nr_arg-th bit in the integer pointed by \a addr.
 * - bitmap_test_and_clear_lock(nr_arg, addr) Test and clear the \a nr_arg-th bit in the integer pointed by \a addr.
 */
#include <atomic.h>

/**
 *
 * @brief An integer is used to indicate that a bit satisfying a certain condition cannot be found.
 *
 * This may be returned by functions searching a specific set bit (e.g. ffs64 and fls32) in the given value of
 * the value happens to be 0.
 *
 **/
#define INVALID_BIT_INDEX 0xffffU

/**
 * @brief Find the most significant bit set in a 32-bit value and return the index of that bit.
 *
 * Bits are numbered starting at 0, the least significant bit. A return value of INVALID_BIT_INDEX means return value
 * is invalid bit index when the input argument was zero.
 *
 * @param[in]    value The integer where the function find the most significant bit.
 *
 * @return Return the index of the most significant bit which is set in \a value if \a value is non-zero, otherwise,
 * return INVALID_BIT_INDEX.
 *
 * @retval INVALID_BIT_INDEX \a value is equal to 0.
 *
 * @pre  The value address should align to 64, to make sure the instruction can operate it at one time.
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety  Yes
 */
static inline uint16_t fls32(uint32_t value)
{
	/** Declare the following local variables of type uint32_t.
	 *  - ret representing the index of the bit in \a value, where bit is the most significant bit set in
	 *    \a value. */
	uint32_t ret;
	/** Find the most significant bit set in \a value.
	 *  - Instruction template:
	 *    "bsrl %1,%0\n"
	 *    "jnz 1f\n"
	 *    "mov %2,%0\n"
	 *    "1:"
	 *  - Input operands: The first input operand holds value, INVALID_BIT_INDEX is the second input operand
	 *    that will be embedded into the assembly code used as the source operand of mov instruction.
	 *  - Output operands: The index of the most significant bit which is set in \a value is stored to variable
	 *    ret.
	 *  - Clobbers: None */
	asm volatile("	bsrl	%1, %0\n"
		     "	jnz	1f\n"
		     "	mov	%2, %0\n"
		     "1:"
		     : "=r"(ret)
		     : "rm"(value), "i"(INVALID_BIT_INDEX));
	/** Return the index of the most significant bit which is set in \a value */
	return (uint16_t)ret;
}

/**
 * @brief Find the least significant bit set in a 64-bit value and return the index of that bit.
 *
 * Bits are numbered starting at 0, which is stated as the least significant bit.
 *
 * @param[in]    value The integer where the function find the least significant bit.
 *
 * @return Return the index of the least significant bit which is set in \a value if \a value is non-zero, otherwise,
 * return INVALID_BIT_INDEX.
 *
 * @retval INVALID_BIT_INDEX \a value is equal to 0.
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint16_t ffs64(uint64_t value)
{
	/** Declare the following local variables of type uint64_t.
	 *  - ret representing the index of the bit in \a value, where bit is the least significant bit set in
	 *    \a value.*/
	uint64_t ret;
	/** Find the least significant bit set in \a value.
	 *  - Instruction template:
	 *    "bsfq %1,%0\n"
	 *    "jnz 1f\n"
	 *    "mov %2,%0\n"
	 *    "1:"
	 *  - Input operands: The first input operand holds value, INVALID_BIT_INDEX is the second input operand
	 *    that will be embedded into the assembly code used as the source operand of mov instruction.
	 *  - Output operands: The index of the least significant bit which is set in \a value is stored to variable
	 *    ret.
	 *  - Clobbers: None */
	asm volatile("	bsfq	%1, %0\n"
		     "	jnz	1f\n"
		     "	mov	%2, %0\n"
		     "1:"
		     : "=r"(ret)
		     : "rm"(value), "i"(INVALID_BIT_INDEX));
	/** Return the index of the least significant bit which is set in \a value */
	return (uint16_t)ret;
}

#define build_bitmap_set(name, op_len, op_type, lock)                                                               \
	static inline void name(uint16_t nr_arg, volatile op_type *addr)                                            \
	{                                                                                                           \
		uint16_t nr;                                                                                        \
		nr = nr_arg & ((8U * sizeof(op_type)) - 1U);                                                        \
		asm volatile(lock "or" op_len " %1, %0" : "+m"(*addr) : "r"((op_type)(1UL << nr)) : "cc", "memory"); \
	}

/**
 * @brief Set the \a nr_arg-th bit in the integer pointed by \a addr. This operation is not protected by the bus lock.
 *
 * @param[in]    nr_arg The index of the bit to be set.
 * @param[inout] addr Pointer to the integer where the bit is to be set.
 *
 * @return None
 *
 * @pre addr != NULL
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be set
 * in the integer pointed by \a addr.
 *
 * 2. Set nr to the value calculated by \a nr_arg bitwise AND with 63.
 *
 * 3. Set nr-th bit in (*addr) to 1 without the protection by the software controlled bus lock (LOCK prefix).
 *
 * The third sequence is the instruction template listing below:
 * - Instruction template
 *   "orq" " %1,%0"
 *
 * Input operands: register operand holds the value calculated by shifting 1 to the left by (\a nr-arg & 63).
 * Output operands: \a *addr with read attribute will be holden by memory address addr, and the value calculated by
 * set (\a nr_arg & 63)-th bit in (*addr) to 1 is stored to addr.
 * Clobbers: cc, memory
 */
build_bitmap_set(bitmap_set_nolock, "q", uint64_t, "")

/**
 * @brief Set the \a nr_arg-th bit atomically in the integer pointed by \a addr.
 *
 * @param[in]    nr_arg The index of the bit to be set.
 * @param[inout] addr Pointer to the integer where the bit is to be set.
 *
 * @return None
 *
 * @pre addr != NULL
 * @post This bit setting operation shall be atomic.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Yes
 * @threadsafety Yes
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be set
 * in the integer pointed by \a addr.
 *
 * 2. Set nr to the value calculated by \a nr_arg bitwise AND with 63.
 *
 * 3. Set nr-th bit in (*addr) to 1 with the protection by the software controlled bus lock (LOCK prefix).
 *
 * The third sequence is the instruction template listing below:
 * - Instruction template
 *   BUS_LOCK "orq" " %1,%0"
 *
 * Input operands: register operand holds the value calculated by shifting 1 to the left by (\a nr-arg & 63).
 * Output operands: \a *addr with read attribute will be holden by memory address addr, and the value calculated by
 * set (\a nr_arg & 63)-th bit in (*addr) to 1 is stored to addr.
 * Clobbers: cc, memory
 */
build_bitmap_set(bitmap_set_lock, "q", uint64_t, BUS_LOCK)

#define build_bitmap_clear(name, op_len, op_type, lock)                  \
	static inline void name(uint16_t nr_arg, volatile op_type *addr) \
	{								 \
		uint16_t nr;						 \
		nr = nr_arg & ((8U * sizeof(op_type)) - 1U);		 \
		asm volatile(lock "and" op_len "	%1, %0"		 \
			     : "+m"(*addr)				 \
			     : "r"((op_type)(~(1UL << (nr))))		 \
			     : "cc", "memory");				 \
	}

/**
 * @brief Clear the \a nr_arg-th bit in the the integer pointed by \a addr. This operation is not protected by the
 * bus lock.
 *
 * @param[in]    nr_arg The index of the bit to be cleared.
 * @param[inout] addr Pointer to the integer where the bit is to be cleared.
 *
 * @return None
 *
 * @pre addr != NULL
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be set
 * in the integer pointed by \a addr.
 *
 * 2. Set nr to the value calculated by \a nr_arg bitwise AND with 63.
 *
 * 3. Set nr-th bit in (*addr) to 0 without the protection by the software controlled bus lock (LOCK prefix).
 *
 * The third sequence is the instruction template listing below:
 * - Instruction template
 *   "andq" " %1,%0"
 *
 * Input operands: register operand holds the negative value calculated by shifting 1 to the left by (\a nr_arg & 63).
 * Output operands: \a *addr with read attribute will be holden by memory address addr, and the value calculated by
 * set (\a nr_arg & 63)-th bit in (*addr) to 0 is stored to addr.
 * Clobbers: cc, memory
 */
build_bitmap_clear(bitmap_clear_nolock, "q", uint64_t, "")

/**
 * @brief Clear the \a nr_arg-th bit atomically in the the integer pointed by \a addr.
 *
 * @param[in]    nr_arg The index of the bit to be cleared.
 * @param[inout] addr Pointer to the integer where the bit is to be cleared.
 *
 * @return None
 *
 * @pre addr != NULL
 * @post This bit clear operation shall be atomic.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be set
 * in the integer pointed by \a addr.
 *
 * 2. Set nr to the value calculated by \a nr_arg bitwise AND with 64.
 *
 * 3. Set nr-th bit in (*addr) to 0 with the protection by the software controlled bus lock (LOCK prefix).
 *
 * The third sequence is the instruction template listing below:
 * - Instruction template
 *   BUS_LOCK "andq" " %1,%0"
 *
 * Input operands: register operand holds the negative value calculated by shifting 1 to the left by (\a nr_arg & 63).
 * Output operands: \a *addr with read attribute will be holden by memory address addr, and the value calculated by
 * set (\a nr_arg & 63)-th bit in (*addr) to 0 is stored to addr.
 * Clobbers: cc, memory
 */
build_bitmap_clear(bitmap_clear_lock, "q", uint64_t, BUS_LOCK)

/**
 * @brief Test whether the \a nr-th bit of the integer pointed by \a addr is set.
 *
 * @param[in] nr The index of the bit which will be tested.
 * @param[in] addr Pointer to the integer where the bit is to be tested.
 *
 * @return Whether the \a nr-th bit of the integer pointed by \a addr is set.
 *
 * @retval true  The \a nr-th bit of the integer pointed by \a addr is set.
 * @retval false The \a nr-th bit of the integer pointed by \a addr is clear.
 *
 * @pre addr != NULL
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * This function does the following actions in sequence:
 *
 * 1. Use bt instruction to store the (\a nr-th & 63) bit of the integer pointed by \a addr in CF flag.
 *
 * 2. Use sbb instruction to subtract ret from ret plus the carry(CF) flag and then update the result into the stack
 *    variable ret.
 *
 * 3. Return true if ret is non-zero, otherwise, return false.
 */
static inline bool bitmap_test(uint16_t nr, const volatile uint64_t *addr)
{
	/** Declare the following local variables of type int32_t.
	 *  - ret representing whether the \a nr-th bit is set in the integer pointed by \a addr, (non-zero value
	 *    indicates that the corresponding bit is 1, otherwise, that bit is 0), initialized as 0. */
	int32_t ret = 0;
	/** Test whether the (\a nr-th & 63) bit is set in the integer pointed by \a addr.
	 *  - Instruction template:
	 *    "btq %q2,%1\nsbbl %0, %0"
	 *  - Input operands: The first memory input operand holds *addr, the second input operand holds
	 *    (uint64_t)(nr & 0x3f).
	 *  - Output operands: Whether the (\a nr-th & 63) bit is set in \a *addr is stored to variable ret.
	 *  - Clobbers: memory, cc */
	asm volatile("btq	%q2, %1\n"
		     "sbbl	%0, %0"
		     : "=r"(ret)
		     : "m"(*addr), "r"((uint64_t)(nr & 0x3fU))
		     : "cc", "memory");
	/** Return (ret != 0). */
	return (ret != 0);
}

#define build_bitmap_testandset(name, op_len, op_type, lock)             \
	static inline bool name(uint16_t nr_arg, volatile op_type *addr) \
	{								 \
		uint16_t nr;						 \
		int32_t ret = 0;					 \
		nr = nr_arg & ((8U * sizeof(op_type)) - 1U);		 \
		asm volatile(lock "bts" op_len "	%2, %1\n"	 \
			     "sbbl			%0, %0"		 \
			     : "=r"(ret), "=m"(*addr)			 \
			     : "r"((op_type)nr)				 \
			     : "cc", "memory");				 \
		return (ret != 0);					 \
	}

/**
 * @brief Test and set the \a nr_arg-th bit in the integer pointed by \a addr.
 *
 * Test whether the \a nr_arg-th bit is set in the integer pointed by \a addr and set the \a nr_arg-th bit in the
 * integer pointed by \a addr.
 *
 * @param[in]    nr_arg The index of the bit to be tested and set.
 * @param[inout] addr Pointer to the integer where the bit is to be tested and set.
 *
 * @return Whether the \a nr-th bit of the integer pointed by \a addr is set.
 *
 * @pre addr != NULL
 * @post This bit test and set operation shall be atomic.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be
 * tested and set in the integer pointed by \a addr.
 *
 * 2. Declare a local variable of type int32_t ret representing whether the \a nr-th bit is set in the integer pointed
 *    by \a addr, the value -1 express that the corresponding bit is 1, otherwise is 0, initialized as 0.
 *
 * 3. Set nr to the value calculated by \a nr_arg bitwise AND with 63.
 *
 * 4. Use bts instruction to selects the bit in \a addr at the bit-position designated by the bit offset operand nr,
 * stores the value of the bit in the CF flag, and sets the selected bit in the integer pointed by \a addr, the btr
 * instruction is executed atomically.
 *
 * 5. Stores the value in ret, where the value is 0 minus CF flag.
 *
 * 6. Return true if ret is non-zero, otherwise, return false.
 *
 * The fourth and fifth sequence are composed into the instruction template listing below:
 * - Instruction template
 *   BUS_LOCK "btsq %2,%1\n
 *   sbbl %0,%0"
 *
 * Input operands: register operand holds the the value calculated by bitwise AND nr_arg with 63.
 * Output operands: The value calculated by set (\a nr_arg & 63)-th bit is stored to the first output operand. Whether
 * the bit is set in (\a nr_arg & 63)-th bit in (*addr) is stored to the second output operand.
 * Clobbers: cc, memory
 */
build_bitmap_testandset(bitmap_test_and_set_lock, "q", uint64_t, BUS_LOCK)

#define build_bitmap_testandclear(name, op_len, op_type, lock)           \
	static inline bool name(uint16_t nr_arg, volatile op_type *addr) \
	{								 \
		uint16_t nr;						 \
		int32_t ret = 0;					 \
		nr = nr_arg & ((8U * sizeof(op_type)) - 1U);		 \
		asm volatile(lock "btr" op_len "	%2, %1\n"	 \
			     "	sbbl			%0, %0"		 \
			     : "=r"(ret), "=m"(*addr)			 \
			     : "r"((op_type)nr)				 \
			     : "cc", "memory");				 \
		return (ret != 0);					 \
	}

/**
 * @brief Test and clear the \a nr_arg-th bit in the integer pointed by \a addr.
 *
 * Test whether the nr_arg-th bit is set in the integer pointed by \a addr and clear the \a nr_arg-th bit in the
 * integer pointed by \a addr.
 *
 * @param[in]    nr_arg The index of the bit to be tested and cleared.
 * @param[inout] addr Pointer to the integer where the bit is to be tested and cleared.
 *
 * @return Whether the \a nr_arg-th bit of the integer pointed by \a addr is set.
 *
 * @pre addr != NULL
 * @post This bit test and set operation shall be atomic.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 *
 * This function does the following actions in sequence:
 *
 * 1. Declare a local variable of type uint16_t,  'nr' representing the effective index of the bit which is to be
 * tested and cleared in the integer pointed by \a addr.
 *
 * 2. Declare a local variable of type int32_t ret representing whether the \a nr-th bit is set in the integer pointed
 *    by \a addr, the value -1 express that the corresponding bit is 1, otherwise is 0, initialized as 0.
 *
 * 3. Set nr to the value calculated by \a nr_arg bitwise AND with Y, where Y is multiply 8 by sizeof(uint64_t)
 * minus 1.
 *
 * 4. Use btr instruction to selects the bit in \a addr at the bit-position designated by the bit offset operand nr,
 * stores the value of the bit in the CF flag, and clears the selected bit in the intger pointed by \a addr, the btr
 * instruction is executed atomically.
 *
 * 5. Stores the value in ret, where the value is 0 minus CF flag.
 *
 * 6. Return true if ret is non-zero, otherwise, return false.
 *
 * The fourth and fifth sequence are composed into the instruction template listing below:
 * - Instruction template
 *   BUS_LOCK "btrq %2,%1\n
 *   sbbl %0,%0"
 *
 * Input operands: register operand holds the the value calculated by bitwise AND nr_arg with 63.
 * Output operands: The value calculated by clear (\a nr_arg & 63)-th bit is stored to the first output operand. Whether
 * the bit is set in (\a nr_arg & 63)-th bit in (*addr) is stored to the second memory operand.
 * Clobbers: cc, memory
 */
build_bitmap_testandclear(bitmap_test_and_clear_lock, "q", uint64_t, BUS_LOCK)

/**
 * @}
 */

#endif /* BITS_H*/
