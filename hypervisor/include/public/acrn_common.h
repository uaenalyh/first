/*
 * common definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACRN_COMMON_H
#define ACRN_COMMON_H

/**
 * @defgroup lib_public lib.public
 * @ingroup lib
 * @brief Definition of public values of data structures
 *
 * This module defines the values (in the form of object-like macros) and data structures that represent vCPU registers
 * and I/O requests. While they are used by ACRN hypervisor only at this moment, they are designed to be shared with a
 * privilege VM in order to enable device sharing and VM lifecycle management, which is why they are separated as a
 * standalone module.
 *
 * This module does not expose any function and only relies on the lib.util module which provides definitions of basic
 * data types such as uint32_t.
 *
 * Usage:
 * - The modules hwmgmt.cpu and hwmgmt.configs use the macros defining page table operation types and invalid addresses.
 * - The module vp-base.vm-config uses the macros defining the guest flags.
 * - The module vp-base.vcpu uses the types defining the set of guest registers and format of descriptor registers.
 * - The module vp-dm.io-req uses the macros and types defining accesses to I/O registers from VMs.
 *
 * @{
 */

/**
 * @file
 * @brief Definition of vCPU registers and I/O requests
 *
 * This file defines the values (in the form of object-like macros) and data structures that represent accesses to I/O
 * registers and contents of hypervisor-managed guest registers including general-purpose registers, descriptor
 * registers, segment selectors, part of control registers and a few model-specific registers.
 */

#include <types.h>

#define REQUEST_READ  0U    /**< A value indicating an I/O register access is a read */
#define REQUEST_WRITE 1U    /**< A value indicating an I/O register access is a write */

/* Generic VM flags from guest OS */
#define GUEST_FLAG_HIGHEST_SEVERITY     (1UL << 6U) /**< Whether has the highest severity */

/**
 * @brief Representation of a port I/O register access
 *
 * @consistency N/A
 *
 * @alignment 8
 */
struct pio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief Port address of the I/O access
	 */
	uint64_t address;

	/**
	 * @brief Width of the I/O access in byte
	 */
	uint64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	uint32_t value;
} __aligned(8);

/**
 * @brief Representation of a general I/O request
 *
 * For now only accesses to a port I/O register will be represented by this union. More types of accesses (e.g. access
 * to memory-mapped I/O registers) can be added later.
 *
 * @consistency N/A
 */
union vhm_io_request {
	struct pio_request pio;  /**< Representation of a port I/O register access */
};

/**
 * @brief Guest general purpose registers
 *
 * General-purpose register layout aligned with the general-purpose register indices on VM exits.
 *
 * @consistency N/A
 */
struct acrn_gp_regs {
	uint64_t rax;  /**< Value in the RAX register */
	uint64_t rcx;  /**< Value in the RCX register */
	uint64_t rdx;  /**< Value in the RDX register */
	uint64_t rbx;  /**< Value in the RBX register */
	uint64_t rsp;  /**< Value in the RSP register */
	uint64_t rbp;  /**< Value in the RBP register */
	uint64_t rsi;  /**< Value in the RSI register */
	uint64_t rdi;  /**< Value in the RDI register */
	uint64_t r8;   /**< Value in the R8 register */
	uint64_t r9;   /**< Value in the R9 register */
	uint64_t r10;  /**< Value in the R10 register */
	uint64_t r11;  /**< Value in the R11 register */
	uint64_t r12;  /**< Value in the R12 register */
	uint64_t r13;  /**< Value in the R13 register */
	uint64_t r14;  /**< Value in the R14 register */
	uint64_t r15;  /**< Value in the R15 register */
};

/**
 * @brief Layout of guest descriptor pointers
 *
 * The structure defining how the descriptor stored in memory. Refer Figure 3-11 "Segment Descriptor Tables", Section
 * 3.5.1, Vol. 3, SDM.
 *
 * @consistency N/A
 *
 * @remark This data structure shall be packed.
 */
struct acrn_descriptor_ptr {
	uint16_t limit;       /**< Index of the last valid byte in the descriptor table. */

	/**
	 * @brief Base linear address of the descriptor table.
	 *
	 * This field could be a host linear address or a guest linear address, depending on whether the structure
	 * represents a host or guest descriptor table.
	 */
	uint64_t base;

	uint16_t reserved[3]; /**< Reserved padding to align the size of this structure to 64bit */
} __packed;

/**
 * @brief registers info for vcpu.
 *
 * @consistency N/A
 */
struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;        /**< Values in guest general-purpose registers */
	struct acrn_descriptor_ptr gdt;  /**< Value in the guest global descriptor table register (GDTR) */
	struct acrn_descriptor_ptr idt;  /**< Value in the guest interrupt descriptor table register (IDTR) */

	uint64_t rip;               /**< Value in guest RIP */
	uint64_t cs_base;           /**< Value in the 'base address' field of the guest CS hidden part */
	uint64_t cr0;               /**< Value in guest CR0 */
	uint64_t cr4;               /**< Value in guest CR4 */
	uint64_t cr3;               /**< Value in guest CR3 */
	uint64_t ia32_efer;         /**< Value in guest IA32_EFER */
	uint64_t rflags;            /**< Value in guest RFLAGS */
	uint64_t reserved_64[4];    /**< Reserved for future extensions */

	uint32_t cs_ar;             /**< Value in the 'access information' field of the CS hidden part */
	uint32_t cs_limit;          /**< Value in the 'limit' field of the CS hidden part */
	uint32_t reserved_32[3];    /**< Reserved for future extensions */

	uint16_t cs_sel;            /**< The value in the guest CS selector */
	uint16_t ss_sel;            /**< The value in the guest SS selector */
	uint16_t ds_sel;            /**< The value in the guest DS selector */
	uint16_t es_sel;            /**< The value in the guest ES selector */
	uint16_t fs_sel;            /**< The value in the guest FS selector */
	uint16_t gs_sel;            /**< The value in the guest GS selector */
	uint16_t ldt_sel;           /**< The value in the guest LDTR selector */
	uint16_t tr_sel;            /**< The value in the guest TR selector */

	uint16_t reserved_16[4];    /**< Reserved for future extensions */
};

/**
 * @}
 */

#endif /* ACRN_COMMON_H */
