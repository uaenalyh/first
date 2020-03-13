/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @brief The prefix used for debug messages in this file.
 */
#define pr_prefix "iommu: "

#include <types.h>
#include <bits.h>
#include <errno.h>
#include <spinlock.h>
#include <page.h>
#include <pgtable.h>
#include <irq.h>
#include <io.h>
#include <mmu.h>
#include <lapic.h>
#include <vtd.h>
#include <timer.h>
#include <logmsg.h>
#include <board.h>
#include <vm_configurations.h>
#include <pci.h>

/**
 * @defgroup hwmgmt_vtd hwmgmt.vtd
 * @ingroup hwmgmt
 * @brief Implementation of all external APIs to support Intel VT-d.
 *
 * This module implements the support of Intel VT-d (Intel Virtualization Technology for Directed I/O).
 * ACRN hypervisor utilizes VT-d to isolate and restrict device accesses to the resources owned by the partition
 * managing the device.
 * The following capabilities are supported in ACRN hypervisor:
 * - I/O device assignment: for flexibly assigning I/O devices to VMs and extending the protection and isolation
 * properties of VMs for I/O operations.
 * - DMA remapping: for supporting address translations for Direct Memory Accesses (DMA) from devices.
 * - Interrupt remapping: for supporting isolation and routing of interrupts from devices to appropriate VMs.
 *
 * Usage:
 * - 'hwmgmt.cpu' module depends on this module to initialize the remapping hardware and enable DMA remapping.
 * - 'hwmgmt.page' module depends on this module to flush cache lines that contain the specified memory region.
 * - 'vp-base.vm' module depends on this module to destroy an IOMMU domain.
 * - 'vp-dm.ptirq' module depends on this module to assign and free the specified interrupt remapping entry.
 * - 'vp-dm.vperipheral' module depends on this module to create the IOMMU domains, assign the specified PCI device
 * to an IOMMU domain, and remove the specified PCI device from an IOMMU domain.
 *
 * Dependency:
 * - This module depends on 'debug' module to log some information in debug phase.
 * - This module depends on 'lib.utils' module to set the contents in the specified memory region to all 0s.
 * - This module depends on 'hwmgmt.time' module to get the current TSC value for debug purpose.
 * - This module depends on 'hwmgmt.mmu' module to clear user/supervisor (U/S) flags for the specified memory region.
 *
 * @{
 */

/**
 * @file
 * @brief This file implements all external APIs that shall be provided by hwmgmt.vtd module.
 *
 * This file implements all external functions that shall be provided by hwmgmt.vtd module and
 * it defines all macros, data structures, and global variables that are used internally in this file.
 * It also defines some helper functions to implement the features that are commonly used in this file.
 * In addition, it defines some decomposed functions to improve the readability of the code.
 *
 * External functions include: iommu_flush_cache, add_iommu_device, remove_iommu_device, create_iommu_domain,
 * destroy_iommu_domain, enable_iommu, init_iommu, dmar_assign_irte, and dmar_free_irte.
 *
 * Decomposed functions include: register_hrhd_units, dmar_register_hrhd, dmar_set_intr_remap_table,
 * dmar_set_root_table, dmar_prepare, dmar_enable, and get_dmar_info.
 *
 * Helper functions to set or get the value of the specified bits in a variable include: dmar_get_bitslice and
 * dmar_set_bitslice.
 *
 * Helper functions to get the base address of the specified table or queue include: get_root_table, get_ctx_table,
 * get_qi_queue, and get_ir_table.
 *
 * Helper functions to access remapping registers include: iommu_read32, iommu_write32, iommu_write64, and
 * dmar_wait_completion.
 *
 * Helper functions to invalidate the cache on remapping hardware include: dmar_issue_qi_request,
 * dmar_invalid_context_cache, dmar_invalid_context_cache_global, dmar_invalid_iotlb, dmar_invalid_iotlb_global,
 * dmar_invalid_iec, and dmar_invalid_iec_global.
 *
 * Helper functions to enable or disable the specified functionality include: dmar_enable_intr_remapping,
 * dmar_enable_qi, dmar_enable_translation, and dmar_disable_translation.
 *
 * Helper functions related to address width include: width_to_level and width_to_aw.
 *
 * Some other miscellaneous helper functions include: vmid_to_domainid and do_action_for_iommus.
 *
 */

/**
 * @brief The log level used for debug messages in this file.
 */
#define ACRN_DBG_IOMMU 6U

/**
 * @brief The stride length (in bits) used for page-table walks with second-level translation.
 *
 * This value is defined in VT-d specification.
 */
#define LEVEL_WIDTH    9U

/**
 * @brief Starting bit position of Present field in low 64-bits of Root Entry.
 *
 * Present field is located at bit [0] in the Root Entry (128-bit).
 */
#define ROOT_ENTRY_LOWER_PRESENT_POS  (0U)
/**
 * @brief Bit mask of Present field in low 64-bits of Root Entry.
 *
 * Present field is located at bit [0] in the Root Entry (128-bit).
 */
#define ROOT_ENTRY_LOWER_PRESENT_MASK (1UL << ROOT_ENTRY_LOWER_PRESENT_POS)
/**
 * @brief Starting bit position of CTP (Context-table Pointer) field in low 64-bits of Root Entry.
 *
 * CTP field is located at bits [63:12] in the Root Entry (128-bit).
 */
#define ROOT_ENTRY_LOWER_CTP_POS      (12U)
/**
 * @brief Bit mask of CTP (Context-table Pointer) field in low 64-bits of Root Entry.
 *
 * CTP field is located at bits [63:12] in the Root Entry (128-bit).
 */
#define ROOT_ENTRY_LOWER_CTP_MASK     (0xFFFFFFFFFFFFFUL << ROOT_ENTRY_LOWER_CTP_POS)

/**
 * @brief Starting bit position of AW (Address Width) field in high 64-bits of Context Entry.
 *
 * AW field is located at bits [66:64] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_UPPER_AW_POS       (0U)
/**
 * @brief Bit mask of AW (Address Width) field in high 64-bits of Context Entry.
 *
 * AW field is located at bits [66:64] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_UPPER_AW_MASK      (0x7UL << CTX_ENTRY_UPPER_AW_POS)
/**
 * @brief Starting bit position of DID (Domain Identifier) field in high 64-bits of Context Entry.
 *
 * DID field is located at bits [87:72] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_UPPER_DID_POS      (8U)
/**
 * @brief Bit mask of DID (Domain Identifier) field in high 64-bits of Context Entry.
 *
 * DID field is located at bits [87:72] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_UPPER_DID_MASK     (0xFFFFUL << CTX_ENTRY_UPPER_DID_POS)
/**
 * @brief Starting bit position of Present field in low 64-bits of Context Entry.
 *
 * Present field is located at bit [0] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_P_POS        (0U)
/**
 * @brief Bit mask of Present field in low 64-bits of Context Entry.
 *
 * Present field is located at bit [0] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_P_MASK       (0x1UL << CTX_ENTRY_LOWER_P_POS)
/**
 * @brief Starting bit position of TT (Translation Type) field in low 64-bits of Context Entry.
 *
 * TT field is located at bits [3:2] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_TT_POS       (2U)
/**
 * @brief Bit mask of TT (Translation Type) field in low 64-bits of Context Entry.
 *
 * TT field is located at bits [3:2] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_TT_MASK      (0x3UL << CTX_ENTRY_LOWER_TT_POS)
/**
 * @brief Starting bit position of SLPTPTR field in low 64-bits of Context Entry.
 *
 * SLPTPTR (Second Level Page Translation Pointer) field is located at bits [63:12] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_SLPTPTR_POS  (12U)
/**
 * @brief Bit mask of SLPTPTR field in low 64-bits of Context Entry.
 *
 * SLPTPTR (Second Level Page Translation Pointer) field is located at bits [63:12] in the Context Entry (128-bit).
 */
#define CTX_ENTRY_LOWER_SLPTPTR_MASK (0xFFFFFFFFFFFFFUL << CTX_ENTRY_LOWER_SLPTPTR_POS)

/**
 * @brief Get the contents of the specified bits in the given value.
 *
 * It is supposed to be called by 'add_iommu_device' and 'remove_iommu_device'.
 *
 * @param[in] var The given value to be manipulated.
 * @param[in] mask The bit mask of the specified bits to be get.
 * @param[in] pos The starting bit position of the specified bits to be get.
 *
 * @return A 64-bit value satisfying the following condition:
 *         the contents of the bits specified by \a mask and \a pos in \a var is equal to the return value,
 *         which indicates that (retval & (mask >> pos)) == ((var & mask) >> pos) and
 *         (retval & (~(mask >> pos))) == 0.
 *
 * @pre (mask & ((1 << pos) - 1)) == 0
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t dmar_get_bitslice(uint64_t var, uint64_t mask, uint32_t pos)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Get the bitwise AND of two operands: \a var and \a mask;
	 *  2. Shift the result of step-1 right for \a pos bits and return it.
	 */
	return ((var & mask) >> pos);
}

/**
 * @brief Set the specified value to the specified bits in the given value.
 *
 * Set the specified value to the specified bits in the given value and return the value after modification.
 *
 * It is supposed to be called only by 'add_iommu_device'.
 *
 * @param[in] var The given value to be manipulated.
 * @param[in] mask The bit mask of the specified bits to be set.
 * @param[in] pos The starting bit position of the specified bits to be set.
 * @param[in] val The specified value to be set.
 *
 * @return A 64-bit value satisfying all of the following conditions:
 *         1. The contents of the bits specified by \a mask and \a pos in the return value is equal to \a val,
 *         which indicates that (retval & mask) == ((val << pos) & mask);
 *         2. The contents of the other bits in the return value is equal to the counterpart in \a var,
 *         which indicates that (retval & (~mask)) == (var & (~mask)).
 *
 * @pre (mask & ((1 << pos) - 1)) == 0
 * @pre val <= (mask >> pos)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint64_t dmar_set_bitslice(uint64_t var, uint64_t mask, uint32_t pos, uint64_t val)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Get the bitwise AND of two operands: \a var and (~mask);
	 *  2. Shift \a val left for \a pos bits;
	 *  3. Get the bitwise AND of two operands: \a mask and the result of the step-2;
	 *  4. Get the bitwise OR of two operands and return it: the result of the step-1 and the result of the step-4.
	 */
	return ((var & ~mask) | ((val << pos) & mask));
}

/* translation type */
/**
 * @brief A flag which indicates that TT (Translation Type) field in Context Entry is set to 00b.
 *
 * With this setting, untranslated requests are translated using second-level paging structures referenced
 * through SLPTPTR field, translated requests and translation requests are blocked.
 *
 */
#define DMAR_CTX_TT_UNTRANSLATED 0x0UL

/**
 * @brief The size (in bytes) of the invalidation queue.
 */
#define DMAR_INVALIDATION_QUEUE_SIZE 4096U
/**
 * @brief The size (in bytes) of each invalidation descriptor submitted into invalidation queue.
 */
#define DMAR_QI_INV_ENTRY_SIZE       16U
/**
 * @brief The number of interrupt remapping entries contained in one 4-KByte page.
 */
#define DMAR_NUM_IR_ENTRIES_PER_PAGE 256U

/**
 * @brief Starting bit position of Status Write field in Invalidation Wait Descriptor.
 *
 * Status Write field is located at bit [5] in the Invalidation Wait Descriptor (128-bit).
 */
#define DMAR_INV_STATUS_WRITE_SHIFT 5U
/**
 * @brief A flag to indicate that the descriptor type is Context-cache Invalidate Descriptor.
 */
#define DMAR_INV_CONTEXT_CACHE_DESC 0x01UL
/**
 * @brief A flag to indicate that the descriptor type is IOTLB Invalidate Descriptor.
 */
#define DMAR_INV_IOTLB_DESC         0x02UL
/**
 * @brief A flag to indicate that the descriptor type is Interrupt Entry Cache Invalidate Descriptor.
 */
#define DMAR_INV_IEC_DESC           0x04UL
/**
 * @brief A flag to indicate that the descriptor type is Invalidation Wait Descriptor.
 */
#define DMAR_INV_WAIT_DESC          0x05UL
/**
 * @brief Bit mask of Status Write Bit in Invalidation Wait Descriptor.
 *
 * Status Write field is located at bit [5] in the Invalidation Wait Descriptor (128-bit).
 */
#define DMAR_INV_STATUS_WRITE       (1UL << DMAR_INV_STATUS_WRITE_SHIFT)
/**
 * @brief A value to indicate that the invalidation request to hardware is not completed.
 */
#define DMAR_INV_STATUS_INCOMPLETE  0UL
/**
 * @brief A value to indicate that the invalidation request to hardware is completed.
 */
#define DMAR_INV_STATUS_COMPLETED   1UL
/**
 * @brief Starting bit position of Status Data field in Invalidation Wait Descriptor.
 *
 * Status Data field is located at bits [63:32] in the Invalidation Wait Descriptor (128-bit).
 */
