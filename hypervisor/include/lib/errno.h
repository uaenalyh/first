/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ERRNO_H
#define ERRNO_H

/**
 * @addtogroup lib_utils
 *
 * @{
 */

/**
 * @file
 * @brief This file declares external error codes that shall be provided by the lib.utils module.
 */

/** Indicate permission denied */
#define EACCES 13
/**
 * @brief Indicate abnormal conditions in guests where exceptions shall be injected
 */
#define EFAULT 14
/**
 * @brief Indicate that target is busy.
 */
#define EBUSY 16
/**
 * @brief Indicate that no such device.
 */
#define ENODEV 19
/**
 * @brief Indicate that argument is not valid.
 */
#define EINVAL 22
/**
 * @brief Indicate that timeout occurs.
 */
#define ETIMEDOUT 110

/**
 * @}
 */

#endif /* ERRNO_H */
