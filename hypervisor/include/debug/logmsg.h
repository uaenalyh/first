/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LOGMSG_H
#define LOGMSG_H

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief Providing log APIs, log level macros.
 *
 * Implement log APIs that shall be provided by the debug module and provide log levels to describe the severity
 * of debug messages.
 *
 * This file is decomposed into the following functions:
 *
 * - ASSERT(x, ...)     Check the given condition \a x for debugging. Pause the current physical processor if \a x is
 *                      false. No operation in release version.
 * - pr_fatal(...)      Log a message with FATAL severity. No operation in release version.
 * - pr_acrnlog(...)    Log a message with ACRN severity. No operation in release version.
 * - pr_err(...)        Log a message with ERROR severity. No operation in release version.
 * - pr_warn(...)       Log a message with WARN severity. No operation in release version.
 * - pr_info(...)       Log a message with INFO severity. No operation in release version.
 * - pr_dbg(...)        Log a message with DEBUG severity. No operation in release version.
 * - dev_dbg(lvl, ...)  Log a message with the given severity. No operation in release version.
 * - panic(...)         Enter safety state. Log message in debug version.
 */
#include <cpu.h>


/**
 * @brief The log level of FATAL
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of FATAL is 1.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_FATAL 1U
/**
 * @brief The log level of ACRN
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of ACRN is 2.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_ACRN    2U
/**
 * @brief The log level of ERROR
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of ERROR is 3.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_ERROR   3U
/**
 * @brief The log level of WARN
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of WARNING is 4.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_WARNING 4U
/**
 * @brief The log level of INFO
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of INFO is 5.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_INFO    5U
/**
 * @brief The log level of DEBUG
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of DEBUG is 6.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define LOG_DEBUG   6U
/**
 * @brief The log level of DBG_LAPICCPT
 *
 * The severity of console log level, to specify the console level of debugging messages in this module.
 * The level of ACRN_DBG_LAPICPT is 6.
 * There are 6 levels ranging from 1 to 6, the priority of logging message drops with the increasing level.
 */
#define ACRN_DBG_LAPICPT 6U

/**
 * @brief Check the given condition \a x for debugging.
 *
 * Pause the current physical processor if \a x is false in debug version.
 * No operation in release version.
 *
 * @param[in] x A bool value indicating the condition to be checked. Not used in release version.
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
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
#define ASSERT(x, ...) \
	do {           \
	} while (0)

void init_logmsg(uint32_t flags);
void do_logmsg(uint32_t severity, const char *fmt, ...);

#ifndef pr_prefix
/**
 * @brief The predefined prefix for debug messages.
 */
#define pr_prefix
#endif

/**
 * @brief Log a message with FATAL severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_FATAL.
 *   - pr_prefix __VA_ARGS
 */
#define pr_fatal(...)                                        \
	do {                                                 \
		do_logmsg(LOG_FATAL, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with ACRN severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_ACRN.
 *   - pr_prefix __VA_ARGS
 */
#define pr_acrnlog(...)                                     \
	do {                                                \
		do_logmsg(LOG_ACRN, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with ERROR severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
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
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_ERROR.
 *   - pr_prefix __VA_ARGS
 */
#define pr_err(...)                                          \
	do {                                                 \
		do_logmsg(LOG_ERROR, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with WARN severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_WARNING.
 *   - pr_prefix __VA_ARGS
 */
#define pr_warn(...)                                           \
	do {                                                   \
		do_logmsg(LOG_WARNING, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with INFO severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_INFO.
 *   - pr_prefix __VA_ARGS
 */
#define pr_info(...)                                        \
	do {                                                \
		do_logmsg(LOG_INFO, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with DEBUG severity. No operation in release version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of LOG_DEBUG.
 *   - pr_prefix __VA_ARGS
 */
#define pr_dbg(...)                                          \
	do {                                                 \
		do_logmsg(LOG_DEBUG, pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Log a message with the given severity. No operation in release version.
 *
 * @param[in] lvl An integer means the severity of the log. Not used in release version.
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL, HV_TERMINATION
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of \a lvl.
 *   - pr_prefix __VA_ARGS
 */
#define dev_dbg(lvl, ...)                                \
	do {                                             \
		do_logmsg((lvl), pr_prefix __VA_ARGS__); \
	} while (0)

/**
 * @brief Enter safety state. Log message in debug version.
 *
 * @param[in] ... Variadic arguments used to format the string which will be logged. Not used in release version.
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 *
 * Expand to the following steps:
 *
 * - Logging the following information with a log level of FATAL.
 *   - Name of the current function
 *   - Line of source where this macro is used
 * - Logging the following information with a log level of FATAL.
 *   - The variadic parameters given to this macro
 * - Call bsp_fatal_error() defined in Board Support Package and never return in order to transfer the control to Board
 *   Support Package.
 */
#define panic(...)                                                    \
	do {                                                          \
		pr_fatal("PANIC: %s line: %d\n", __func__, __LINE__); \
		pr_fatal(__VA_ARGS__);                                \
		bsp_fatal_error();                                    \
	} while (0)

/**
 * @}
 */

#endif /* LOGMSG_H */
