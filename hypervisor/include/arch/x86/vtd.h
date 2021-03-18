/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VTD_H
#define VTD_H

/**
 * @addtogroup hwmgmt_vtd
 *
 * @{
 */

/**
 * @file
 * @brief This file declares all external APIs that shall be provided by the hwmgmt.vtd module.
 *
 * This file declares all external functions and data structures that shall be provided by the hwmgmt.vtd module.
 * In addition, it defines some helper functions, data structures, and macros that are used to implement those
 * external APIs.
 *
 * Helper functions include: dma_ccmd_fm, dma_ccmd_sid, dma_ccmd_did, dma_iotlb_did, and dma_iec_index.
 */

#include <types.h>
#include <pci.h>
#include <platform_acpi_info.h>

/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */

#define DMAR_GCMD_REG   0x18U /**< Register offset of Global Command Register. */
#define DMAR_GSTS_REG   0x1cU /**< Register offset of Global Status Register. */
#define DMAR_RTADDR_REG 0x20U /**< Register offset of Root Table Address Register. */
#define DMAR_IQT_REG    0x88U /**< Register offset of Invalidation Queue Tail Register. */
#define DMAR_IQA_REG    0x90U /**< Register offset of Invalidation Queue Address Register. */
#define DMAR_IRTA_REG   0xb8U /**< Register offset of Interrupt Remapping Table Address Register. */

#define DMAR_GSTS_REG_MASK   0x96FFFFFFU /**< Mask of GSTS_REG enable/disable bits*/

/* Values for entry_type in ACPI_DMAR_DEVICE_SCOPE - device types */
/**
 * @brief Data structure to enumerate different Device Scope Entry types.
 *
 * Each enumeration member specifies one type.
 *
 * It is supposed to be used when hypervisor accesses Device Scope Entries.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
enum acpi_dmar_scope_type {
	/**
	 * @brief Reserved for future use.
	 *
	 * All values other than [1, 5] are reserved for future use according to VT-d specification.
	 */
	ACPI_DMAR_SCOPE_TYPE_NOT_USED = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT = 1, /**< PCI endpoint device. */
	ACPI_DMAR_SCOPE_TYPE_BRIDGE = 2, /**< PCI-PCI bridge. */
	ACPI_DMAR_SCOPE_TYPE_IOAPIC = 3, /**< I/O APIC (or I/O SAPIC) device. */
	ACPI_DMAR_SCOPE_TYPE_HPET = 4, /**< HPET. */
	ACPI_DMAR_SCOPE_TYPE_NAMESPACE = 5, /**< ACPI name-space enumerated device. */
	/**
	 * @brief Reserved for future use.
	 *
	 * All values other than [1, 5] are reserved for future use according to VT-d specification.
	 */
	ACPI_DMAR_SCOPE_TYPE_RESERVED = 6
};

/**
 * @brief Data structure to illustrate the IOMMU domain.
 *
 * Each VM has a dedicated IOMMU domain for it.
 *
 * It is supposed to be used when hypervisor accesses IOMMU domain for each VM.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct iommu_domain {
	uint16_t vm_id; /**< The VM identifier corresponding to this IOMMU domain. */
	uint32_t addr_width; /**< The address width (in bit) of this IOMMU domain. */
	uint64_t trans_table_ptr; /**< The base address of the translation table associated with this IOMMU domain. */
};

/**
 * @brief Data structure to illustrate different types of the interrupt source.
 *
 * Only MSI is supported in current scope.
 *
 * It is supposed to be used when hypervisor processes the interrupt.
 *
 * @consistency N/A
 * @alignment 2
 *
 * @remark N/A
 */
union source {
	union pci_bdf msi; /**< PCI BDF used to locate the interrupt source. */
};

/**
 * @brief Data structure to illustrate the interrupt source.
 *
 * It is supposed to be used when hypervisor processes the interrupt.
 *
 * @consistency N/A
 * @alignment 2
 *
 * @remark N/A
 */
struct intr_source {
	union source src; /**< The interrupt source. */
};

/* GCMD_REG */
/**
 * @brief Bit indicator for TE (Translation Enable) Bit in Global Command Register.
 */
#define DMA_GCMD_TE    (1U << 31U)
/**
 * @brief Bit indicator for SRTP (Set Root Table Pointer) Bit in Global Command Register.
 */
