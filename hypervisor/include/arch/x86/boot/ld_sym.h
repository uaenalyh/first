/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LD_SYM_H
#define LD_SYM_H

/**
 * @addtogroup boot
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all global variables that shall be provided by the boot module.
 */

/**
 * @brief Global uint8_t variable whose address is the host virtual address of the first byte after the .bss
 * segment in the loaded hypervisor image.
 */
extern uint8_t ld_text_end;

/**
 * @brief Global uint8_t variable whose address is the host virtual address of the very beginning of the
 * .bss segment in the loaded hypervisor image.
 */
extern uint8_t ld_bss_start;

/**
 * @brief Global uint8_t variable whose address is the host virtual address of the first byte after the .bss
 * segment in the loaded hypervisor image.
 */
extern uint8_t ld_bss_end;

/**
 * @brief Global uint8_t variable whose address is the host virtual address of the very beginning of the
 * .trampoline segment in the loaded hypervisor image.
 */
extern const uint8_t ld_trampoline_load;

extern uint8_t ld_trampoline_start;
extern uint8_t ld_trampoline_end;

/**
 * @}
 */

#endif /* LD_SYM_H */
