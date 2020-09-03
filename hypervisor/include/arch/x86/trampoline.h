/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRAMPOLINE_H
#define TRAMPOLINE_H

/**
 * @addtogroup boot
 *
 * @{
 */

/**
 * @file
 * @brief Declare trampoline APIs used to initialize the environment before start APs.
 */

#include <types.h>

/* external symbols that are helpful for relocation */
extern uint8_t trampoline_fixup_cs;
extern uint8_t trampoline_fixup_ip;
extern uint8_t trampoline_fixup_target;
extern uint8_t cpu_boot_page_tables_start;
extern uint8_t cpu_boot_page_tables_ptr;
extern uint8_t trampoline_gdt_ptr;
extern uint8_t trampoline_start64_fixup;

/**
 * @}
 */

#endif /* TRAMPOLINE_H */
