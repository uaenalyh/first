/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>

/**
 * @defgroup debug debug
 * @brief Providing debug APIs and macros which is used to describe the severity of debug messages.
 *
 * Implementation of debug APIs to initialize console, set console, dump exception, log messages, initialize shell,
 * trace event, initialize 16550 UART. All of these APIs are unused in release version. This module provide macros
 * to describe the severity of debug messages.
 *
 * This module interacts with the following external interface:
 *
 * bsp_fatal_error - Defined in Board Support Package, after calling it the hypervisor will enter safety state.
 *
 * This module is decomposed into the following files:
 *
 * - console.h       Declare the functions related to console operations, including initialization, timer setup, and
 *                   task kick-off.
 * - dump.h          Declare a function related to exception information dumping.
 * - logmsg.h        Providing log APIs, log levels.
 * - profiling.h     Declare the functions related to profiling operations, including saving the information of the
 *                   specific virtual CPU, initialize the profiling utility.
 * - shell.h         Declare a function that used to do initialize the hypervisor shell which is solely for debugging.
 * - trace.h         Declare trace APIs and define trace event that shall be provided by debug module.
 * - uart16550.h     Declare an 16550 UART initialization API that shall be provided by the debug module.
 * - console.c       Implement console APIs that shall be provided by the debug module.
 * - dump.c          Implement a function related to exception information dumping.
 * - logmsg.c        Implement log APIs that shall be provided by the debug module.
 * - profiling.c     Implement profiling APIs that shall be provided by the debug module.
 * - trace.c         Implement trace APIs that shall be provided by the debug module.
 * - uart16550.c     Implement an 16550 UART initialization API that shall be provided by the debug module.
 * - vuart.c         Implement virtual UART APIs that shall be provided by the debug module.
 */

/**
 * @file
 * @brief This file declares the functions related to log operations, including initialization and log message printing.
 *
 * This file is decomposed into the following functions:
 *
 * - init_logmsg(flags)            Initialize the logging service used internally by the hypervisor. No operation in
 *                                 release version.
 * - do_logmsg(severity, fmt, ...) Log a string with the given severity. No operation in release version.
 */

/**
 * @brief Initialize the logging service used internally by the hypervisor. No operation in release version.
 *
 * @param[in]    flags An integer used for configuration of output method. Not used in release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_logmsg(__unused uint32_t flags)
{
}

/**
 * @brief Log a string with the given severity. No operation in release version.
 *
 * @param[in]    severity An integer means the severity of the log. Not used in release version.
 * @param[in]    fmt      A format string. Not used in release version.
 * @param[in]    ...      Variadic argument used to be introduced after meet '%' in the \a fmt immediately followed
 *                        conversion specifier, where conversion specifier is a character that specifies the type of
 *                        conversion to be applied. Not used in release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void do_logmsg(__unused uint32_t severity, __unused const char *fmt, ...)
{
}

/**
 * @}
 */
