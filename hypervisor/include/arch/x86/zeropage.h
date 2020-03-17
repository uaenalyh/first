/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEROPAGE_H
#define ZEROPAGE_H

/**
 * @addtogroup vp-base_vm
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the data structure of zeropage.
 *
 * Zeropage is a data structure used to exchange boot information with OS kernels following Linux/x86 boot protocol.
 *
 */
#include <e820.h>

/**
 * @brief Data structure that defines the layout of zeropage.
 *
 * This data structure defines the layout of zeropage; it includes protocol version, e820 table, boot-time parameters,
 * and some other information.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct zero_page {
	/**
	 * @brief Aligned data for this structure
	 */
	uint8_t pad1[0x1e8]; /* 0x000 */
	/**
	 * @brief Number of entries of following e820 table
	 */
	uint8_t e820_nentries; /* 0x1e8 */
	/**
	 * @brief Aligned data for this structure
	 */
	uint8_t pad2[0x8]; /* 0x1e9 */

	struct {
		/**
		 * @brief sectors number of setup code (1 sector == 512bytes)
		 */
		uint8_t setup_sects; /* 0x1f1 */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad1[0x14]; /* 0x1f2 */
		/**
		 * @brief The boot protocol version.
		 */
		uint16_t version;	/* 0x206 */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad8[0x8]; /* 0x208 */
		/**
		 * @brief The type of loader, please refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint8_t loader_type; /* 0x210 */
		/**
		 * @brief Load flag for kernel, please refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint8_t load_flags; /* 0x211 */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad2[0x6]; /* 0x212 */
		/**
		 * @brief the load address of RAMDISK if it is included
		 */
		uint32_t ramdisk_addr; /* 0x218 */
		/**
		 * @brief The size of RAMDISK if it is included.
		 */
		uint32_t ramdisk_size; /* 0x21c */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad3[0x8]; /* 0x220 */
		/**
		 * @brief The load address of bootargs if it's included
		 */
		uint32_t bootargs_addr; /* 0x228 */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad4[0x8]; /* 0x22c */
		/**
		 * @brief Kernel is relocatable or not, please refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint8_t relocatable_kernel; /* 0x234 */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad5[0x13]; /* 0x235 */
		/**
		 * @brief Payload offset, not used in ACRN, please refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint32_t payload_offset; /* 0x248 */
		/**
		 * @brief Payload length, not used in ACRN, please refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint32_t payload_length; /* 0x24c */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad6[0x8]; /* 0x250 */
		/**
		 * @brief A preferred load address for the kernel, refer to Linux kernel: Documentation/x86/boot.txt
		 */
		uint64_t pref_addr; /* 0x258 */    /**<  */
		/**
		 * @brief Aligned data for this structure
		 */
		uint8_t hdr_pad7[8]; /* 0x260 */
	} __packed hdr;
	/**
	 * @brief Aligned data for this structure
	 */
	uint8_t pad3[0x68]; /* 0x268 */
	/**
	 * @brief The entries of the e820 table.
	 */
	struct e820_entry entries[0x80]; /* 0x2d0 */
	/**
	 * @brief Aligned data for this structure
	 */
	uint8_t pad4[0x330]; /* 0xcd0 */
} __packed;

/**
 * @}
 */

#endif