#define DMAR_INV_STATUS_DATA_SHIFT  32U
/**
 * @brief The initial setting of the Status Data field in an Invalidation Wait Descriptor.
 */
#define DMAR_INV_STATUS_DATA        (DMAR_INV_STATUS_COMPLETED << DMAR_INV_STATUS_DATA_SHIFT)
/**
 * @brief The initial setting of low 64-bits of an Invalidation Wait Descriptor.
 */
#define DMAR_INV_WAIT_DESC_LOWER    (DMAR_INV_STATUS_WRITE | DMAR_INV_WAIT_DESC | DMAR_INV_STATUS_DATA)

/**
 * @brief Starting bit position of EIME field in Interrupt Remapping Table Address Register.
 *
 * EIME (Extended Interrupt Mode Enable) field is located at bit [11] in Interrupt Remapping Table Address Register.
 */
#define DMAR_IR_ENABLE_EIM_SHIFT 11UL
/**
 * @brief Bit mask of EIME (Extended Interrupt Mode Enable) field in Interrupt Remapping Table Address Register.
 *
 * EIME (Extended Interrupt Mode Enable) field is located at bit [11] in Interrupt Remapping Table Address Register.
 */
#define DMAR_IR_ENABLE_EIM       (1UL << DMAR_IR_ENABLE_EIM_SHIFT)

/**
 * @brief Data structure to enumerate different CIRGs (Context Invalidation Request Granularity).
 *
 * Each enumeration member specifies one granularity.
 *
 * It is supposed to be used when hypervisor attempts to submit a context-cache invalidation request.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
enum dmar_cirg_type {
	/**
	 * @brief Reserved.
	 */
	DMAR_CIRG_RESERVED = 0,
	/**
	 * @brief Global Invalidation.
	 *
	 * With this granularity, all context-cache entries cached at the remapping hardware are invalidated.
	 */
	DMAR_CIRG_GLOBAL,
	/**
	 * @brief Domain-Selective Invalidation.
	 *
	 * With this granularity, context-cache entries associated with the specified domain-id are invalidated.
	 * It is not used in current scope, keeping it here for future scope extension.
	 */
	DMAR_CIRG_DOMAIN,
	/**
	 * @brief Device-Selective Invalidation.
	 *
	 * With this granularity, context-cache entries associated with the specified device source-id and domain-id
	 * are invalidated.
	 */
	DMAR_CIRG_DEVICE
};

/**
 * @brief Data structure to enumerate different IIRGs (IOTLB Invalidation Request Granularity).
 *
 * Each enumeration member specifies one granularity.
 *
 * It is supposed to be used when hypervisor attempts to submit an IOTLB invalidation request.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
enum dmar_iirg_type {
	/**
	 * @brief Reserved.
	 */
	DMAR_IIRG_RESERVED = 0,
	/**
	 * @brief Global Invalidation.
	 *
	 * With this granularity, all IOTLB entries are invalidated and all paging-structure-cache entries are
	 * invalidated.
	 */
	DMAR_IIRG_GLOBAL,
	/**
	 * @brief Domain-Selective Invalidation.
	 *
	 * With this granularity, IOTLB entries caching mappings associated with the specified domain-id are
	 * invalidated, and paging-structure-cache entries caching mappings associated with the specified domain-id
	 * are invalidated.
	 */
	DMAR_IIRG_DOMAIN,
	/**
	 * @brief Page-Selective-within-Domain Invalidation.
	 *
	 * Refer to VT-d specification for details about this granularity.
	 * It is not used in current scope, keeping it here for future scope extension.
	 *
	 */
	DMAR_IIRG_PAGE
};

/**
 * @brief Data structure to store the runtime information for each DRHD structure.
 *
 * Data structure to store the runtime information for each DRHD (DMA Remapping Hardware Unit Definition) structure.
 *
 * It is supposed to be used when hypervisor accesses DRHD at runtime.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct dmar_drhd_rt {
	/**
	 * @brief The index allocated for this runtime DRHD structure.
	 */
	uint32_t index;
	/**
	 * @brief The lock used to protect the operations on this runtime DRHD structure.
	 */
	spinlock_t lock;

	/**
	 * @brief A pointer to the structure storing the physical DRHD information associated with this structure.
	 */
	struct dmar_drhd *drhd;

	/**
	 * @brief The base address (host physical address) of the root table associated with this structure.
	 */
	uint64_t root_table_addr;
	/**
	 * @brief The base address (host physical address) of the interrupt remapping table associated with this
	 *        structure.
	 */
	uint64_t ir_table_addr;
	/**
	 * @brief The base address (host physical address) of the invalidation queue associated with this structure.
	 */
	uint64_t qi_queue;
	/**
	 * @brief The offset to the invalidation queue for the command that will be written next by software.
	 */
	uint16_t qi_tail;

	/**
	 * @brief The cached content of Global Command Register.
	 *
	 * Hypervisor would update this value whenever it attempts to write a new value to Global Command Register.
	 * Hardware will synchronize this value to Global Status Register after the command is serviced.
	 * Thus, this value also indicates the remapping hardware status.
	 */
	uint32_t gcmd;
};

/**
 * @brief Data structure to store the context tables allocated to one DRHD structure.
 *
 * It is supposed to be used when hypervisor accesses the context table.
 *
 * @consistency N/A
 * @alignment 4096
 *
 * @remark N/A
 */
struct context_table {
	/**
	 * @brief The context tables allocated to one DRHD structure.
	 *
	 * For each DRHD structure, there are CONFIG_IOMMU_BUS_NUM root entries to cover the PCI bus number space.
	 * Each root entry contains a context table pointer to the dedicated context table (4-KByte in size) for it.
	 * Consequently, there are CONFIG_IOMMU_BUS_NUM context tables allocated to one DRHD structure.
	 */
	struct page buses[CONFIG_IOMMU_BUS_NUM];
};

/**
 * @brief Data structure to store the interrupt remapping table allocated to one DRHD structure.
 *
 * It is supposed to be used when hypervisor accesses the interrupt remapping table.
 *
 * @consistency N/A
 * @alignment 4096
 *
 * @remark N/A
 */
struct intr_remap_table {
	/**
	 * @brief The interrupt remapping table allocated to one DRHD structure.
	 *
	 * For each DRHD structure, it would support at most CONFIG_MAX_IR_ENTRIES interrupt remapping entries,
	 * where CONFIG_MAX_IR_ENTRIES is a multiple of DMAR_NUM_IR_ENTRIES_PER_PAGE and it is smaller than or
	 * equal to 2^16.
	 * And one 4-KByte page contains DMAR_NUM_IR_ENTRIES_PER_PAGE interrupt remapping entries.
	 * Consequently, the number of the 4-KByte pages required by one DRHD structure is
	 * (CONFIG_MAX_IR_ENTRIES / DMAR_NUM_IR_ENTRIES_PER_PAGE).
	 */
	struct page tables[CONFIG_MAX_IR_ENTRIES / DMAR_NUM_IR_ENTRIES_PER_PAGE];
};

/**
 * @brief Get the 4-KByte aligned root table for the specified DRHD structure.
 *
 * For each DRHD structure present in the platform, there is a DRHD index allocated to it by hypervisor.
 * And each DRHD structure has a dedicated root table (4-KByte in size) for it.
 * Consequently, the root table for the specified DRHD structure could be referenced with the DRHD index.
 *
 * It is supposed to be called by 'dmar_set_root_table'.
 *
 * @param[in] dmar_index The DRHD index allocated to the specified DRHD structure, where the to-be-get root table
 *                       belongs to.
 *
 * @return A pointer to the root table associated with the given DRHD index.
 *
 * @pre dmar_index < DRHD_COUNT
 *
 * @post return != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint8_t *get_root_table(uint32_t dmar_index)
{
	/** Declare the following static array of type 'struct page'.
	 *  - root_tables representing an array that stores the root tables allocated to all DRHD structures
	 *  (the number of elements in this array is DRHD_COUNT and this array has an alignment of 4096 Bytes),
	 *  initialized as all 0s.
	 */
	static struct page root_tables[DRHD_COUNT] __aligned(PAGE_SIZE);
	/** Return root_tables[dmar_index].contents, which points to the root table whose corresponding
	 *  DRHD index is \a dmar_index */
	return root_tables[dmar_index].contents;
}

/**
 * @brief Get the 4-KByte aligned context table according to the given DRHD index and the given bus number.
 *
 * For each DRHD structure present in the platform, there is a DRHD index allocated to it by hypervisor.
 * For each DRHD structure, there are CONFIG_IOMMU_BUS_NUM root entries to cover the PCI bus number space,
 * and each root entry contains a context table pointer to the dedicated context table (4-KByte in size) for it.
 * Consequently, the context table for the specified DRHD structure and the specified PCI bus could be referenced
 * with the DRHD index and the bus number.
 *
 * It is supposed to be called by 'add_iommu_device'.
 *
 * @param[in] dmar_index The DRHD index allocated to the specified DRHD structure, where the to-be-get context table
 *                       belongs to.
 * @param[in] bus_no The bus number of the specified PCI bus, which is associated with the to-be-get context table.
 *
 * @return A pointer to the context table associated with the given DRHD index and the given bus number.
 *
 * @pre dmar_index < DRHD_COUNT
 *
 * @post return != NULL
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint8_t *get_ctx_table(uint32_t dmar_index, uint8_t bus_no)
{
	/** Declare the following static array of type 'struct context_table'.
	 *  - ctx_tables representing an array that stores the context tables allocated to all DRHD structures
	 *  (the number of elements in this array is DRHD_COUNT and this array has an alignment of 4096 Bytes),
	 *  initialized as all 0s.
	 */
	static struct context_table ctx_tables[DRHD_COUNT] __aligned(PAGE_SIZE);
	/** Return ctx_tables[dmar_index].buses[bus_no].contents, which points to the context table whose
	 *  corresponding bus number is \a bus_no and it belongs to the DRHD structure whose DRHD index is
	 *  \a dmar_index */
	return ctx_tables[dmar_index].buses[bus_no].contents;
}

/**
 * @brief Get the 4-KByte aligned invalidation queue for the specified DRHD structure.
 *
 * For each DRHD structure present in the platform, there is a DRHD index allocated to it by hypervisor.
 * And each DRHD structure has a dedicated invalidation queue (4-KByte in size) for it.
 * Consequently, the invalidation queue for the specified DRHD structure could be referenced with the DRHD index.
 *
 * It is supposed to be called by 'dmar_enable_qi'.
 *
 * @param[in] dmar_index The DRHD index allocated to the specified DRHD structure, where the to-be-get
 *                       invalidation queue belongs to.
 *
 * @return A pointer to the invalidation queue associated with the given DRHD index.
 *
 * @pre dmar_index < DRHD_COUNT
 *
 * @post return != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint8_t *get_qi_queue(uint32_t dmar_index)
{
	/** Declare the following static array of type 'struct page'.
	 *  - qi_queues representing an array that stores the invalidation queues allocated to all DRHD structures
	 *  (the number of elements in this array is DRHD_COUNT and this array has an alignment of 4096 Bytes),
	 *  initialized as all 0s.
	 */
	static struct page qi_queues[DRHD_COUNT] __aligned(PAGE_SIZE);
	/** Return qi_queues[dmar_index].contents, which points to the invalidation queue whose corresponding
	 *  DRHD index is \a dmar_index */
	return qi_queues[dmar_index].contents;
}

/**
 * @brief Get the 4-KByte aligned interrupt remapping table for the specified DRHD structure.
 *
 * For each DRHD structure present in the platform, there is a DRHD index allocated to it by hypervisor.
 * And each DRHD structure has an interrupt remapping table (a multiple of 4-KByte in size) for it.
 * Consequently, the interrupt remapping table for the specified DRHD structure could be referenced with the DRHD
 * index.
 *
 * It is supposed to be called by 'dmar_set_intr_remap_table'.
 *
 * @param[in] dmar_index The DRHD index allocated to the specified DRHD structure, where the to-be-get
 *                       interrupt remapping table belongs to.
 *
 * @return A pointer to the interrupt remapping table associated with the given DRHD index.
 *
 * @pre dmar_index < DRHD_COUNT
 *
 * @post return != NULL
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static inline uint8_t *get_ir_table(uint32_t dmar_index)
{
	/** Declare the following static array of type 'struct intr_remap_table'.
	 *  - ir_tables representing an array that stores the interrupt remapping tables allocated to all DRHD
	 *  structures (the number of elements in this array is DRHD_COUNT and this array has an alignment of
	 *  4096 Bytes), initialized as all 0s.
	 */
	static struct intr_remap_table ir_tables[DRHD_COUNT] __aligned(PAGE_SIZE);
	/** Return ir_tables[dmar_index].tables[0].contents, which points to the interrupt remapping table
	 *  whose corresponding DRHD index is \a dmar_index */
	return ir_tables[dmar_index].tables[0].contents;
}

