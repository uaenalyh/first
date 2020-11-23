/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TYPES_H
#define TYPES_H

/**
 * @addtogroup lib_util
 *
 * @{
 */

/**
 * @file
 * @brief This file declares external macros and types that shall be provided by the lib.util module.
 *
 * This file is decomposed into the following macros:
 *
 *     ARRAY_SIZE(arr)    - Get the number of elements in \a arr as is statically defined in the declaration or
 *                          definition of the array.
 *     __aligned(x)       - Restrict the compiler to place instances of a struct/union type or a global variable at
 *                          \a x-byte aligned host virtual addresses.
 *     __packed           - Specify each member of the structure or union is placed to minimize the memory required.
 *     __unused           - Attached to a type, used to specify the variables of that type are possibly unused.
 */

/**
 * @brief Get the number of elements in \a arr as is statically defined in the declaration or definition of the array.
 *
 * @param[in]    arr An array with statically defined capacity.
 *
 * @return The number of elements in \a arr as is statically defined in the declaration or definition of the array.
 *
 * @pre \a arr shall be an array that has at least one element.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to a value calculated by dividing the size of the whole \a arr by size of the single element embedded in
 * \a arr.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Restrict the compiler to place instances of a struct/union type or a global variable at \a x-byte aligned
 * host virtual addresses.
 *
 * @param[in]    x An integer constant indicating the number of bytes the struct, union or global variable shall be
 * aligned to.
 *
 * @pre \a x shall be an integer constant.
 * @pre \a x shall be a power of 2.
 * @pre \a x shall be no more than 4096.
 *
 * @mode NOT_APPLICABLE
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
#define __aligned(x)    __attribute__((aligned(x)))

/**
 * @brief A compiler attribute that specify the members of a structure or union shall be placed with no padding in.
 * between.
 */
#define __packed        __attribute__((packed))
/**
 * @brief A compiler attribute that is attached to formal parameters that are not used within the function.
 */
#define __unused        __attribute__((unused))

#ifndef ASSEMBLER

/**
 * @brief Represent an unsigned 8 bit integer type
 */
typedef unsigned char uint8_t;
/**
 * @brief Represent an unsigned 16 bit integer type
 */
typedef unsigned short uint16_t;
/**
 * @brief Represent a signed 32 bit integer type
 */
typedef signed int int32_t;
/**
 * @brief Represent an unsigned 32 bit integer type
 */
typedef unsigned int uint32_t;
/**
 * @brief Represent an unsigned 64 bit integer type
 */
typedef unsigned long uint64_t;
/**
 * @brief Represent an unsigned 32 bit integer type
 *
 * It is used as the type of integers representing lengths.
 */
typedef unsigned int size_t;

/**
 * @brief Represent an Boolean type
 */
typedef _Bool bool;

#ifndef NULL
/**
 * @brief A wild pointer.
 *
 * Dereferenced the wild pointer will cause page fault exception.
 */
#define NULL ((void *)0)
#endif

#ifndef true
/**
 * @brief The Boolean value of true
 */
#define true ((_Bool)1)
/**
 * @brief The Boolean value of false
 */
#define false ((_Bool)0)
#endif

#ifndef UINT64_MAX
/**
 * @brief The maximum unsigned 64 bit integer value.
 */
#define UINT64_MAX (0xffffffffffffffffUL)
#endif

#ifndef UINT32_MAX
/**
 * @brief The maximum unsigned 32 bit integer value.
 */
#define UINT32_MAX (0xffffffffU)
#endif

#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* INCLUDE_TYPES_H defined */
