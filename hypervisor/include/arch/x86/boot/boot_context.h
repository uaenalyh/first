/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_CTX_H
#define BOOT_CTX_H

/**
 * @addtogroup boot
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the macros used in cpu_primary.S of boot module.
 *
 */

#ifdef ASSEMBLER
#define BOOT_CTX_CR0_OFFSET       176
#define BOOT_CTX_CR3_OFFSET       192
#define BOOT_CTX_CR4_OFFSET       184
#define BOOT_CTX_IDT_OFFSET       144
#define BOOT_CTX_GDT_OFFSET       128
#define BOOT_CTX_LDT_SEL_OFFSET   280
#define BOOT_CTX_TR_SEL_OFFSET    282
#define BOOT_CTX_CS_SEL_OFFSET    268
#define BOOT_CTX_SS_SEL_OFFSET    270
#define BOOT_CTX_DS_SEL_OFFSET    272
#define BOOT_CTX_ES_SEL_OFFSET    274
#define BOOT_CTX_FS_SEL_OFFSET    276
#define BOOT_CTX_GS_SEL_OFFSET    278
#define BOOT_CTX_CS_AR_OFFSET     248
#define BOOT_CTX_CS_LIMIT_OFFSET  252
#define BOOT_CTX_EFER_LOW_OFFSET  200
#define BOOT_CTX_EFER_HIGH_OFFSET 204
#define SIZE_OF_BOOT_CTX	  296
#else
/** offset in the boot context to store CR0 */
#define BOOT_CTX_CR0_OFFSET       176U
/** offset in the boot context to store CR3 */
#define BOOT_CTX_CR3_OFFSET       192U
/** offset in the boot context to store CR4 */
#define BOOT_CTX_CR4_OFFSET       184U
/** offset in the boot context to store IDTR */
#define BOOT_CTX_IDT_OFFSET       144U
/** offset in the boot context to store GDTR */
#define BOOT_CTX_GDT_OFFSET       128U
/** offset in the boot context to store selector from LDTR */
#define BOOT_CTX_LDT_SEL_OFFSET   280U
/** offset in the boot context to store selector from TR */
#define BOOT_CTX_TR_SEL_OFFSET    282U
/** offset in the boot context to store CS selector */
#define BOOT_CTX_CS_SEL_OFFSET    268U
/** offset in the boot context to store SS selector */
#define BOOT_CTX_SS_SEL_OFFSET    270U
/** offset in the boot context to store DS selector */
#define BOOT_CTX_DS_SEL_OFFSET    272U
/** offset in the boot context to store ES selector */
#define BOOT_CTX_ES_SEL_OFFSET    274U
/** offset in the boot context to store FS selector */
#define BOOT_CTX_FS_SEL_OFFSET    276U
/** offset in the boot context to store GS selector */
#define BOOT_CTX_GS_SEL_OFFSET    278U
/** offset in the boot context to store CS attribute */
#define BOOT_CTX_CS_AR_OFFSET     248U
/** offset in the boot context to store CS limit */
#define BOOT_CTX_CS_LIMIT_OFFSET  252U
/** offset in the boot context to store EFER low byte */
#define BOOT_CTX_EFER_LOW_OFFSET  200U
/** offset in the boot context to store EFER high byte */
#define BOOT_CTX_EFER_HIGH_OFFSET 204U
/** bytes of the boot context */
#define SIZE_OF_BOOT_CTX  296U
struct acrn_vcpu_regs;
#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* BOOT_CTX_H */