/**
 * @brief A global array of type 'struct dmar_drhd_rt' which stores the runtime information for all DRHD structures.
 *
 * In current scope, GPU device is covered by the DRHD structure specified by the first element in 'dmar_drhd_units',
 * and all the other devices are covered by the DRHD structure specified by the second element in 'dmar_drhd_units'.
 * As GPU is not supported in current scope, all DMA remapping and interrupt remapping at runtime are handled by
 * the DRHD structure specified by the second element in 'dmar_drhd_units'.
 *
 * Thus, the second element in 'dmar_drhd_units' is referenced directly at runtime to eliminate the effort to
 * look for the corresponding DRHD structure. This policy applies to following functions: add_iommu_device,
 * remove_iommu_device, dmar_assign_irte, and dmar_free_irte.
 */
static struct dmar_drhd_rt dmar_drhd_units[DRHD_COUNT];

/**
 * @brief A global pointer to a data structure of type 'struct dmar_info' that stores the physical information of
 *        the remapping hardware.
 */
static struct dmar_info *platform_dmar_info = NULL;

/**
 * @brief The maximum number of IOMMU domains.
 *
 * Each virtual machine has a dedicated IOMMU domain for it and domain identifier of 0 is reserved in some cases
 * according to VT-d specification. So, the maximum number of IOMMU domains would be one plus the maximum number of
 * VMs.
 *
 */
#define MAX_DOMAIN_NUM (CONFIG_MAX_VM_NUM + 1U)

/**
 * @brief Get the corresponding IOMMU domain identifier according to the given VM identifier.
 *
 * Each virtual machine has a dedicated IOMMU domain for it and domain identifier of 0 is reserved in some cases
 * according to VT-d specification.
 * For the specified VM, hypervisor allocates the domain identifier as one plus the VM identifier to it.
 *
 * It is supposed to be called by 'add_iommu_device', 'remove_iommu_device', and 'create_iommu_domain'.
 *
 * @param[in] vm_id The given VM identifier.
 *
 * @return The corresponding IOMMU domain identifier according to the given VM identifier.
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint16_t vmid_to_domainid(uint16_t vm_id)
{
	/** Return one plus \a vm_id, which is the corresponding IOMMU domain identifier */
	return vm_id + 1U;
}

static void dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit);

/**
 * @brief Register all DRHD structures in hypervisor.
 *
 * It is supposed to be called by 'init_iommu'.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void register_hrhd_units(void)
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - drhd_rt representing a pointer to a data structure that stores the runtime information for one
	 *  DRHD structure, not initialized. */
	struct dmar_drhd_rt *drhd_rt;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as DRHD index, not initialized. */
	uint32_t i;

	/** For each 'i' ranging from 0 to 'platform_dmar_info->drhd_count - 1' [with a step of 1] */
	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		/** Set 'drhd_rt' to the address of the (i+1)-th element in array 'dmar_drhd_units' */
		drhd_rt = &dmar_drhd_units[i];
		/** Set 'drhd_rt->index' to i, which is the index allocated to the DRHD structure associated with
		 *  'drhd_rt' */
		drhd_rt->index = i;
		/** Set 'drhd_rt->drhd' to the address of the (i+1)-th element in array
		 *  'platform_dmar_info->drhd_units' */
		drhd_rt->drhd = &platform_dmar_info->drhd_units[i];

		/** Call hv_access_memory_region_update with the following parameters, in order to
		 *  clear user/supervisor (U/S) flags for the memory region of the DRHD structure associated with
		 *  'drhd_rt', so that hypervisor (which is in supervisor-mode) could access this memory region.
		 *  - drhd_rt->drhd->reg_base_addr
		 *  - PAGE_SIZE
		 */
		hv_access_memory_region_update((uint64_t)hpa2hva(drhd_rt->drhd->reg_base_addr), PAGE_SIZE);

		/** Call dmar_register_hrhd with the following parameters, in order to register the DRHD structure
		 *  associated with 'drhd_rt' in hypervisor.
		 *  - drhd_rt */
		dmar_register_hrhd(drhd_rt);
	}
}

/**
 * @brief Read a value from the specified 32-bit remapping register.
 *
 * Read a value from the specified 32-bit remapping register, which is placed at a memory-mapped location.
 *
 * This 32-bit remapping register is associated with the specified DRHD structure and
 * it is located with a specified offset in the register set for that DRHD structure.
 * Thus, the base address of this register could be calculated by adding the specified offset to
 * the base address of remapping hardware register-set for the specified DRHD structure.
 *
 * It is supposed to be called when hypervisor attempts to read from a 32-bit remapping register.
 *
 * @param[in] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                      DRHD structure whose remapping register is to be read.
 * @param[in] offset The register offset of the specified 32-bit remapping register.
 *
 * @return The content in the specified 32-bit remapping register.
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a dmar_unit is different among parallel invocation.
 */
static uint32_t iommu_read32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	/** Return the return value of 'mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset))',
	 *  which is the content in the register whose base address (host physical address) is specified by
	 *  'dmar_unit->drhd->reg_base_addr + offset' */
	return mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

/**
 * @brief Write a value to the specified 32-bit remapping register.
 *
 * Write a value to the specified 32-bit remapping register, which is placed at a memory-mapped location.
 *
 * This 32-bit remapping register is associated with the specified DRHD structure and
 * it is located with a specified offset in the register set for that DRHD structure.
 * Thus, the base address of this register could be calculated by adding the specified offset to
 * the base address of remapping hardware register-set for the specified DRHD structure.
 *
 * It is supposed to be called when hypervisor attempts to write a value to a 32-bit remapping register.
 *
 * @param[in] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                      DRHD structure whose remapping register is to be written.
 * @param[in] offset The register offset of the specified 32-bit remapping register.
 * @param[in] value The specified value to be written.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a dmar_unit is different among parallel invocation.
 */
static void iommu_write32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t value)
{
	/** Call mmio_write32 with the following parameters, in order to write \a value to the register
	 *  whose base address (host physical address) is specified by 'dmar_unit->drhd->reg_base_addr + offset'.
	 *  - value
	 *  - hpa2hva(dmar_unit->drhd->reg_base_addr + offset)
	 */
	mmio_write32(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

/**
 * @brief Write a value to the specified 64-bit remapping register.
 *
 * Write a value to the specified 64-bit remapping register, which is placed at a memory-mapped location.
 *
 * This 64-bit remapping register is associated with the specified DRHD structure and
 * it is located with a specified offset in the register set for that DRHD structure.
 * Thus, the base address of this register could be calculated by adding the specified offset to
 * the base address of remapping hardware register-set for the specified DRHD structure.
 *
 * It is supposed to be called when hypervisor attempts to write a value to a 64-bit remapping register.
 *
 * @param[in] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                      DRHD structure whose remapping register is to be written.
 * @param[in] offset The register offset of the specified 64-bit remapping register.
 * @param[in] value The specified value to be written.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a dmar_unit is different among parallel invocation.
 */
static void iommu_write64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint64_t value)
{
	/** Declare the following local variables of type uint32_t.
	 *  - temp representing a temporary variable to specify a 32-bit value to be written, not initialized. */
	uint32_t temp;

	/** Set 'temp' to low 32-bits of \a value */
	temp = (uint32_t)value;
	/** Call mmio_write32 with the following parameters, in order to write 'temp' to a 32-bit memory region
	 *  whose base address (host physical address) is specified by 'dmar_unit->drhd->reg_base_addr + offset'.
	 *  - temp
	 *  - hpa2hva(dmar_unit->drhd->reg_base_addr + offset)
	 */
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));

	/** Set 'temp' to high 32-bits of \a value */
	temp = (uint32_t)(value >> 32U);
	/** Call mmio_write32 with the following parameters, in order to write 'temp' to a 32-bit memory region
	 *  whose base address (host physical address) is specified by 'dmar_unit->drhd->reg_base_addr + offset + 4'.
	 *  - temp
	 *  - hpa2hva(dmar_unit->drhd->reg_base_addr + offset + 4)
	 */
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset + 4U));
}

/**
 * @brief Wait until hardware completes the specified request.
 *
 * This function checks the content in the specified 32-bit remapping register of the specified DRHD structure
 * in a spin-wait loop and terminates the loop when the content is updated as requested.
 *
 * It is supposed to be called when hypervisor attempts to update remapping hardware status and waits the hardware
 * to complete the request.
 *
 * @param[in] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                      DRHD structure whose remapping register is to be checked.
 * @param[in] offset The register offset of the specified 32-bit remapping register.
 * @param[in] mask The bit mask of the specified bits to be checked.
 * @param[in] pre_condition A flag to indicate whether the requested value of the specified bits is 0 or not.
 *                          It is true if the requested value is 0. Otherwise, it is false.
 * @param[inout] status A pointer to a variable representing the content in the specified 32-bit remapping register.
 *                      The variable value after returning from this function could be used by the caller for
 *                      debug purpose.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 * @pre status != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a dmar_unit is different among parallel invocation.
 */
static inline void dmar_wait_completion(
	const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t mask, bool pre_condition, uint32_t *status)
{
	/* variable start isn't used when built as release version */
	/** Declare the following local variables of type uint64_t.
	 *  - start representing current TSC value, initialized as the return value of 'rdtsc()'.
	 *  (This variable is only used for debug purpose.)
	 */
	__unused uint64_t start = rdtsc();
	/** Declare the following local variables of type bool.
	 *  - temp_condition representing a flag to indicate whether the value of the specified bits in the
	 *  specified 32-bit remapping register is 0 or not, not initialized.
	 */
	bool temp_condition;

	while (1) {
		/** Set '*status' to the return value of 'iommu_read32(dmar_unit, offset)', which is the
		 *  content in the 32-bit remapping register specified by \a dmar_unit and \a offset. */
		*status = iommu_read32(dmar_unit, offset);
		/** Set 'temp_condition' to true if the value of the bits specified by \a mask in '*status' is 0,
		 *  otherwise, set 'temp_condition' to false. */
		temp_condition = ((*status & mask) == 0U) ? true : false;

		/** If 'temp_condition' is same as 'pre_condition', indicating that the requested condition is
		 *  satisfied */
		if (temp_condition == pre_condition) {
			/** Terminate the loop */
			break;
		}
		/** Assert when 'rdtsc() - start' is equal to or larger than CYCLES_PER_MS */
		ASSERT(((rdtsc() - start) < CYCLES_PER_MS), "DMAR OP Timeout!");
		/** Call asm_pause without any parameters, in order to improve processor performance in this
		 *  spin-wait loop. */
		asm_pause();
	}
}

/**
 * @brief Flush cache lines that contain the specified memory region.
 *
 * It is supposed to be called when root table, context table or second-level translation table is updated.
 *
 * @param[in] p The base address of the memory region whose corresponding cache lines need to be invalidated.
 * @param[in] size The size of the memory region whose corresponding cache lines need to be invalidated.
 *
 * @return None
 *
 * @pre p != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety The memory region specified by \a p and \a size is not overlapped among parallel invocation.
 */
void iommu_flush_cache(const void *p, uint32_t size)
{

	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter and it is used to calculate the base address of a memory region,
	 *  not initialized. */
	uint32_t i;

	/** For each 'i' ranging from 0 to '(size - 1) / CACHE_LINE_SIZE' [with a step of CACHE_LINE_SIZE] */
	for (i = 0U; i < size; i += CACHE_LINE_SIZE) {
		/** Call clflush with the following parameters, in order to flush the cache line that contains the
		 *  memory region whose base address is specified by '(const char *)p + i'.
		 *  - (const char *)p + i */
		clflush((const char *)p + i);
	}
}

/**
 * @brief Calculate the number of page walk levels corresponding to the given AGAW.
 *
 * Calculate the number of page walk levels corresponding to the given AGAW (Adjusted Guest Address Width).
 * According to VT-d specification, (AGAW - 12) is a multiple of 9 to allow page-table walks with 9-bit stride.
 *
 * The following encodings are defined in VT-d specification:
 * - 39-bit AGAW (3-level page table);
 * - 48-bit AGAW (4-level page table);
 * - 57-bit AGAW (5-level page table).
 *
 * It is supposed to be called by 'width_to_aw'.
 *
 * @param[in] width The given AGAW (in bit).
 *
 * @return The number of page walk levels corresponding to the given AGAW.
 *
 * @pre (width == 39) || (width == 48) || (width == 57)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint8_t width_to_level(uint32_t width)
{
	/** Return the division of 'width - 12' by LEVEL_WIDTH
	 *  ('width - 12' is guaranteed to be a multiple of LEVEL_WIDTH by the pre-condition). */
	return (uint8_t)((width - 12U) / (LEVEL_WIDTH));
}

