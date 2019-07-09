/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOFTIRQ_H
#define SOFTIRQ_H

#define NR_SOFTIRQS		2U

typedef void (*softirq_handler)(uint16_t cpu_id);

void do_softirq(void);
#endif /* SOFTIRQ_H */
