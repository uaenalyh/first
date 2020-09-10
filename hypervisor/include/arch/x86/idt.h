/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IDT_H
#define IDT_H

/**
 * @addtogroup hwmgmt_irq
 *
 * @{
 */

/**
 * @file
 * @brief This file declares data structures and macros for IDT that is used by the hwmgmt.irq module.
 *
 */

#include <types.h>

#ifndef ASSEMBLER

/**
 * @brief The macro defines number of the HOST IDT entries.
 */
#define HOST_IDT_ENTRIES (0x100U)

/**
 * @brief Union to store the 64-Bit Mode IDT information.
 *
 * Union to store the Segment Selector, Offset 63..0, P, DPL, TYPE, IST and other reserved
 * bits of the 64-Bit IDT Gate Descriptor.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
union idt_64_descriptor {
	/**
	 * @brief The variable to store the whole 64-Bit IDT Gate Descriptor.
	 */
	uint64_t value;
	/**
	 * @brief The structure to store the whole 64-Bit IDT Gate Descriptor.
	 */
	struct {
		/**
		 * @brief The union to store low 32 bits of the 64-Bit IDT Gate Descriptor.
		 */
		union {
			/**
			 * @brief The variable to store low 32 bits of the 64-Bit IDT Gate Descriptor.
			 */
			uint32_t value;
			/**
			 * @brief The structure to store low 32 bits of the 64-Bit IDT Gate Descriptor.
			 */
			struct {
				/**
				 * @brief The Offset 15..0 of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t offset_15_0 : 16;
				/**
				 * @brief The Segment Selector of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t seg_sel : 16;
			} bits;
		} low32;
		/**
		 * @brief The union to store bits 32-63 of the 64-Bit IDT Gate Descriptor.
		 */
		union {
			/**
			 * @brief The variable to store bits 32-63 of the 64-Bit IDT Gate Descriptor.
			 */
			uint32_t value;
			/**
			 * @brief The structure to store bits 32-63 of the 64-Bit IDT Gate Descriptor.
			 */
			struct {
				/**
				 * @brief IST bits of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t ist : 3;
				/**
				 * @brief bit 35 of the 64-Bit IDT Gate Descriptor.
				 * This bit shall always be set to 0.
				 */
				uint32_t bit_3_clr : 1;
				/**
				 * @brief bit 36 of the 64-Bit IDT Gate Descriptor.
				 * This bit shall always be set to 0.
				 */
				uint32_t bit_4_clr : 1;
				/**
				 * @brief bits 37-39 of the 64-Bit IDT Gate Descriptor.
				 * These bits shall always be set to 0.
				 */
				uint32_t bits_5_7_clr : 3;
				/**
				 * @brief Type bits of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t type : 4;
				/**
				 * @brief bit 44 of the 64-Bit IDT Gate Descriptor.
				 * This bit shall always be set to 0.
				 */
				uint32_t bit_12_clr : 1;
				/**
				 * @brief DPL bits of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t dpl : 2;
				/**
				 * @brief P bit of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t present : 1;
				/**
				 * @brief Offset 31..16 of the 64-Bit IDT Gate Descriptor.
				 */
				uint32_t offset_31_16 : 16;
			} bits;
		} high32;
		/**
		 * @brief Offset 63..32 of the 64-Bit IDT Gate Descriptor.
		 */
		uint32_t offset_63_32;
		/**
		 * @brief Reserved bits of the 64-Bit IDT Gate Descriptor.
		 */
		uint32_t rsvd;
	} fields;
} __aligned(8);

/**
 * @brief Data structure to store the IDT.
 *
 * Data structure to store all these IDT descriptors with number of HOST_IDT_ENTRIES.
 *
 * @consistency N/A
 *
 * @alignment 8
 *
 * @remark N/A
 */
struct host_idt {
	/**
	 * @brief all these IDT descriptors with number of HOST_IDT_ENTRIES
	 */
	union idt_64_descriptor host_idt_descriptors[HOST_IDT_ENTRIES];
} __aligned(8);

/**
 * @brief Data structure representing the content to be loaded to host IDTR.
 *
 * Data structure to store the length of the host IDT and pointer which points to host IDT.
 *
 * @consistency N/A
 *
 * @alignment N/A
 *
 * @remark This data structure shall be packed.
 */
struct host_idt_descriptor {
	/**
	 * @brief length of the host IDT.
	 */
	uint16_t len;
	/**
	 * @brief pointer which points to host IDT.
	 */
	struct host_idt *idt;
} __packed;

/**
 * @brief This global variable stores all the host IDT descriptors with number of HOST_IDT_ENTRIES.
 *
 */
extern struct host_idt HOST_IDT;
/**
 * @brief This global variable stores the content to be loaded to host IDTR.
 *
 */
extern struct host_idt_descriptor HOST_IDTR;

#else

/**
 * @brief The macro defines the size(in bytes) of Interrupt Descriptor Table (IDT) descriptor.
 */
#define X64_IDT_DESC_SIZE (0x10)
/**
 * @brief The macro defines number of the HOST IDT entries.
 */
#define HOST_IDT_ENTRIES  (0x100)
/**
 * @brief The macro defines size of the HOST IDT.
 */
#define HOST_IDT_SIZE     (HOST_IDT_ENTRIES * X64_IDT_DESC_SIZE)

#endif /* end #ifndef ASSEMBLER */

/**
 * @}
 */

#endif /* IDT_H */