/**
 * @brief Calculate the value to be set in the AW field of a context entry based on the given AGAW.
 *
 * Calculate the value to be set in the AW (Address Width) field of a context entry based on the given
 * AGAW (Adjusted Guest Address Width).
 *
 * The following encodings are defined in VT-d specification:
 * - AW field is 1H for 39-bit AGAW (3-level page table);
 * - AW field is 2H for 48-bit AGAW (4-level page table);
 * - AW field is 3H for 57-bit AGAW (5-level page table).
 *
 * It is supposed to be called by 'add_iommu_device'.
 *
 * @param[in] width The given AGAW (in bit).
 *
 * @return The value to be set in the AW field of a context entry.
 *
 * @pre (width == 39) || (width == 48) || (width == 57)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline uint8_t width_to_aw(uint32_t width)
{
	/** Return the subtraction of 2H from the return value of 'width_to_level(width)'. */
	return width_to_level(width) - 2U;
}

/**
 * @brief Enable interrupt remapping on the specified DRHD structure.
 *
 * Enable interrupt remapping on the specified DRHD structure if it's not enabled yet.
 *
 * It is supposed to be called by 'dmar_assign_irte'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose interrupt remapping is to be enabled.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void dmar_enable_intr_remapping(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the remapping hardware status, initialized as 0. */
	uint32_t status = 0U;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));
	/** If Interrupt Remapping Enable Bit (Bit 25) of 'dmar_unit->gcmd' is equal to 0, indicating that
	 *  interrupt remapping is disabled on the DRHD structure specified by \a dmar_unit */
	if ((dmar_unit->gcmd & DMA_GCMD_IRE) == 0U) {
		/** Set Interrupt Remapping Enable Bit (Bit 25) of 'dmar_unit->gcmd' to 1 */
		dmar_unit->gcmd |= DMA_GCMD_IRE;
		/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd' to
		 *  Global Command Register associated with \a dmar_unit.
		 *  - dmar_unit
		 *  - DMAR_GCMD_REG
		 *  - dmar_unit->gcmd
		 */
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		/** Call dmar_wait_completion with the following parameters, in order to wait until
		 *  Interrupt Remapping Enable Status Bit (Bit 25) of Global Status Register associated with
		 *  \a dmar_unit is updated to 1 and record the latest content of Global Status Register to 'status'.
		 *  - dmar_unit
		 *  - DMAR_GSTS_REG
		 *  - DMA_GSTS_IRES
		 *  - false
		 *  - &status
		 */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, false, &status);
	}

	/** Call spinlock_release with the following parameters, in order to release the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - __func__
	 *  - status
	 */
	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

/**
 * @brief Enable DMA remapping on the specified DRHD structure.
 *
 * Enable DMA remapping on the specified DRHD structure if it's not enabled yet.
 *
 * It is supposed to be called by 'dmar_enable'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose DMA remapping is to be enabled.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_enable_translation(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the remapping hardware status, initialized as 0. */
	uint32_t status = 0U;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));
	/** If Translation Enable Bit (Bit 31) of 'dmar_unit->gcmd' is equal to 0, indicating that
	 *  DMA remapping is disabled on the DRHD structure specified by \a dmar_unit */
	if ((dmar_unit->gcmd & DMA_GCMD_TE) == 0U) {
		/** Set Translation Enable Bit (Bit 31) of 'dmar_unit->gcmd' to 1 */
		dmar_unit->gcmd |= DMA_GCMD_TE;
		/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd' to
		 *  Global Command Register associated with \a dmar_unit.
		 *  - dmar_unit
		 *  - DMAR_GCMD_REG
		 *  - dmar_unit->gcmd
		 */
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		/** Call dmar_wait_completion with the following parameters, in order to wait until
		 *  Translation Enable Status Bit (Bit 31) of Global Status Register associated with
		 *  \a dmar_unit is updated to 1 and record the latest content of Global Status Register to 'status'.
		 *  - dmar_unit
		 *  - DMAR_GSTS_REG
		 *  - DMA_GSTS_TES
		 *  - false
		 *  - &status
		 */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, false, &status);
	}

	/** Call spinlock_release with the following parameters, in order to release the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));

	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - __func__
	 *  - status
	 */
	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

/**
 * @brief Disable DMA remapping on the specified DRHD structure.
 *
 * Disable DMA remapping on the specified DRHD structure if it's not disabled yet.
 *
 * It is supposed to be called by 'dmar_register_hrhd'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose DMA remapping is to be disabled.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_disable_translation(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the remapping hardware status, not initialized. */
	uint32_t status;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));
	/** If Translation Enable Bit (Bit 31) of 'dmar_unit->gcmd' is equal to 1, indicating that
	 *  DMA remapping is enabled on the DRHD structure specified by \a dmar_unit */
	if ((dmar_unit->gcmd & DMA_GCMD_TE) != 0U) {
		/** Clear Translation Enable Bit (Bit 31) of 'dmar_unit->gcmd' */
		dmar_unit->gcmd &= ~DMA_GCMD_TE;
		/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd' to
		 *  Global Command Register associated with \a dmar_unit.
		 *  - dmar_unit
		 *  - DMAR_GCMD_REG
		 *  - dmar_unit->gcmd
		 */
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		/** Call dmar_wait_completion with the following parameters, in order to wait until
		 *  Translation Enable Status Bit (Bit 31) of Global Status Register associated with
		 *  \a dmar_unit is updated to 0.
		 *  - dmar_unit
		 *  - DMAR_GSTS_REG
		 *  - DMA_GSTS_TES
		 *  - true
		 *  - &status
		 */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, true, &status);
	}

	/** Call spinlock_release with the following parameters, in order to release the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
}

/**
 * @brief Register the specified DRHD structure in hypervisor.
 *
 * It is supposed to be called by 'register_hrhd_units'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure which is to be registered.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit)
{
	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - dmar_unit->index
	 *  - dmar_unit->drhd->reg_base_addr
	 */
	dev_dbg(ACRN_DBG_IOMMU, "Register dmar unit [%d] @0x%lx", dmar_unit->index, dmar_unit->drhd->reg_base_addr);

	/** Call spinlock_init with the following parameters, in order to initialize the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &dmar_unit->lock */
	spinlock_init(&dmar_unit->lock);

	/*
	 * The initialization of "dmar_unit->gcmd" shall be done via reading from Global Status Register rather than
	 * Global Command Register.
	 * According to Chapter 10.4.4 Global Command Register in VT-d spec, Global Command Register is a write-only
	 * register to control remapping hardware. Global Status Register is the corresponding read-only register to
	 * report remapping hardware status.
	 */
	/** Set 'dmar_unit->gcmd' to the return value of 'iommu_read32(dmar_unit, DMAR_GSTS_REG)', which is the
	 *  content in the Global Status Register associated with \a dmar_unit. */
	dmar_unit->gcmd = iommu_read32(dmar_unit, DMAR_GSTS_REG);

	/** Call dmar_disable_translation with the following parameters, in order to disable DMA remapping on
	 *  the DRHD structure specified by \a dmar_unit (if it's not disabled yet).
	 *  - dmar_unit */
	dmar_disable_translation(dmar_unit);
}

/**
 * @brief Submit the specified invalidation request to hardware and wait until hardware completes it.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the context cache, the interrupt entry cache,
 * and the IOTLB and paging structure caches.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 * @param[in] invalidate_desc The specified invalidation descriptor.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a dmar_unit is different among parallel invocation.
 */
static void dmar_issue_qi_request(struct dmar_drhd_rt *dmar_unit, struct dmar_entry invalidate_desc)
{
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - invalidate_desc_ptr representing a pointer to an invalidation descriptor, not initialized. */
	struct dmar_entry *invalidate_desc_ptr;
	/** Declare the following local variables of type uint32_t.
	 *  - qi_status representing the queued invalidation status, not initialized. */
	uint32_t qi_status;
	/** Declare the following local variables of type uint64_t.
	 *  - 'start' representing the TSC value right after hypervisor submits the invalidation request to hardware,
	 *  not initialized. (This variable is only used for debug purpose.) */
	__unused uint64_t start;

	/** Set 'invalidate_desc_ptr' to 'dmar_unit->qi_queue + dmar_unit->qi_tail', which is the pointer to the
	 *  invalidation descriptor in the invalidation queue to be written next by software. */
	invalidate_desc_ptr = (struct dmar_entry *)(dmar_unit->qi_queue + dmar_unit->qi_tail);

	/** Set 'invalidate_desc_ptr->hi_64' to 'invalidate_desc.hi_64', which is the high 64-bits of the specified
	 *  invalidation descriptor */
	invalidate_desc_ptr->hi_64 = invalidate_desc.hi_64;
	/** Set 'invalidate_desc_ptr->lo_64' to 'invalidate_desc.lo_64', which is the low 64-bits of the specified
	 *  invalidation descriptor */
	invalidate_desc_ptr->lo_64 = invalidate_desc.lo_64;
	/** Set 'dmar_unit->qi_tail' to '(dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE',
	 *  which specifies the new base address of the invalidation queue tail. (The invalidation queue is a
	 *  circular buffer and the size of each entry inside it is DMAR_QI_INV_ENTRY_SIZE.)
	 */
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	/** Set 'invalidate_desc_ptr' to 'dmar_unit->qi_queue + dmar_unit->qi_tail', which is the pointer to the
	 *  invalidation descriptor in the invalidation queue to be written next by software.
	 *  It would be an Invalidation Wait Descriptor which is used to check whether hardware completes
	 *  all invalidation requests or not. */
	invalidate_desc_ptr = (struct dmar_entry *)(dmar_unit->qi_queue + dmar_unit->qi_tail);

	/** Set 'invalidate_desc_ptr->hi_64' to the return value of 'hva2hpa(&qi_status)', which is the
	 *  host physical address of 'qi_status' and it specifies the Status Address field in the
	 *  Invalidation Wait Descriptor. The value of 'qi_status' will be used to check if the hardware completes
	 *  the invalidation requests. */
	invalidate_desc_ptr->hi_64 = hva2hpa(&qi_status);
	/** Set 'invalidate_desc_ptr->lo_64' to DMAR_INV_WAIT_DESC_LOWER */
	invalidate_desc_ptr->lo_64 = DMAR_INV_WAIT_DESC_LOWER;
	/** Set 'dmar_unit->qi_tail' to '(dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE',
	 *  which specifies the new base address of the invalidation queue tail. (The invalidation queue is a
	 *  circular buffer and the size of each entry inside it is DMAR_QI_INV_ENTRY_SIZE.)
	 */
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	/** Set 'qi_status' to DMAR_INV_STATUS_INCOMPLETE */
	qi_status = DMAR_INV_STATUS_INCOMPLETE;
	/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->qi_tail' to
	 *  Invalidation Queue Tail Register associated with \a dmar_unit.
	 *  At this moment, the invalidation requests are submitted to hardware.
	 *  - dmar_unit
	 *  - DMAR_IQT_REG
	 *  - dmar_unit->qi_tail
	 */
	iommu_write32(dmar_unit, DMAR_IQT_REG, dmar_unit->qi_tail);

	/** Set 'start' to the return value of 'rdtsc()' */
	start = rdtsc();
	/** Until 'qi_status' is equal to DMAR_INV_STATUS_COMPLETED */
	while (qi_status != DMAR_INV_STATUS_COMPLETED) {
		/** If 'rdtsc() - start' is larger than CYCLES_PER_MS */
		if ((rdtsc() - start) > CYCLES_PER_MS) {
			/** Logging the following information with a log level of 3.
			 *  - __func__ */
			pr_err("DMAR OP Timeout! @ %s", __func__);
		}
		/** Call asm_pause without any parameters, in order to improve processor performance in this
		 *  spin-wait loop. */
		asm_pause();
	}
}

