/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INIT_H
#define INIT_H

/**
 * @addtogroup init
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all the interfaces that will be used at boot up time.
 */

/**
 * @brief Magic number used as a guard on the stack to prevent stack corruption.
 */
#define SP_BOTTOM_MAGIC 0x696e746cUL

void init_primary_pcpu(void);
void init_secondary_pcpu(void);

/**
 * @}
 */

#endif /* INIT_H*/