#define DMA_GCMD_SRTP  (1U << 30U)
/**
 * @brief Bit indicator for QIE (Queued Invalidation Enable) Bit in Global Command Register.
 */
#define DMA_GCMD_QIE   (1U << 26U)
/**
 * @brief Bit indicator for SIRTP (Set Interrupt Remap Table Pointer) Bit in Global Command Register.
 */
#define DMA_GCMD_SIRTP (1U << 24U)
/**
 * @brief Bit indicator for IRE (Interrupt Remapping Enable) Bit in Global Command Register.
 */
#define DMA_GCMD_IRE   (1U << 25U)

/* GSTS_REG */
/**
 * @brief Bit indicator for TES (Translation Enable Status) Bit in Global Status Register.
 */
#define DMA_GSTS_TES   (1U << 31U)
/**
 * @brief Bit indicator for RTPS (Root Table Pointer Status) Bit in Global Status Register.
 */
#define DMA_GSTS_RTPS  (1U << 30U)
/**
 * @brief Bit indicator for QIES (Queued Invalidation Enable Status) Bit in Global Status Register.
 */
#define DMA_GSTS_QIES  (1U << 26U)
/**
 * @brief Bit indicator for IRTPS (Interrupt Remapping Table Pointer Status) Bit in Global Status Register.
 */
#define DMA_GSTS_IRTPS (1U << 24U)
/**
 * @brief Bit indicator for IRES (Interrupt Remapping Enable Status) Bit in Global Status Register.
 */
#define DMA_GSTS_IRES  (1U << 25U)

/* CCMD_REG */
/**
 * @brief Setting of the granularity for Global Invalidation in a Context-cache Invalidate Descriptor.
 */
#define DMA_CONTEXT_GLOBAL_INVL (1UL << 4U)
/**
 * @brief Setting of the granularity for Device-Selective Invalidation in a Context-cache Invalidate Descriptor.
 */
#define DMA_CONTEXT_DEVICE_INVL (3UL << 4U)

/**
 * @brief Return a 64-bit value based on the specified Function Mask.
 *
 * Return a 64-bit value based on the specified Function Mask in order to assist the caller to place the specified
 * Function Mask into a Context-cache Invalidate Descriptor.
 *
 * The Function Mask field is located at bits [49:48] in the Context-cache Invalidate Descriptor and
 * it indicates the bits of the Source-ID field to be masked for device-selective invalidations.
 *
 * It is supposed to be called by 'dmar_invalid_context_cache'.
 *
 * @param[in] fm The specified Function Mask.
 *
 * @return A 64-bit value, its bits [49:48] is equal to bits [1:0] of \a fm and all the other bits are 0.
 *
 * @pre N/A
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
static inline uint64_t dma_ccmd_fm(uint8_t fm)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Cast \a fm to unsigned 64 bit integer;
	 *  2. Get the bitwise AND of two operands: 3H and the result of the step-1;
	 *  3. Shift the result of step-2 left for 48 bits and return it.
	 */
	return (((uint64_t)fm & 0x3UL) << 48UL);
}

/**
 * @brief Return a 64-bit value based on the specified Source-ID.
 *
 * Return a 64-bit value based on the specified Source-ID in order to assist the caller to place the specified
 * Source-ID into a Context-cache Invalidate Descriptor.
 *
 * The Source-ID field is located at bits [47:32] in the Context-cache Invalidate Descriptor and
 * it indicates the device source-id for device-selective invalidations.
 *
 * It is supposed to be called by 'dmar_invalid_context_cache'.
 *
 * @param[in] sid The specified Source-ID.
 *
 * @return A 64-bit value, its bits [47:32] is equal to \a sid and all the other bits are 0.
 *
 * @pre N/A
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
static inline uint64_t dma_ccmd_sid(uint16_t sid)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Cast \a sid to unsigned 64 bit integer;
	 *  2. Get the bitwise AND of two operands: ffffH and the result of the step-1;
	 *  3. Shift the result of step-2 left for 32 bits and return it.
	 */
	return (((uint64_t)sid & 0xffffUL) << 32UL);
}