/**
 * @brief Submit the specified context cache invalidation request to hardware and wait until hardware completes it.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the context cache.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 * @param[in] did The specified value for Domain ID field, which is to be set to a context cache invalidate descriptor.
 * @param[in] sid The specified value for Source ID field, which is to be set to a context cache invalidate descriptor.
 * @param[in] fm The specified value for Function Mask field, which is to be set to a context cache invalidate
 *               descriptor.
 * @param[in] cirg The specified value for Context Invalidation Request Granularity field, which is to be set to a
 *                 context cache invalidate descriptor.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 * @pre (cirg == DMAR_CIRG_GLOBAL) || (cirg == DMAR_CIRG_DEVICE)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void dmar_invalid_context_cache(
	struct dmar_drhd_rt *dmar_unit, uint16_t did, uint16_t sid, uint8_t fm, enum dmar_cirg_type cirg)
{
	/** Declare the following local variables of type 'struct dmar_entry'.
	 *  - invalidate_desc representing an invalidation descriptor, not initialized. */
	struct dmar_entry invalidate_desc;

	/** Set 'invalidate_desc.hi_64' to 0 */
	invalidate_desc.hi_64 = 0UL;
	/** Set 'invalidate_desc.lo_64' to DMAR_INV_CONTEXT_CACHE_DESC, indicating that 'invalidate_desc' is
	 *  a context cache invalidation descriptor. */
	invalidate_desc.lo_64 = DMAR_INV_CONTEXT_CACHE_DESC;
	/** Depending on Context Invalidation Request Granularity specified by \a cirg */
	switch (cirg) {
	/** \a cirg is DMAR_CIRG_GLOBAL, indicating a global invalidation request */
	case DMAR_CIRG_GLOBAL:
		/** Set Context Invalidation Request Granularity field (Bits 5:4) in 'invalidate_desc.lo_64' to 1 */
		invalidate_desc.lo_64 |= DMA_CONTEXT_GLOBAL_INVL;
		/** End of case */
		break;
	/** \a cirg is DMAR_CIRG_DEVICE, indicating a device-selective invalidation request */
	case DMAR_CIRG_DEVICE:
		/** Set following fields in 'invalidate_desc.lo_64' to the specified values:
		 *  - Context Invalidation Request Granularity field (Bits 5:4) to 3
		 *  - Domain-ID field (Bits 31:16) to \a did
		 *  - Source-ID field (Bits 47:32) to \a sid
		 *  - Function Mask field (Bits 49:48) to low 2-bits of \a fm
		 */
		invalidate_desc.lo_64 |=
			DMA_CONTEXT_DEVICE_INVL | dma_ccmd_did(did) | dma_ccmd_sid(sid) | dma_ccmd_fm(fm);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Set 'invalidate_desc.lo_64' to 0 */
		invalidate_desc.lo_64 = 0UL;
		/** Logging a message with a log level of 3 to indicate that the default case is reached.
		 *  Pre-condition guarantees that this case would not happen.
		 *  Keeping it here for defensive programming. */
		pr_err("unknown CIRG type");
		/** End of case */
		break;
	}

	/** If 'invalidate_desc.lo_64' is not equal to 0 */
	if (invalidate_desc.lo_64 != 0UL) {
		/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that
		 *  is used to protect the operations on \a dmar_unit.
		 *  - &(dmar_unit->lock) */
		spinlock_obtain(&(dmar_unit->lock));

		/** Call dmar_issue_qi_request with the following parameters, in order to submit the
		 *  invalidation request specified by 'invalidate_desc' to the DRHD structure specified by \a dmar_unit
		 *  and wait until hardware completes it.
		 *  - dmar_unit
		 *  - invalidate_desc
		 */
		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		/** Call spinlock_release with the following parameters, in order to release the spin lock that
		 *  is used to protect the operations on \a dmar_unit.
		 *  - &(dmar_unit->lock) */
		spinlock_release(&(dmar_unit->lock));
	}
}

/**
 * @brief Submit a global context cache invalidation request to hardware and wait until hardware completes it.
 *
 * All context-cache entries cached at the remapping hardware will be invalidated with this invalidation request.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the context cache globally.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_invalid_context_cache_global(struct dmar_drhd_rt *dmar_unit)
{
	/** Call dmar_invalid_context_cache with the following parameters, in order to
	 *  submit a global context cache invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - 0H
	 *  - 0H
	 *  - 0H
	 *  - DMAR_CIRG_GLOBAL
	 */
	dmar_invalid_context_cache(dmar_unit, 0U, 0U, 0U, DMAR_CIRG_GLOBAL);
}

/**
 * @brief Submit the specified IOTLB invalidation request to hardware and wait until hardware completes it.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the IOTLB and paging structure caches.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 * @param[in] did The specified value for Domain ID field, which is to be set to an IOTLB invalidate descriptor.
 * @param[in] address The specified value for Address field, which is to be set to an IOTLB invalidate descriptor.
 *                    This argument is only needed for page-selective-within-domain invalidation, which is not
 *                    supported in current scope, keeping it here for scope extension in the future.
 * @param[in] am The specified value for Address Mask field, which is to be set to an IOTLB invalidate descriptor.
 *               This argument is only needed for page-selective-within-domain invalidation, which is not
 *               supported in current scope, keeping it here for scope extension in the future.
 * @param[in] hint The specified value for Invalidation Hint field, which is to be set to an IOTLB invalidate
 *                 descriptor. This argument is only needed for page-selective-within-domain invalidation, which
 *                 is not supported in current scope, keeping it here for scope extension in the future.
 * @param[in] iirg The specified value for IOTLB Invalidation Request Granularity field, which is to be set to an IOTLB
 *                 invalidate descriptor.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 * @pre (iirg == DMAR_IIRG_GLOBAL) || (iirg == DMAR_IIRG_DOMAIN)
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void dmar_invalid_iotlb(struct dmar_drhd_rt *dmar_unit, uint16_t did, __unused uint64_t address,
	__unused uint8_t am, __unused bool hint, enum dmar_iirg_type iirg)
{
	/* set Drain Reads & Drain Writes,
	 * if hardware doesn't support it, will be ignored by hardware
	 */
	/** Declare the following local variables of type 'struct dmar_entry'.
	 *  - invalidate_desc representing an invalidation descriptor, not initialized. */
	struct dmar_entry invalidate_desc;

	/** Set 'invalidate_desc.hi_64' to 0 */
	invalidate_desc.hi_64 = 0UL;

	/** Set following fields in 'invalidate_desc.lo_64' to the specified values, indicating that 'invalidate_desc'
	 *  is an IOTLB invalidate descriptor and hardware will perform the read drain and write drain
	 *  before the next Invalidation Wait Descriptor is completed:
	 *  - Drain Reads field (Bit 7) to 1
	 *  - Drain Writes field (Bit 6) to 1
	 *  - Type field (Bits 3:0) to 2
	 */
	invalidate_desc.lo_64 = DMA_IOTLB_DR | DMA_IOTLB_DW | DMAR_INV_IOTLB_DESC;

	/** Depending on IOTLB Invalidation Request Granularity specified by \a iirg */
	switch (iirg) {
	/** \a iirg is DMAR_IIRG_GLOBAL, indicating a global invalidation request */
	case DMAR_IIRG_GLOBAL:
		/** Set IOTLB Invalidation Request Granularity field (Bits 5:4) in 'invalidate_desc.lo_64' to 1 */
		invalidate_desc.lo_64 |= DMA_IOTLB_GLOBAL_INVL;
		/** End of case */
		break;
	/** \a iirg is DMAR_IIRG_DOMAIN, indicating a domain-selective invalidation request */
	case DMAR_IIRG_DOMAIN:
		/** Set following fields in 'invalidate_desc.lo_64' to the specified values:
		 *  - IOTLB Invalidation Request Granularity field (Bits 5:4) to 2
		 *  - Domain-ID field (Bits 31:16) to \a did
		 */
		invalidate_desc.lo_64 |= DMA_IOTLB_DOMAIN_INVL | dma_iotlb_did(did);
		/** End of case */
		break;
	/** Otherwise */
	default:
		/** Set 'invalidate_desc.lo_64' to 0 */
		invalidate_desc.lo_64 = 0UL;
		/** Logging a message with a log level of 3 to indicate that the default case is reached.
		 *  Pre-condition guarantees that this case would not happen.
		 *  Keeping it here for defensive programming. */
		pr_err("unknown IIRG type");
		/** End of case */
		break;
	}

	/** If 'invalidate_desc.lo_64' is not equal to 0 */
	if (invalidate_desc.lo_64 != 0UL) {
		/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that
		 *  is used to protect the operations on \a dmar_unit.
		 *  - &(dmar_unit->lock) */
		spinlock_obtain(&(dmar_unit->lock));

		/** Call dmar_issue_qi_request with the following parameters, in order to submit the
		 *  invalidation request specified by 'invalidate_desc' to the DRHD structure specified by \a dmar_unit
		 *  and wait until hardware completes it.
		 *  - dmar_unit
		 *  - invalidate_desc
		 */
		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		/** Call spinlock_release with the following parameters, in order to release the spin lock that
		 *  is used to protect the operations on \a dmar_unit.
		 *  - &(dmar_unit->lock) */
		spinlock_release(&(dmar_unit->lock));
	}
}

/**
 * @brief Submit a global IOTLB invalidation request to hardware and wait until hardware completes it.
 *
 * All IOTLB entries will be invalidated and all paging-structure-cache entries will be invalidated with this
 * invalidation request.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the IOTLB and paging structure caches globally.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_invalid_iotlb_global(struct dmar_drhd_rt *dmar_unit)
{
	/** Call dmar_invalid_iotlb with the following parameters, in order to
	 *  submit a global IOTLB invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - 0H
	 *  - 0H
	 *  - 0H
	 *  - false
	 *  - DMAR_IIRG_GLOBAL
	 */
	dmar_invalid_iotlb(dmar_unit, 0U, 0UL, 0U, false, DMAR_IIRG_GLOBAL);
}

/**
 * @brief Set up the interrupt remapping table for the specified DRHD structure.
 *
 * It is supposed to be called by 'dmar_prepare'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose interrupt remapping table is to be set up.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_set_intr_remap_table(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint64_t.
	 *  - address representing the value to be set to Interrupt Remapping Table Address Register,
	 *  not initialized. */
	uint64_t address;
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the content of Global Status Register, not initialized. */
	uint32_t status;
	/** Declare the following local variables of type uint8_t.
	 *  - size representing the value to be set to Size field in Interrupt Remapping Table Address Register,
	 *  not initialized. */
	uint8_t size;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));

	/** If 'dmar_unit->ir_table_addr' is equal to 0H, indicating that there is no interrupt remapping table
	 *  allocated to the DRHD structure specified by \a dmar_unit */
	if (dmar_unit->ir_table_addr == 0UL) {
		/** Set 'dmar_unit->ir_table_addr' to the return value of 'hva2hpa(get_ir_table(dmar_unit->index))',
		 *  which is the base address (host physical address) of the 4-KByte aligned interrupt remapping table
		 *  allocated to the DRHD structure specified by \a dmar_unit */
		dmar_unit->ir_table_addr = hva2hpa(get_ir_table(dmar_unit->index));
	}

	/** Set 'address' to 'dmar_unit->ir_table_addr | DMAR_IR_ENABLE_EIM'
	 *  (Extended Interrupt Mode Enable field in 'address' is set to 1 to indicate that x2APIC mode is active.) */
	address = dmar_unit->ir_table_addr | DMAR_IR_ENABLE_EIM;

	/* Set number of bits needed to represent the entries minus 1 */
	/** Set 'size' to the subtraction of 1 from the index of most significant bit set in CONFIG_MAX_IR_ENTRIES,
	 *  where CONFIG_MAX_IR_ENTRIES is a multiple of DMAR_NUM_IR_ENTRIES_PER_PAGE and it is smaller than or
	 *  equal to 2^16. */
	size = (uint8_t)fls32(CONFIG_MAX_IR_ENTRIES) - 1U;
	/** Set 'address' to 'address | size'
	 *  (Size field in 'address' is set to 'size' to indicate that this DRHD structure supports at most
	 *  CONFIG_MAX_IR_ENTRIES interrupt remapping entries.) */
	address = address | size;

	/** Call iommu_write64 with the following parameters, in order to write 'address' to
	 *  Interrupt Remapping Table Address Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_IRTA_REG
	 *  - address
	 */
	iommu_write64(dmar_unit, DMAR_IRTA_REG, address);

	/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd | DMA_GCMD_SIRTP' to
	 *  Global Command Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_GCMD_REG
	 *  - dmar_unit->gcmd | DMA_GCMD_SIRTP
	 */
	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SIRTP);

	/** Call dmar_wait_completion with the following parameters, in order to wait until
	 *  Interrupt Remapping Table Pointer Status Bit (Bit 24) of Global Status Register associated with
	 *  \a dmar_unit is updated to 1.
	 *  - dmar_unit
	 *  - DMAR_GSTS_REG
	 *  - DMA_GSTS_IRTPS
	 *  - false
	 *  - &status
	 */
	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRTPS, false, &status);

	/** Call spinlock_release with the following parameters, in order to release the spin lock that
	 *  is used to protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
}

/**
 * @brief Submit the specified interrupt entry cache invalidation request to hardware.
 *
 * Submit the specified interrupt entry cache invalidation request to hardware and wait until hardware completes it.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the interrupt entry cache.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 * @param[in] intr_index The specified value for Interrupt Index field, which is to be set to an interrupt entry
 *                       cache invalidate descriptor.
 * @param[in] index_mask The specified value for Index Mask field, which is to be set to an interrupt entry
 *                       cache invalidate descriptor.
 * @param[in] is_global A flag to indicate whether the specified invalidation request is global or not.
 *                      It is true if the specified invalidation request is global, otherwise, it is false.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void dmar_invalid_iec(struct dmar_drhd_rt *dmar_unit, uint16_t intr_index, uint8_t index_mask, bool is_global)
{
	/** Declare the following local variables of type 'struct dmar_entry'.
	 *  - invalidate_desc representing an invalidation descriptor, not initialized. */
	struct dmar_entry invalidate_desc;

	/** Set 'invalidate_desc.hi_64' to 0 */
	invalidate_desc.hi_64 = 0UL;
	/** Set 'invalidate_desc.lo_64' to DMAR_INV_IEC_DESC, indicating that 'invalidate_desc' is
	 *  an interrupt entry cache invalidation descriptor. */
	invalidate_desc.lo_64 = DMAR_INV_IEC_DESC;

	/** If 'is_global' is true, indicating that the specified invalidation request is global */
	if (is_global) {
		/** Set Granularity field (Bit 4) in 'invalidate_desc.lo_64' to 0 */
		invalidate_desc.lo_64 |= DMAR_IEC_GLOBAL_INVL;
	} else {
		/** Set following fields in 'invalidate_desc.lo_64' to the specified values:
		 *  - Granularity field (Bit 4) in 'invalidate_desc.lo_64' to 1
		 *  - Interrupt Index field (Bits 47:32) to \a intr_index
		 *  - Interrupt Mask field (Bits 31:27) to bits [4:0] of \a index_mask
		 */
		invalidate_desc.lo_64 |= DMAR_IECI_INDEXED | dma_iec_index(intr_index, index_mask);
	}

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));

	/** Call dmar_issue_qi_request with the following parameters, in order to submit the
	 *  invalidation request specified by 'invalidate_desc' to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - invalidate_desc
	 */
	dmar_issue_qi_request(dmar_unit, invalidate_desc);

	/** Call spinlock_release with the following parameters, in order to release the spin lock that
	 *  is used to protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
}

/**
 * @brief Submit a global interrupt entry cache invalidation request to hardware.
 *
 * Submit a global interrupt entry cache invalidation request to hardware and wait until hardware completes it.
 * All interrupt remapping entries cached at the remapping hardware will be invalidated with this invalidation request.
 *
 * It is supposed to be called when hypervisor attempts to invalidate the interrupt entry cache globally.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure where the invalidation request is submitted to.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_invalid_iec_global(struct dmar_drhd_rt *dmar_unit)
{
	/** Call dmar_invalid_iec with the following parameters, in order to
	 *  submit a global interrupt entry cache invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - 0H
	 *  - 0H
	 *  - true
	 */
	dmar_invalid_iec(dmar_unit, 0U, 0U, true);
}