/**
 * @brief Return a 64-bit value based on the specified Domain-ID.
 *
 * Return a 64-bit value based on the specified Domain-ID in order to assist the caller to place the specified
 * Domain-ID into a Context-cache Invalidate Descriptor.
 *
 * The Domain-ID field is located at bits [31:16] in the Context-cache Invalidate Descriptor and
 * it indicates the target domain-id for device-selective invalidations.
 *
 * It is supposed to be called by 'dmar_invalid_context_cache'.
 *
 * @param[in] did The specified Domain-ID.
 *
 * @return A 64-bit value, its bits [31:16] is equal to \a did and all the other bits are 0.
 *
 * @pre N/A
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
static inline uint64_t dma_ccmd_did(uint16_t did)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Cast \a did to unsigned 64 bit integer;
	 *  2. Get the bitwise AND of two operands: ffffH and the result of the step-1;
	 *  3. Shift the result of step-2 left for 16 bits and return it.
	 */
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

/**
 * @brief Setting of the granularity for Global Invalidation in an IOTLB Invalidate Descriptor.
 */
#define DMA_IOTLB_GLOBAL_INVL (((uint64_t)1UL) << 4U)
/**
 * @brief Setting of the granularity for Domain-Selective Invalidation in an IOTLB Invalidate Descriptor.
 */
#define DMA_IOTLB_DOMAIN_INVL (((uint64_t)2UL) << 4U)

/**
 * @brief Bit indicator for Drain Reads Bit in IOTLB Invalidate Descriptor.
 */
#define DMA_IOTLB_DR          (((uint64_t)1UL) << 7U)
/**
 * @brief Bit indicator for Drain Writes Bit in IOTLB Invalidate Descriptor.
 */
#define DMA_IOTLB_DW          (((uint64_t)1UL) << 6U)

/**
 * @brief Return a 64-bit value based on the specified Domain-ID.
 *
 * Return a 64-bit value based on the specified Domain-ID in order to assist the caller to place the specified
 * Domain-ID into an IOTLB Invalidate Descriptor.
 *
 * The Domain-ID field is located at bits [31:16] in the IOTLB Invalidate Descriptor and
 * it indicates the target domain-id for domain-selective invalidations.
 *
 * It is supposed to be called by 'dmar_invalid_iotlb'.
 *
 * @param[in] did The specified Domain-ID.
 *
 * @return A 64-bit value, its bits [31:16] is equal to \a did and all the other bits are 0.
 *
 * @pre N/A
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
static inline uint64_t dma_iotlb_did(uint16_t did)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Cast \a did to unsigned 64 bit integer;
	 *  2. Get the bitwise AND of two operands: ffffH and the result of the step-1;
	 *  3. Shift the result of step-2 left for 16 bits and return it.
	 */
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

/* IEC_REG */
/**
 * @brief Setting of the granularity for Index-Selective Invalidation in an Interrupt Entry Cache Invalidate Descriptor.
 */
#define DMAR_IECI_INDEXED    (((uint64_t)1UL) << 4U)
/**
 * @brief Setting of the granularity for Global Invalidation in an Interrupt Entry Cache Invalidate Descriptor.
 */
#define DMAR_IEC_GLOBAL_INVL (((uint64_t)0UL) << 4U)

/**
 * @brief Return a 64-bit value based on the specified Interrupt Index and the specified Index Mask.
 *
 * Return a 64-bit value based on the specified Interrupt Index and the specified Index Mask in order to assist
 * the caller to place the specified Interrupt Index and the specified Index Mask into
 * an Interrupt Entry Cache Invalidate Descriptor.
 *
 * The Interrupt Index field is located at bits [47:32] in the Interrupt Entry Cache Invalidate Descriptor and
 * it specifies the index of the interrupt remapping entry that needs to be invalidated through a
 * index-selective invalidation.
 * The Index Mask field is located at bits [31:27] in the Interrupt Entry Cache Invalidate Descriptor and
 * it specifies the number of contiguous interrupt indexes that needs to be invalidated for
 * index-selective invalidations, refer to VT-d specification for detailed encodings.
 *
 * It is supposed to be called by 'dmar_invalid_iec'.
 *
 * @param[in] index The specified Interrupt Index.
 * @param[in] index_mask The specified Index Mask.
 *
 * @return A 64-bit value, its bits [47:32] is equal to \a index, its bits [31:27] is equal to bits [4:0]
 *         of \a index_mask, and all the other bits are 0.
 *
 * @pre N/A
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
static inline uint64_t dma_iec_index(uint16_t index, uint8_t index_mask)
{
	/** Return a 64-bit value via following arithmetic operations:
	 *  1. Cast \a index to unsigned 64 bit integer;
	 *  2. Get the bitwise AND of two operands: ffffH and the result of the step-1;
	 *  3. Shift the result of step-2 left for 32 bits.
	 *  4. Cast \a index_mask to unsigned 64 bit integer;
	 *  5. Get the bitwise AND of two operands: 1fH and the result of the step-4;
	 *  6. Shift the result of step-5 left for 27 bits.
	 *  7. Get the bitwise OR of two operands and return it: the result of the step-3 and the result of the step-6.
	 */
	return ((((uint64_t)index & 0xFFFFU) << 32U) | (((uint64_t)index_mask & 0x1FU) << 27U));
}

/**
 * @brief Data structure to store the physical information for each Device Scope Entry.
 *
 * Refer to VT-d specification for detailed description of each field defined in this data structure.
 *
 * It is supposed to be used when hypervisor accesses Device Scope Entries.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct dmar_dev_scope {
	/**
	 * @brief The type of this Device Scope Entry.
	 */
	enum acpi_dmar_scope_type type;
	/**
	 * @brief The enumeration ID associated with this Device Scope Entry.
	 */
	uint8_t id;
	/**
	 * @brief The start bus number associated with this Device Scope Entry.
	 *
	 * This field describes the bus number (bus number of the first PCI Bus produced by the PCI Host Bridge)
	 * under which the device identified by this Device Scope resides.
	 */
	uint8_t bus;
	/**
	 * @brief The path associated with this Device Scope Entry.
	 *
	 * It describes the hierarchical path from the Host Bridge to the device specified by this Device Scope Entry.
	 */
	uint8_t devfun;
};

/**
 * @brief Data structure to store the physical information for each DRHD structure.
 *
 * Data structure to store the physical information for each DRHD (DMA Remapping Hardware Unit Definition) structure.
 * A DRHD structure uniquely represents a remapping hardware unit present in the platform.
 *
 * It is supposed to be used when hypervisor accesses DRHD.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct dmar_drhd {
	/**
	 * @brief The number of Device Scope Entries supported by this unit.
	 */
	uint32_t dev_cnt;
	/**
	 * @brief The PCI Segment associated with this unit.
	 */
	uint16_t segment;
	/**
	 * @brief Flags associated with this unit.
	 *
	 * Bit 0 of this field is defined as INCLUDE_PCI_ALL flag.
	 * If INCLUDE_PCI_ALL flag is set to 1H, this remapping hardware unit has under its scope all PCI
	 * compatible devices in the specified Segment, except devices reported under the scope of other
	 * remapping hardware units for the same Segment.
	 * If INCLUDE_PCI_ALL flag is set to 0H, this remapping hardware unit has under its scope only devices
	 * in the specified Segment that are explicitly identified through the 'Device Scope' field.
	 *
	 * All the other bits are reserved.
	 */
	uint8_t flags;
	/**
	 * @brief A flag to indicate whether this unit is ignored by hypervisor or not.
	 */
	bool ignore;
	/**
	 * @brief Base address of remapping hardware register-set for this unit.
	 */
	uint64_t reg_base_addr;

	/* assume no pci device hotplug support */
	/**
	 * @brief A pointer to the Device Scope structure associated with this unit.
	 *
	 * The Device Scope structure contains zero or more Device Scope Entries that identify devices in the
	 * specified segment and under the scope of this remapping hardware unit.
	 */
	struct dmar_dev_scope *devices;
};

/**
 * @brief Data structure to store the physical information of the remapping hardware.
 *
 * It is supposed to be used when hypervisor accesses the remapping hardware.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct dmar_info {
	/**
	 * @brief The number of all DRHD structures present in the platform.
	 *
	 * A DMA-remapping hardware unit definition (DRHD) structure uniquely represents a remapping hardware unit
	 * present in the platform.
	 */
	uint32_t drhd_count;
	/**
	 * @brief A pointer to all DRHD structures present in the platform.
	 */
	struct dmar_drhd *drhd_units;
};

/**
 * @brief Data structure to illustrate the table entry for DMA remapping and interrupt remapping.
 *
 * It is supposed to be used when hypervisor accesses the table entry for DMA remapping and interrupt remapping.
 *
 * @consistency N/A
 * @alignment 16
 *
 * @remark N/A
 */