/**
 * @brief Set up the root table for the specified DRHD structure.
 *
 * It is supposed to be called by 'dmar_prepare'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose root table is to be set up.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_set_root_table(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint64_t.
	 *  - address representing the value to be set to Root Table Address Register,
	 *  not initialized. */
	uint64_t address;
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the content of Global Status Register, not initialized. */
	uint32_t status;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));

	/** If 'dmar_unit->root_table_addr' is equal to 0H, indicating that there is no root table
	 *  allocated to the DRHD structure specified by \a dmar_unit */
	if (dmar_unit->root_table_addr == 0UL) {
		/** Set 'dmar_unit->root_table_addr' to the return value of 'hva2hpa(get_root_table(dmar_unit->index))',
		 *  which is the base address (host physical address) of the 4-KByte aligned root table
		 *  allocated to the DRHD structure specified by \a dmar_unit */
		dmar_unit->root_table_addr = hva2hpa(get_root_table(dmar_unit->index));
	}

	/** Set 'address' to dmar_unit->root_table_addr */
	address = dmar_unit->root_table_addr;

	/** Call iommu_write64 with the following parameters, in order to write 'address' to
	 *  Root Table Address Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_RTADDR_REG
	 *  - address
	 */
	iommu_write64(dmar_unit, DMAR_RTADDR_REG, address);

	/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd | DMA_GCMD_SRTP' to
	 *  Global Command Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_GCMD_REG
	 *  - dmar_unit->gcmd | DMA_GCMD_SRTP
	 */
	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	/** Call dmar_wait_completion with the following parameters, in order to wait until
	 *  Root Table Pointer Status Bit (Bit 30) of Global Status Register associated with
	 *  \a dmar_unit is updated to 1.
	 *  - dmar_unit
	 *  - DMAR_GSTS_REG
	 *  - DMA_GSTS_RTPS
	 *  - false
	 *  - &status
	 */
	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_RTPS, false, &status);
	/** Call spinlock_release with the following parameters, in order to release the spin lock that
	 *  is used to protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
}

/**
 * @brief Enable queued invalidation for the specified DRHD structure.
 *
 * It is supposed to be called by 'dmar_prepare'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose queued invalidation is to be enabled.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_enable_qi(struct dmar_drhd_rt *dmar_unit)
{
	/** Declare the following local variables of type uint32_t.
	 *  - status representing the content of Global Status Register, initialized as 0. */
	uint32_t status = 0U;

	/** Call spinlock_obtain with the following parameters, in order to obtain the spin lock that is used to
	 *  protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_obtain(&(dmar_unit->lock));

	/** Set 'dmar_unit->qi_queue' to the return value of 'hva2hpa(get_qi_queue(dmar_unit->index))',
	 *  which is the base address (host physical address) of the 4-KByte aligned invalidation queue
	 *  allocated to the DRHD structure specified by \a dmar_unit */
	dmar_unit->qi_queue = hva2hpa(get_qi_queue(dmar_unit->index));
	/** Call iommu_write64 with the following parameters, in order to write 'dmar_unit->qi_queue' to
	 *  Invalidation Queue Address Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_IQA_REG
	 *  - dmar_unit->qi_queue
	 */
	iommu_write64(dmar_unit, DMAR_IQA_REG, dmar_unit->qi_queue);

	/** Call iommu_write32 with the following parameters, in order to write 0 to
	 *  Invalidation Queue Tail Register associated with \a dmar_unit.
	 *  - dmar_unit
	 *  - DMAR_IQT_REG
	 *  - 0
	 */
	iommu_write32(dmar_unit, DMAR_IQT_REG, 0U);

	/** If Queued Invalidation Enable Bit (Bit 26) of 'dmar_unit->gcmd' is equal to 0, indicating that
	 *  queued invalidation is disabled on the DRHD structure specified by \a dmar_unit */
	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == 0U) {
		/** Set Queued Invalidation Enable Bit (Bit 26) of 'dmar_unit->gcmd' to 1 */
		dmar_unit->gcmd |= DMA_GCMD_QIE;
		/** Call iommu_write32 with the following parameters, in order to write 'dmar_unit->gcmd' to
		 *  Global Command Register associated with \a dmar_unit.
		 *  - dmar_unit
		 *  - DMAR_GCMD_REG
		 *  - dmar_unit->gcmd
		 */
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/** Call dmar_wait_completion with the following parameters, in order to wait until
		 *  Queued Invalidation Enable Status Bit (Bit 26) of Global Status Register associated with
		 *  \a dmar_unit is updated to 1.
		 *  - dmar_unit
		 *  - DMAR_GSTS_REG
		 *  - DMA_GSTS_QIES
		 *  - false
		 *  - &status
		 */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, false, &status);
	}

	/** Call spinlock_release with the following parameters, in order to release the spin lock that
	 *  is used to protect the operations on \a dmar_unit.
	 *  - &(dmar_unit->lock) */
	spinlock_release(&(dmar_unit->lock));
}

/**
 * @brief Prepare the resources needed for the specified DRHD structure.
 *
 * Prepare the resources needed for the specified DRHD structure, including the root table set-up,
 * the interrupt remapping table set-up, and queued invalidation enabling.
 *
 * It is supposed to be used as a callback in 'init_iommu'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose resources is to be prepared.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_prepare(struct dmar_drhd_rt *dmar_unit)
{
	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - dmar_unit->drhd->reg_base_addr
	 */
	dev_dbg(ACRN_DBG_IOMMU, "prepare dmar unit [0x%x]", dmar_unit->drhd->reg_base_addr);
	/** Call dmar_set_root_table with the following parameters, in order to
	 *  set up the root table for the DRHD structure specified by \a dmar_unit.
	 *  - dmar_unit */
	dmar_set_root_table(dmar_unit);
	/** Call dmar_enable_qi with the following parameters, in order to
	 *  enable queued invalidation for the DRHD structure specified by \a dmar_unit.
	 *  - dmar_unit */
	dmar_enable_qi(dmar_unit);
	/** Call dmar_set_intr_remap_table with the following parameters, in order to
	 *  set up the interrupt remapping table for the DRHD structure specified by \a dmar_unit.
	 *  - dmar_unit */
	dmar_set_intr_remap_table(dmar_unit);
}

/**
 * @brief Invalidate cached entries globally and enable DMA remapping on the specified DRHD structure.
 *
 * Following entries will be invalidated within this function:
 * - All context-cache entries cached at the remapping hardware;
 * - All IOTLB entries and all paging-structure-cache entries cached at the remapping hardware;
 * - All interrupt remapping entries cached at the remapping hardware.
 *
 * It is supposed to be used as a callback in 'enable_iommu'.
 *
 * @param[inout] dmar_unit A pointer to a data structure that stores the runtime information for the specified
 *                         DRHD structure whose DMA remapping is to be enabled.
 *
 * @return None
 *
 * @pre dmar_unit != NULL
 * @pre dmar_unit->drhd != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void dmar_enable(struct dmar_drhd_rt *dmar_unit)
{
	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - dmar_unit->drhd->reg_base_addr
	 */
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar unit [0x%x]", dmar_unit->drhd->reg_base_addr);
	/** Call dmar_invalid_context_cache_global with the following parameters, in order to
	 *  submit a global context cache invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 */
	dmar_invalid_context_cache_global(dmar_unit);
	/** Call dmar_invalid_iotlb_global with the following parameters, in order to
	 *  submit a global IOTLB invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 */
	dmar_invalid_iotlb_global(dmar_unit);
	/** Call dmar_invalid_iotlb_global with the following parameters, in order to
	 *  submit a global interrupt entry cache invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 */
	dmar_invalid_iec_global(dmar_unit);
	/** Call dmar_enable_translation with the following parameters, in order to
	 *  enable DMA remapping on the DRHD structure specified by \a dmar_unit (if it's not enabled yet).
	 *  - dmar_unit
	 */
	dmar_enable_translation(dmar_unit);
}

/**
 * @brief Assign the specified PCI device to the specified IOMMU domain.
 *
 * It is supposed to be called only by 'assign_vdev_pt_iommu_domain' from 'vp-dm.vperipheral' module.
 *
 * @param[in] domain The pointer which points to an IOMMU domain where the PCI device is assigned to.
 * @param[in] bus The bus number of the specified PCI device.
 * @param[in] devfun The 8-bit device(5-bit):function(3-bit) of the specified PCI device.
 *
 * @return A status code indicating whether the requested device assignment is succeeded or not.
 *
 * @retval 0 The requested device assignment is succeeded.
 *           Return value is always 0 in current scope, keeping it here for scope extension in the future.
 *
 * @pre domain != NULL
 * @pre domain->addr_width == 48
 * @pre bus < CONFIG_IOMMU_BUS_NUM
 * @pre !((bus == 0H) && (devfun == 10H))
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark The physical platform shall guarantee that the data read from Capability Register of DMAR unit
 *         for all devices except GPU is d2008c40660462H.
 * @remark The physical platform shall guarantee that the data read from Extended Capability Register of DMAR unit
 *         for all devices except GPU is f050daH.
 * @remark The 'vp-dm.vperipheral' module shall guarantee that add_iommu_device would be called at most once
 *         for each PCI device specified by \a bus and \a devfun.
 * @remark This API shall be called after init_iommu has been called once on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety When the combination of \a bus and \a devfun is different among parallel invocation.
 */
int32_t add_iommu_device(struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - dmar_unit representing a pointer to the data structure that stores the runtime information for the
	 *  DRHD structure which covers the specified PCI device, not initialized. */
	struct dmar_drhd_rt *dmar_unit;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - root_table representing a pointer to the root table associated with the specified DRHD structure,
	 *  not initialized. */
	struct dmar_entry *root_table;
	/** Declare the following local variables of type uint64_t.
	 *  - context_table_addr representing the base address of the context table associated with the specified
	 *  DRHD structure and the specified PCI bus, not initialized. */
	uint64_t context_table_addr;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - context representing a pointer to the context table associated with the specified DRHD structure
	 *  and the specified PCI bus, not initialized. */
	struct dmar_entry *context;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - root_entry representing a pointer to the root entry associated with the specified DRHD structure
	 *  and the specified PCI bus, not initialized. */
	struct dmar_entry *root_entry;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - context_entry representing a pointer to the context entry associated with the specified
	 *  DRHD structure and the specified PCI device, not initialized. */
	struct dmar_entry *context_entry;
	/** Declare the following local variables of type uint64_t.
	 *  - hi_64 representing the high 64-bits value of a 128-bit entry, not initialized. */
	uint64_t hi_64;
	/** Declare the following local variables of type uint64_t.
	 *  - lo_64 representing the low 64-bits value of a 128-bit entry, initialized as 0. */
	uint64_t lo_64 = 0UL;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing a status code indicating whether the requested device assignment is succeeded or not,
	 *  initialized as 0. */
	int32_t ret = 0;

	/** Set 'dmar_unit' to '&dmar_drhd_units[1]', which points to the data structure that stores the
	 *  runtime information for the DRHD structure that covers the PCI device specified by \a bus and \a devfun.
	 */
	dmar_unit = &dmar_drhd_units[1];

	/** Set 'root_table' to the return value of 'hpa2hva(dmar_unit->root_table_addr)', which points to
	 *  the root table associated with 'dmar_unit' */
	root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);

	/** Set 'root_entry' to 'root_table + bus', which points to the root entry associated with \a bus */
	root_entry = root_table + bus;

	/** If Present field (Bit 0) in 'root_entry->lo_64' is equal to 0, indicating that the root entry specified by
	 *  'root_entry' is not present and all the other fields of this root entry are ignored by hardware. */
	if (dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS) == 0UL) {
		/* create context table for the bus if not present */
		/** Set 'context_table_addr' to the return value of 'hva2hpa(get_ctx_table(dmar_unit->index, bus))',
		 *  which indicates the base address (host physical address) of the context table associated with
		 *  'dmar_unit' and \a bus */
		context_table_addr = hva2hpa(get_ctx_table(dmar_unit->index, bus));

		/** Shift 'context_table_addr' right for PAGE_SHIFT bits */
		context_table_addr = context_table_addr >> PAGE_SHIFT;

		/** Set CTP (Context-table Pointer) field (Bits 63:12) in 'lo_64' to 'context_table_addr' */
		lo_64 = dmar_set_bitslice(
			lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS, context_table_addr);
		/** Set Present field (Bit 0) in 'lo_64' to 1 */
		lo_64 = dmar_set_bitslice(lo_64, ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS, 1UL);

		/** Set 'root_entry->hi_64' to 0 */
		root_entry->hi_64 = 0UL;
		/** Set 'root_entry->lo_64' to 'lo_64' */
		root_entry->lo_64 = lo_64;
		/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
		 *  the root entry (128-bit in size) pointed by 'root_entry'.
		 *  - root_entry
		 *  - sizeof(struct dmar_entry)
		 */
		iommu_flush_cache(root_entry, sizeof(struct dmar_entry));
	} else {
		/** Set 'context_table_addr' to CTP (Context-table Pointer) field (Bits 63:12) in 'root_entry->lo_64' */
		context_table_addr =
			dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
	}

	/** Shift 'context_table_addr' left for PAGE_SHIFT bits */
	context_table_addr = context_table_addr << PAGE_SHIFT;

	/** Set 'context' to the return value of 'hpa2hva(context_table_addr)', which points to
	 *  the context table associated with 'dmar_unit' and \a bus */
	context = (struct dmar_entry *)hpa2hva(context_table_addr);
	/** Set 'context_entry' to 'context + devfun', which points to the context entry associated with \a devfun */
	context_entry = context + devfun;
	/* setup context entry for the devfun */
	/** Set 'hi_64' to 0 */
	hi_64 = 0UL;
	/** Set 'lo_64' to 0 */
	lo_64 = 0UL;

	/** Set AW (Address Width) field (Bits 2:0) in 'hi_64' to the return value of
	 *  'width_to_aw(domain->addr_width)' */
	hi_64 = dmar_set_bitslice(
		hi_64, CTX_ENTRY_UPPER_AW_MASK, CTX_ENTRY_UPPER_AW_POS, (uint64_t)width_to_aw(domain->addr_width));
	/** Set TT (Translation Type) field (Bits 3:2) in 'lo_64' to DMAR_CTX_TT_UNTRANSLATED */
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_TT_MASK, CTX_ENTRY_LOWER_TT_POS, DMAR_CTX_TT_UNTRANSLATED);

	/** Set DID (Domain Identifier) field (Bits 23:8) in 'hi_64' to the return value of
	 *  'vmid_to_domainid(domain->vm_id)' */
	hi_64 = dmar_set_bitslice(hi_64, CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS,
		(uint64_t)vmid_to_domainid(domain->vm_id));
	/** Set SLPTPTR (Second Level Page Translation Pointer) field (Bits 63:12) in 'lo_64' to
	 *  'domain->trans_table_ptr >> PAGE_SHIFT' */
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_SLPTPTR_MASK, CTX_ENTRY_LOWER_SLPTPTR_POS,
		domain->trans_table_ptr >> PAGE_SHIFT);
	/** Set Present field (Bit 0) in 'lo_64' to 1 */
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS, 1UL);

	/** Set 'context_entry->hi_64' to 'hi_64' */
	context_entry->hi_64 = hi_64;
	/** Set 'context_entry->lo_64' to 'lo_64' */
	context_entry->lo_64 = lo_64;
	/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
	 *  the context entry (128-bit in size) pointed by 'context_entry'.
	 *  - context_entry
	 *  - sizeof(struct dmar_entry)
	 */
	iommu_flush_cache(context_entry, sizeof(struct dmar_entry));

	/** Return 'ret' */
	return ret;
}

/**
 * @brief Remove the specified PCI device from the specified IOMMU domain.
 *
 * It is supposed to be called only by 'remove_vdev_pt_iommu_domain' from 'vp-dm.vperipheral' module.
 *
 * @param[in] domain The pointer which points to an IOMMU domain where the PCI device is removed from.
 * @param[in] bus The bus number of the specified PCI device.
 * @param[in] devfun The 8-bit device(5-bit):function(3-bit) of the specified PCI device.
 *
 * @return A status code indicating whether the requested device removal is succeeded or not.
 *
 * @retval 0 The requested device removal is succeeded.
 *           Return value is always 0 in current scope, keeping it here for scope extension in the future.
 *
 * @pre domain != NULL
 * @pre bus < CONFIG_IOMMU_BUS_NUM
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark The physical platform shall guarantee that the data read from Capability Register of DMAR unit
 *         for all devices except GPU is d2008c40660462H.
 * @remark The physical platform shall guarantee that the data read from Extended Capability Register of DMAR unit
 *         for all devices except GPU is f050daH.
 * @remark This API shall be called after add_iommu_device has been called only once with
 *         \a domain, \a bus, \a devfun as arguments on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
int32_t remove_iommu_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - dmar_unit representing a pointer to the data structure that stores the runtime information for the
	 *  DRHD structure which covers the specified PCI device, not initialized. */
	struct dmar_drhd_rt *dmar_unit;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - root_table representing a pointer to the root table associated with the specified DRHD structure,
	 *  not initialized. */
	struct dmar_entry *root_table;
	/** Declare the following local variables of type uint64_t.
	 *  - context_table_addr representing the base address of the context table associated with the specified
	 *  DRHD structure and the specified PCI bus, not initialized. */
	uint64_t context_table_addr;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - context representing a pointer to the context table associated with the specified DRHD structure
	 *  and the specified PCI bus, not initialized. */
	struct dmar_entry *context;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - root_entry representing a pointer to the root entry associated with the specified DRHD structure
	 *  and the specified PCI bus, not initialized. */
	struct dmar_entry *root_entry;
	/** Declare the following local variables of type 'struct dmar_entry *'.
	 *  - context_entry representing a pointer to the context entry associated with the specified
	 *  DRHD structure and the specified PCI device, not initialized. */
	struct dmar_entry *context_entry;
	/* source id */
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - sid representing the source ID to be used for context cache invalidation, not initialized. */
	union pci_bdf sid;
	/** Declare the following local variables of type int32_t.
	 *  - ret representing a status code indicating whether the requested device removal is succeeded or not,
	 *  initialized as 0. */
	int32_t ret = 0;

	/** Set 'dmar_unit' to '&dmar_drhd_units[1]', which points to the data structure that stores the
	 *  runtime information for the DRHD structure that covers the PCI device specified by \a bus and \a devfun.
	 */
	dmar_unit = &dmar_drhd_units[1];

	/** Set 'sid.fields.bus' to \a bus */
	sid.fields.bus = bus;
	/** Set 'sid.fields.devfun' to \a devfun */
	sid.fields.devfun = devfun;

	/** Set 'root_table' to the return value of 'hpa2hva(dmar_unit->root_table_addr)', which points to
	 *  the root table associated with 'dmar_unit' */
	root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);
	/** Set 'root_entry' to 'root_table + bus', which points to the root entry associated with \a bus */
	root_entry = root_table + bus;

	/** Set 'context_table_addr' to CTP (Context-table Pointer) field (Bits 63:12) in 'root_entry->lo_64' */
	context_table_addr = dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
	/** Shift 'context_table_addr' left for PAGE_SHIFT bits */
	context_table_addr = context_table_addr << PAGE_SHIFT;
	/** Set 'context' to the return value of 'hpa2hva(context_table_addr)', which points to
	 *  the context table associated with 'dmar_unit' and \a bus */
	context = (struct dmar_entry *)hpa2hva(context_table_addr);

	/** Set 'context_entry' to 'context + devfun', which points to the context entry associated with \a devfun */
	context_entry = context + devfun;
	/* clear the present bit first */
	/** Set 'context_entry->lo_64' to 0 */
	context_entry->lo_64 = 0UL;
	/** Set 'context_entry->hi_64' to 0 */
	context_entry->hi_64 = 0UL;

	/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
	 *  the context entry (128-bit in size) pointed by 'context_entry'.
	 *  - context_entry
	 *  - sizeof(struct dmar_entry)
	 */
	iommu_flush_cache(context_entry, sizeof(struct dmar_entry));

	/** Call dmar_invalid_context_cache with the following parameters, in order to submit a device-selective
	 *  context cache invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - vmid_to_domainid(domain->vm_id)
	 *  - sid.value
	 *  - 0H
	 *  - DMAR_CIRG_DEVICE
	 */
	dmar_invalid_context_cache(dmar_unit, vmid_to_domainid(domain->vm_id), sid.value, 0U, DMAR_CIRG_DEVICE);
	/** Call dmar_invalid_iotlb with the following parameters, in order to submit a domain-selective
	 *  IOTLB invalidation request to the DRHD structure specified by \a dmar_unit
	 *  and wait until hardware completes it.
	 *  - dmar_unit
	 *  - vmid_to_domainid(domain->vm_id)
	 *  - 0H
	 *  - 0H
	 *  - false
	 *  - DMAR_IIRG_DOMAIN
	 */
	dmar_invalid_iotlb(dmar_unit, vmid_to_domainid(domain->vm_id), 0UL, 0U, false, DMAR_IIRG_DOMAIN);

	/** Return 'ret' */
	return ret;
}

/**
 * @brief Perform the specified action on all DRHD structures that are not ignored by hypervisor.
 *
 * It is supposed to be called by 'init_iommu' and 'enable_iommu'.
 *
 * @param[in] action A callback function specifies the action to be performed on all DRHD structures that are not
 *                   ignored by hypervisor.
 *                   This callback function is a void function and has one argument with type 'struct dmar_drhd_rt *'.
 *                   The argument is a pointer to a data structure that stores the runtime information for the
 *                   specified DRHD structure where the action is to be performed.
 *
 * @return None
 *
 * @pre action != NULL
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void do_action_for_iommus(void (*action)(struct dmar_drhd_rt *))
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - dmar_unit representing a pointer to the data structure that stores the runtime information
	 *  for the DRHD structure where the specified action is performed on, not initialized. */
	struct dmar_drhd_rt *dmar_unit;
	/** Declare the following local variables of type uint32_t.
	 *  - i representing the loop counter as DRHD index, not initialized. */
	uint32_t i;

	/** For each 'i' ranging from 0 to 'platform_dmar_info->drhd_count - 1' [with a step of 1] */
	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		/** Set 'dmar_unit' to '&dmar_drhd_units[i]', which points to the (i+1)-th DRHD structure stored
		 *  in the array 'dmar_drhd_units' */
		dmar_unit = &dmar_drhd_units[i];
		/** If 'dmar_unit->drhd->ignore' is false, indicating that the DRHD structure specified by 'dmar_unit'
		 *  is not ignored by hypervisor */
		if (!dmar_unit->drhd->ignore) {
			/** Call action with the following parameters, in order to
			 *  perform the action specified by \a action on the DRHD structure specified by 'dmar_unit'.
			 *  - dmar_unit
			 */
			action(dmar_unit);
		} else {
			/** Logging the following information with a log level of ACRN_DBG_IOMMU.
			 *  - dmar_unit->drhd->reg_base_addr
			 */
			dev_dbg(ACRN_DBG_IOMMU, "ignore dmar_unit @0x%x", dmar_unit->drhd->reg_base_addr);
		}
	}
}

/**
 * @brief Create an IOMMU domain based on the given information.
 *
 * It is supposed to be called only by 'vpci_init' from 'vp-dm.vperipheral' module.
 *
 * @param[in] vm_id The given VM identifier.
 * @param[in] translation_table The given host physical base address of the translation table.
 * @param[in] addr_width The given address width (in bit) of the IOMMU domain.
 *
 * @return A pointer to an IOMMU domain which contains the given information.
 *
 * @pre translation_table != 0H
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a vm_id is different among parallel invocation.
 */
struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width)
{
	/** Declare the following static array of type 'struct iommu_domain'.
	 *  - iommu_domains representing an array that contains all the IOMMU domains
	 *  (the number of elements in this array is MAX_DOMAIN_NUM), initialized as all 0s.
	 */
	static struct iommu_domain iommu_domains[MAX_DOMAIN_NUM];
	/** Declare the following local variables of type 'struct iommu_domain *'.
	 *  - domain representing a pointer to an IOMMU domain to be created, not initialized. */
	struct iommu_domain *domain;

	/** Set 'domain' to '&iommu_domains[vmid_to_domainid(vm_id)]', which points to the X-th IOMMU domain stored
	 *  in the array 'iommu_domains', where X is the domain ID associated with the VM specified by \a vm_id */
	domain = &iommu_domains[vmid_to_domainid(vm_id)];

	/** Set 'domain->vm_id' to \a vm_id */
	domain->vm_id = vm_id;
	/** Set 'domain->trans_table_ptr' to \a translation_table */
	domain->trans_table_ptr = translation_table;
	/** Set 'domain->addr_width' to \a addr_width */
	domain->addr_width = addr_width;

	/** Logging the following information with a log level of ACRN_DBG_IOMMU.
	 *  - vmid_to_domainid(domain->vm_id)
	 *  - domain->vm_id
	 *  - domain->trans_table_ptr
	 */
	dev_dbg(ACRN_DBG_IOMMU, "create domain [%d]: vm_id = %hu, ept@0x%x", vmid_to_domainid(domain->vm_id),
		domain->vm_id, domain->trans_table_ptr);

	/** Return 'domain' */
	return domain;
}

/**
 * @brief Destroy the specified IOMMU domain via resetting the contents inside to all 0s.
 *
 * It is supposed to be called only by 'shutdown_vm' from 'vp-base.vm' module.
 *
 * @param[inout] domain A pointer to an IOMMU domain whose data inside is to be cleared.
 *
 * @return None
 *
 * @pre domain != NULL
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark This API shall be called after create_iommu_domain has been called once with "domain->vm_id" as
 *         first argument on any processor.
 * @remark This API shall be called after vpci_cleanup has been called once with "get_vm_from_vmid(domain->vm_id)"
 *         as argument on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void destroy_iommu_domain(struct iommu_domain *domain)
{
	/** Call memset with the following parameters, in order to reset the contents stored in \a domain to all 0s,
	 *  and discard its return value.
	 *  - domain
	 *  - 0
	 *  - sizeof(*domain)
	 */
	(void)memset(domain, 0U, sizeof(*domain));
}

/**
 * @brief Enable IOMMU.
 *
 * For all DRHD structures that are not ignored by hypervisor, this interface enables IOMMU, including
 * invalidating the context cache globally, invalidating the IOTLB and paging structure caches globally,
 * invalidating the interrupt entry cache globally, and enabling DMA remapping.
 *
 * It is supposed to be called only by 'init_pcpu_post' from 'hwmgmt.cpu' module.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark This API shall be called after init_iommu has been called once on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void enable_iommu(void)
{
	/** Call do_action_for_iommus with the following parameters, in order to
	 *  invalidate cached entries globally and then enable DMA remapping for all DRHD structures that are not
	 *  ignored by hypervisor.
	 *  - dmar_enable
	 */
	do_action_for_iommus(dmar_enable);
}

/**
 * @brief Get the physical information of the remapping hardware.
 *
 * It is supposed to be called only by 'init_iommu'.
 *
 * @return A pointer to the data structure which contains the physical information of the remapping hardware.
 *
 * @pre N/A
 *
 * @post return != NULL
 * @post return->drhd_count == DRHD_COUNT
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static struct dmar_info *get_dmar_info(void)
{
	/** Return '&plat_dmar_info', which points to the data structure containing the physical information of the
	 *  remapping hardware. */
	return &plat_dmar_info;
}

/**
 * @brief Initialize the remapping hardware.
 *
 * For all DRHD structures present in the platform, this function clears user/supervisor (U/S) flags for each entry
 * inside its memory region and disables DMA remapping, calling this process as registration in hypervisor.
 *
 * And for the DRHD structures that are not ignored by hypervisor, this function also sets up root table and interrupt
 * remapping table, and enables queued invalidation, calling this process as resource preparation.
 *
 * It is supposed to be called only by 'init_pcpu_post' from 'hwmgmt.cpu' module.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark The physical platform shall guarantee that the data read from Capability Register of DMAR unit
 *         for all devices except GPU is d2008c40660462H.
 * @remark The physical platform shall guarantee that the data read from Extended Capability Register of DMAR unit
 *         for all devices except GPU is f050daH.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_iommu(void)
{
	/** Set 'platform_dmar_info' to the return value of 'get_dmar_info()', which points to the data structure
	 *  containing the physical information of the remapping hardware. */
	platform_dmar_info = get_dmar_info();

	/** Call register_hrhd_units without any parameters, in order to register all DRHD structures in hypervisor. */
	register_hrhd_units();

	/** Call do_action_for_iommus with the following parameters, in order to
	 *  prepare the resources needed for all DRHD structures that are not ignored by hypervisor.
	 *  - dmar_prepare
	 */
	do_action_for_iommus(dmar_prepare);
}

/**
 * @brief Assign an interrupt remapping entry to the given interrupt request source.
 *
 * It is supposed to be called only by 'ptirq_build_physical_msi' from 'vp-dm.ptirq' module.
 *
 * @param[in] intr_src The specified interrupt request source.
 * @param[in] irte The predefined interrupt remapping entry.
 * @param[in] index The index of the interrupt remapping entry in the interrupt remapping table.
 *
 * @return None
 *
 * @pre index < CONFIG_MAX_IR_ENTRIES
 * @pre intr_src.src.msi.value != 10H
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety When \a intr_src is different among parallel invocation and \a index is different among parallel
 *               invocation.
 */
void dmar_assign_irte(struct intr_source intr_src, union dmar_ir_entry irte, uint16_t index)
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - dmar_unit representing a pointer to the data structure that stores the runtime information for the
	 *  DRHD structure which covers the specified interrupt source, not initialized. */
	struct dmar_drhd_rt *dmar_unit;
	/** Declare the following local variables of type 'union dmar_ir_entry *'.
	 *  - ir_table representing a pointer to the interrupt remapping table associated with the specified DRHD
	 *  structure, not initialized.
	 *  - ir_entry representing a pointer to the interrupt remapping entry associated with the specified DRHD
	 *  structure and the specified interrupt remapping index, not initialized.
	 */
	union dmar_ir_entry *ir_table, *ir_entry;
	/** Declare the following local variables of type 'union dmar_ir_entry'.
	 *  - effective_irte representing the setting of the interrupt remapping entry that eventually takes effect,
	 *  initialized as \a irte. */
	union dmar_ir_entry effective_irte = irte;
	/** Declare the following local variables of type 'union pci_bdf'.
	 *  - sid representing the source identifier of the interrupt, not initialized. */
	union pci_bdf sid;
	/** Declare the following local variables of type uint64_t.
	 *  - trigger_mode representing the trigger mode of the interrupt, not initialized. */
	uint64_t trigger_mode;

	/** Set 'dmar_unit' to '&dmar_drhd_units[1]', which points to the data structure that stores the
	 *  runtime information for the DRHD structure covering the interrupt source specified by \a intr_src.
	 */
	dmar_unit = &dmar_drhd_units[1];

	/** Set 'sid.value' to 'intr_src.src.msi.value' */
	sid.value = intr_src.src.msi.value;
	/** Set 'trigger_mode' to 0, indicating that the signal type of the interrupt is edge sensitive. */
	trigger_mode = 0x0UL;

	/** Call dmar_enable_intr_remapping with the following parameters, in order to
	 *  enable interrupt remapping on the DRHD structure specified by \a dmar_unit (if it's not enabled yet).
	 *  - dmar_unit
	 */
	dmar_enable_intr_remapping(dmar_unit);

	/** Set 'effective_irte.bits.svt' to 1, indicating that the requester-id in interrupt request is verified by
	 *  hardware via using SID and SQ fields in this entry. */
	effective_irte.bits.svt = 0x1UL;
	/** Set 'effective_irte.bits.sq' to 0, indicating that the interrupt request is verified by hardware
	 *  via comparing all 16-bits of SID field with the 16-bit requester-id of the interrupt request. */
	effective_irte.bits.sq = 0x0UL;
	/** Set 'effective_irte.bits.sid' to 'sid.value', indicating the source identifier of the interrupt. */
	effective_irte.bits.sid = sid.value;
	/** Set 'effective_irte.bits.present' to 1, indicating that hardware will process interrupt requests
	 *  referencing this entry. */
	effective_irte.bits.present = 0x1UL;
	/** Set 'effective_irte.bits.mode' to 0, indicating that interrupt requests processed through this entry are
	 *  remapped. */
	effective_irte.bits.mode = 0x0UL;
	/** Set 'effective_irte.bits.trigger_mode' to 'trigger_mode' */
	effective_irte.bits.trigger_mode = trigger_mode;
	/** Set 'effective_irte.bits.fpd' to 0, indicating that fault recording/reporting is enabled for interrupt
	 *  requests processed through this entry */
	effective_irte.bits.fpd = 0x0UL;

	/** Set 'ir_table' to the return value of 'hpa2hva(dmar_unit->ir_table_addr)', which points to
	 *  the interrupt remapping table associated with 'dmar_unit' */
	ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
	/** Set 'ir_entry' to 'ir_table + index', which points to the interrupt remapping entry associated with
	 *  \a index */
	ir_entry = ir_table + index;
	/** Set 'ir_entry->entry.hi_64' to 'effective_irte.entry.hi_64' */
	ir_entry->entry.hi_64 = effective_irte.entry.hi_64;
	/** Set 'ir_entry->entry.lo_64' to 'effective_irte.entry.lo_64' */
	ir_entry->entry.lo_64 = effective_irte.entry.lo_64;

	/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
	 *  the interrupt remapping entry (128-bit in size) pointed by 'ir_entry'.
	 *  - ir_entry
	 *  - sizeof(struct dmar_ir_entry)
	 */
	iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
	/** Call dmar_invalid_iec with the following parameters, in order to
	 *  submit an index-selective interrupt entry cache invalidation request to the DRHD structure specified
	 *  by \a dmar_unit.
	 *  - dmar_unit
	 *  - index
	 *  - 0H
	 *  - false
	 */
	dmar_invalid_iec(dmar_unit, index, 0U, false);
}

/**
 * @brief Free the interrupt remapping entry associated with the given interrupt index.
 *
 * It is supposed to be called only by 'remove_msix_remapping' from 'vp-dm.ptirq' module.
 *
 * @param[in] intr_src The specified interrupt request source. This argument is not used in current scope,
 *                     keeping it here for scope extension in the future.
 * @param[in] index The index of the interrupt remapping entry in the interrupt remapping table.
 *
 * @return None
 *
 * @pre index < CONFIG_MAX_IR_ENTRIES
 * @pre intr_src.src.msi.value != 10H
 *
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @remark N/A
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void dmar_free_irte(__unused struct intr_source intr_src, uint16_t index)
{
	/** Declare the following local variables of type 'struct dmar_drhd_rt *'.
	 *  - dmar_unit representing a pointer to the data structure that stores the runtime information for the
	 *  DRHD structure which covers the specified interrupt source, not initialized. */
	struct dmar_drhd_rt *dmar_unit;
	/** Declare the following local variables of type 'union dmar_ir_entry *'.
	 *  - ir_table representing a pointer to the interrupt remapping table associated with the specified DRHD
	 *  structure, not initialized.
	 *  - ir_entry representing a pointer to the interrupt remapping entry associated with the specified DRHD
	 *  structure and the specified interrupt remapping index, not initialized.
	 */
	union dmar_ir_entry *ir_table, *ir_entry;

	/** Set 'dmar_unit' to '&dmar_drhd_units[1]', which points to the data structure that stores the
	 *  runtime information for the DRHD structure covering the interrupt source specified by \a intr_src.
	 */
	dmar_unit = &dmar_drhd_units[1];

	/** Set 'ir_table' to the return value of 'hpa2hva(dmar_unit->ir_table_addr)', which points to
	 *  the interrupt remapping table associated with 'dmar_unit' */
	ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
	/** Set 'ir_entry' to 'ir_table + index', which points to the interrupt remapping entry associated with
	 *  \a index */
	ir_entry = ir_table + index;
	/** Set 'ir_entry->bits.present' to 0, indicating that interrupt requests referencing this entry is blocked by
	 *  hardware. */
	ir_entry->bits.present = 0x0UL;

	/** Call iommu_flush_cache with the following parameters, in order to flush cache lines that contain
	 *  the interrupt remapping entry (128-bit in size) pointed by 'ir_entry'.
	 *  - ir_entry
	 *  - sizeof(struct dmar_ir_entry)
	 */
	iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
	/** Call dmar_invalid_iec with the following parameters, in order to
	 *  submit an index-selective interrupt entry cache invalidation request to the DRHD structure specified
	 *  by \a dmar_unit.
	 *  - dmar_unit
	 *  - index
	 *  - 0H
	 *  - false
	 */
	dmar_invalid_iec(dmar_unit, index, 0U, false);
}

/**
 * @}
 */