struct dmar_entry {
	uint64_t lo_64; /**< Low 64-bit of the table entry. */
	uint64_t hi_64; /**< High 64-bit of the table entry. */
};

/**
 * @brief Data structure to illustrate the interrupt remapping table entry.
 *
 * It is supposed to be used when hypervisor accesses the table entry for interrupt remapping.
 *
 * @consistency N/A
 * @alignment 16
 *
 * @remark This data structure is packed.
 */
union dmar_ir_entry {
	struct dmar_entry entry; /**< The interrupt remapping table entry. */

	/**
	 * @brief Data structure to illustrate the interrupt remapping table entry with each field specified.
	 */
	struct {
		/**
		 * @brief Present.
		 *
		 * This field is used by software to indicate to hardware if the corresponding IRTE is present
		 * and initialized.
		 */
		uint64_t present : 1;
		/**
		 * @brief Fault Processing Disable.
		 *
		 * Enables or disables recording/reporting of faults caused by interrupt messages requests processed
		 * through this entry.
		 */
		uint64_t fpd : 1;
		/**
		 * @brief Destination Mode.
		 *
		 * This field indicates whether the Destination ID field in an IRTE should be interpreted as
		 * logical or physical APIC ID.
		 */
		uint64_t dest_mode : 1;
		/**
		 * @brief Redirection Hint.
		 *
		 * This bit indicates whether the remapped interrupt request should be directed to one among
		 * N processors specified in Destination ID field, under hardware control.
		 */
		uint64_t rh : 1;
		/**
		 * @brief Trigger Mode.
		 *
		 * This field indicates the signal type of the interrupt that uses the IRTE.
		 */
		uint64_t trigger_mode : 1;
		/**
		 * @brief Delivery Mode.
		 *
		 * This 3-bit field specifies how the remapped interrupt is handled.
		 */
		uint64_t delivery_mode : 3;
		/**
		 * @brief Bits for software.
		 *
		 * This field is available to software. Hardware always ignores the programming of this field.
		 */
		uint64_t sw_bits : 4;
		/**
		 * @brief Reserved Bits.
		 */
		uint64_t rsvd_1 : 3;
		/**
		 * @brief IRTE(Interrupt Remapping Table Entry) Mode.
		 *
		 * This field indicates interrupt requests processed through this IRTE are remapped or posted.
		 */
		uint64_t mode : 1;
		/**
		 * @brief Interrupt Vector.
		 *
		 * This 8-bit field contains the interrupt vector associated with the remapped interrupt request.
		 */
		uint64_t vector : 8;
		/**
		 * @brief Reserved Bits.
		 */
		uint64_t rsvd_2 : 8;
		/**
		 * @brief Destination ID.
		 *
		 * This field identifies the remapped interrupt request's target processor(s).
		 */
		uint64_t dest : 32;
		/**
		 * @brief Source Identifier.
		 *
		 * This field specifies the originator (source) of the interrupt request that references this IRTE.
		 */
		uint64_t sid : 16;
		/**
		 * @brief Source-id Qualifier.
		 *
		 * This field specifies how to verify origination of interrupt requests.
		 */
		uint64_t sq : 2;
		/**
		 * @brief Source Validation Type.
		 *
		 * This field specifies the type of validation that must be performed by the interrupt-remapping
		 * hardware on the source-id of the interrupt requests referencing this IRTE.
		 */
		uint64_t svt : 2;
		/**
		 * @brief Reserved Bits.
		 */
		uint64_t rsvd_3 : 44;
	} bits __packed;
};

int32_t remove_iommu_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun);
int32_t add_iommu_device(struct iommu_domain *domain, uint8_t bus, uint8_t devfun);

struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width);
void destroy_iommu_domain(struct iommu_domain *domain);

void enable_iommu(void);
void init_iommu(void);

void dmar_assign_irte(struct intr_source intr_src, union dmar_ir_entry irte, uint16_t index);
void dmar_free_irte(__unused struct intr_source intr_src, uint16_t index);

void iommu_flush_cache(const void *p, uint32_t size);

/**
 * @brief This is the declaration of the global variable 'plat_dmar_info' that stores the physical information of
 *        the remapping hardware.
 */
extern struct dmar_info plat_dmar_info;

/**
 * @}
 */

#endif
