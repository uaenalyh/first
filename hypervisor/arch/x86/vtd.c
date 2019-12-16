/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
 * @defgroup hwmgmt_iommu hwmgmt.iommu
 * @ingroup hwmgmt
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define ACRN_DBG_IOMMU 6U
#define LEVEL_WIDTH    9U

#define ROOT_ENTRY_LOWER_PRESENT_POS  (0U)
#define ROOT_ENTRY_LOWER_PRESENT_MASK (1UL << ROOT_ENTRY_LOWER_PRESENT_POS)
#define ROOT_ENTRY_LOWER_CTP_POS      (12U)
#define ROOT_ENTRY_LOWER_CTP_MASK     (0xFFFFFFFFFFFFFUL << ROOT_ENTRY_LOWER_CTP_POS)

#define CONFIG_MAX_IOMMU_NUM DRHD_COUNT

#define CTX_ENTRY_UPPER_AW_POS       (0U)
#define CTX_ENTRY_UPPER_AW_MASK      (0x7UL << CTX_ENTRY_UPPER_AW_POS)
#define CTX_ENTRY_UPPER_DID_POS      (8U)
#define CTX_ENTRY_UPPER_DID_MASK     (0xFFFFUL << CTX_ENTRY_UPPER_DID_POS)
#define CTX_ENTRY_LOWER_P_POS        (0U)
#define CTX_ENTRY_LOWER_P_MASK       (0x1UL << CTX_ENTRY_LOWER_P_POS)
#define CTX_ENTRY_LOWER_TT_POS       (2U)
#define CTX_ENTRY_LOWER_TT_MASK      (0x3UL << CTX_ENTRY_LOWER_TT_POS)
#define CTX_ENTRY_LOWER_SLPTPTR_POS  (12U)
#define CTX_ENTRY_LOWER_SLPTPTR_MASK (0xFFFFFFFFFFFFFUL << CTX_ENTRY_LOWER_SLPTPTR_POS)

static inline uint64_t dmar_get_bitslice(uint64_t var, uint64_t mask, uint32_t pos)
{
	return ((var & mask) >> pos);
}

static inline uint64_t dmar_set_bitslice(uint64_t var, uint64_t mask, uint32_t pos, uint64_t val)
{
	return ((var & ~mask) | ((val << pos) & mask));
}

/* translation type */
#define DMAR_CTX_TT_UNTRANSLATED 0x0UL

#define DMAR_INVALIDATION_QUEUE_SIZE 4096U
#define DMAR_QI_INV_ENTRY_SIZE       16U
#define DMAR_NUM_IR_ENTRIES_PER_PAGE 256U

#define DMAR_INV_STATUS_WRITE_SHIFT 5U
#define DMAR_INV_CONTEXT_CACHE_DESC 0x01UL
#define DMAR_INV_IOTLB_DESC         0x02UL
#define DMAR_INV_IEC_DESC           0x04UL
#define DMAR_INV_WAIT_DESC          0x05UL
#define DMAR_INV_STATUS_WRITE       (1UL << DMAR_INV_STATUS_WRITE_SHIFT)
#define DMAR_INV_STATUS_INCOMPLETE  0UL
#define DMAR_INV_STATUS_COMPLETED   1UL
#define DMAR_INV_STATUS_DATA_SHIFT  32U
#define DMAR_INV_STATUS_DATA        (DMAR_INV_STATUS_COMPLETED << DMAR_INV_STATUS_DATA_SHIFT)
#define DMAR_INV_WAIT_DESC_LOWER    (DMAR_INV_STATUS_WRITE | DMAR_INV_WAIT_DESC | DMAR_INV_STATUS_DATA)

#define DMAR_IR_ENABLE_EIM_SHIFT 11UL
#define DMAR_IR_ENABLE_EIM       (1UL << DMAR_IR_ENABLE_EIM_SHIFT)

enum dmar_cirg_type { DMAR_CIRG_RESERVED = 0, DMAR_CIRG_GLOBAL, DMAR_CIRG_DOMAIN, DMAR_CIRG_DEVICE };

enum dmar_iirg_type { DMAR_IIRG_RESERVED = 0, DMAR_IIRG_GLOBAL, DMAR_IIRG_DOMAIN, DMAR_IIRG_PAGE };

/* dmar unit runtime data */
struct dmar_drhd_rt {
	uint32_t index;
	spinlock_t lock;

	struct dmar_drhd *drhd;

	uint64_t root_table_addr;
	uint64_t ir_table_addr;
	uint64_t qi_queue;
	uint16_t qi_tail;

	uint32_t gcmd; /* sw cache value of global cmd register */
};

struct context_table {
	struct page buses[CONFIG_IOMMU_BUS_NUM];
};

struct intr_remap_table {
	struct page tables[CONFIG_MAX_IR_ENTRIES / DMAR_NUM_IR_ENTRIES_PER_PAGE];
};

static inline uint8_t *get_root_table(uint32_t dmar_index)
{
	static struct page root_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return root_tables[dmar_index].contents;
}

static inline uint8_t *get_ctx_table(uint32_t dmar_index, uint8_t bus_no)
{
	static struct context_table ctx_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return ctx_tables[dmar_index].buses[bus_no].contents;
}

/*
 * @pre dmar_index < CONFIG_MAX_IOMMU_NUM
 */
static inline uint8_t *get_qi_queue(uint32_t dmar_index)
{
	static struct page qi_queues[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return qi_queues[dmar_index].contents;
}

static inline uint8_t *get_ir_table(uint32_t dmar_index)
{
	static struct intr_remap_table ir_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return ir_tables[dmar_index].tables[0].contents;
}

static struct dmar_drhd_rt dmar_drhd_units[MAX_DRHDS];
static uint32_t qi_status = 0U;
static struct dmar_info *platform_dmar_info = NULL;

/* Domain id 0 is reserved in some cases per VT-d */
#define MAX_DOMAIN_NUM (CONFIG_MAX_VM_NUM + 1)

static inline uint16_t vmid_to_domainid(uint16_t vm_id)
{
	return vm_id + 1U;
}

static void dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit);
static void register_hrhd_units(void)
{
	struct dmar_drhd_rt *drhd_rt;
	uint32_t i;

	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		drhd_rt = &dmar_drhd_units[i];
		drhd_rt->index = i;
		drhd_rt->drhd = &platform_dmar_info->drhd_units[i];

		hv_access_memory_region_update(drhd_rt->drhd->reg_base_addr, PAGE_SIZE);

		dmar_register_hrhd(drhd_rt);
	}
}

static uint32_t iommu_read32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	return mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static void iommu_write32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t value)
{
	mmio_write32(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static void iommu_write64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint64_t value)
{
	uint32_t temp;

	temp = (uint32_t)value;
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));

	temp = (uint32_t)(value >> 32U);
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset + 4U));
}

static inline void dmar_wait_completion(
	const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t mask, bool pre_condition, uint32_t *status)
{
	/* variable start isn't used when built as release version */
	__unused uint64_t start = rdtsc();
	bool condition, temp_condition;

	while (1) {
		*status = iommu_read32(dmar_unit, offset);
		temp_condition = ((*status & mask) == 0U) ? true : false;

		/*
		 * pre_condition    temp_condition    | condition
		 * -----------------------------------|----------
		 * true             true              | true
		 * true             false             | false
		 * false            true              | false
		 * false            false             | true
		 */
		condition = (temp_condition == pre_condition) ? true : false;

		if (condition) {
			break;
		}
		ASSERT(((rdtsc() - start) < CYCLES_PER_MS), "DMAR OP Timeout!");
		asm_pause();
	}
}

/* Flush CPU cache when root table, context table or second-level translation teable updated
 * In the context of ACRN, GPA to HPA mapping relationship is not changed after VM created,
 * skip flushing iotlb to avoid performance penalty.
 */
void iommu_flush_cache(const void *p, uint32_t size)
{

	uint32_t i;

	for (i = 0U; i < size; i += CACHE_LINE_SIZE) {
		clflush((const char *)p + i);
	}
}

static inline uint8_t width_to_level(uint32_t width)
{
	return (uint8_t)(((width - 12U) + (LEVEL_WIDTH)-1U) / (LEVEL_WIDTH));
}

static inline uint8_t width_to_agaw(uint32_t width)
{
	return width_to_level(width) - 2U;
}

static void dmar_enable_intr_remapping(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_IRE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_IRE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, false, &status);
	}

	spinlock_release(&(dmar_unit->lock));
	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_enable_translation(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_TE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_TE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, false, &status);
	}

	spinlock_release(&(dmar_unit->lock));

	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_disable_translation(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_TE) != 0U) {
		dmar_unit->gcmd &= ~DMA_GCMD_TE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, true, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(ACRN_DBG_IOMMU, "Register dmar uint [%d] @0x%lx", dmar_unit->index, dmar_unit->drhd->reg_base_addr);

	spinlock_init(&dmar_unit->lock);

	/*
	 * The initialization of "dmar_unit->gcmd" shall be done via reading from Global Status Register rather than
	 * Global Command Register.
	 * According to Chapter 10.4.4 Global Command Register in VT-d spec, Global Command Register is a write-only
	 * register to control remapping hardware. Global Status Register is the corresponding read-only register to
	 * report remapping hardware status.
	 */
	dmar_unit->gcmd = iommu_read32(dmar_unit, DMAR_GSTS_REG);

	dmar_disable_translation(dmar_unit);
}

static void dmar_issue_qi_request(struct dmar_drhd_rt *dmar_unit, struct dmar_entry invalidate_desc)
{
	struct dmar_entry *invalidate_desc_ptr;
	__unused uint64_t start;

	invalidate_desc_ptr = (struct dmar_entry *)(dmar_unit->qi_queue + dmar_unit->qi_tail);

	invalidate_desc_ptr->hi_64 = invalidate_desc.hi_64;
	invalidate_desc_ptr->lo_64 = invalidate_desc.lo_64;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	invalidate_desc_ptr++;

	invalidate_desc_ptr->hi_64 = hva2hpa(&qi_status);
	invalidate_desc_ptr->lo_64 = DMAR_INV_WAIT_DESC_LOWER;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	qi_status = DMAR_INV_STATUS_INCOMPLETE;
	iommu_write32(dmar_unit, DMAR_IQT_REG, dmar_unit->qi_tail);

	start = rdtsc();
	while (qi_status == DMAR_INV_STATUS_INCOMPLETE) {
		if (qi_status == DMAR_INV_STATUS_COMPLETED) {
			break;
		}
		if ((rdtsc() - start) > CYCLES_PER_MS) {
			pr_err("DMAR OP Timeout! @ %s", __func__);
		}
		asm_pause();
	}
}

/*
 * did: domain id
 * sid: source id
 * fm: function mask
 * cirg: cache-invalidation request granularity
 */
static void dmar_invalid_context_cache(
	struct dmar_drhd_rt *dmar_unit, uint16_t did, uint16_t sid, uint8_t fm, enum dmar_cirg_type cirg)
{
	struct dmar_entry invalidate_desc;

	invalidate_desc.hi_64 = 0UL;
	invalidate_desc.lo_64 = DMAR_INV_CONTEXT_CACHE_DESC;
	switch (cirg) {
	case DMAR_CIRG_GLOBAL:
		invalidate_desc.lo_64 |= DMA_CONTEXT_GLOBAL_INVL;
		break;
	case DMAR_CIRG_DEVICE:
		invalidate_desc.lo_64 |=
			DMA_CONTEXT_DEVICE_INVL | dma_ccmd_did(did) | dma_ccmd_sid(sid) | dma_ccmd_fm(fm);
		break;
	default:
		invalidate_desc.lo_64 = 0UL;
		pr_err("unknown CIRG type");
		break;
	}

	if (invalidate_desc.lo_64 != 0UL) {
		spinlock_obtain(&(dmar_unit->lock));

		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		spinlock_release(&(dmar_unit->lock));
	}
}

static void dmar_invalid_context_cache_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_context_cache(dmar_unit, 0U, 0U, 0U, DMAR_CIRG_GLOBAL);
}

static void dmar_invalid_iotlb(
	struct dmar_drhd_rt *dmar_unit, uint16_t did, uint64_t address, uint8_t am, bool hint, enum dmar_iirg_type iirg)
{
	/* set Drain Reads & Drain Writes,
	 * if hardware doesn't support it, will be ignored by hardware
	 */
	struct dmar_entry invalidate_desc;
	uint64_t addr = 0UL;

	invalidate_desc.hi_64 = 0UL;

	invalidate_desc.lo_64 = DMA_IOTLB_DR | DMA_IOTLB_DW | DMAR_INV_IOTLB_DESC;

	switch (iirg) {
	case DMAR_IIRG_GLOBAL:
		invalidate_desc.lo_64 |= DMA_IOTLB_GLOBAL_INVL;
		break;
	case DMAR_IIRG_DOMAIN:
		invalidate_desc.lo_64 |= DMA_IOTLB_DOMAIN_INVL | dma_iotlb_did(did);
		break;
	default:
		invalidate_desc.lo_64 = 0UL;
		pr_err("unknown IIRG type");
	}

	if (invalidate_desc.lo_64 != 0UL) {
		spinlock_obtain(&(dmar_unit->lock));

		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		spinlock_release(&(dmar_unit->lock));
	}
}

/* Invalidate IOTLB globally,
 * all iotlb entries are invalidated,
 * all PASID-cache entries are invalidated,
 * all paging-structure-cache entries are invalidated.
 */
static void dmar_invalid_iotlb_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_iotlb(dmar_unit, 0U, 0UL, 0U, false, DMAR_IIRG_GLOBAL);
}

static void dmar_set_intr_remap_table(struct dmar_drhd_rt *dmar_unit)
{
	uint64_t address;
	uint32_t status;
	uint8_t size;

	spinlock_obtain(&(dmar_unit->lock));

	if (dmar_unit->ir_table_addr == 0UL) {
		dmar_unit->ir_table_addr = hva2hpa(get_ir_table(dmar_unit->index));
	}

	address = dmar_unit->ir_table_addr | DMAR_IR_ENABLE_EIM;

	/* Set number of bits needed to represent the entries minus 1 */
	size = (uint8_t)fls32(CONFIG_MAX_IR_ENTRIES) - 1U;
	address = address | size;

	iommu_write64(dmar_unit, DMAR_IRTA_REG, address);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SIRTP);

	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRTPS, false, &status);

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_invalid_iec(struct dmar_drhd_rt *dmar_unit, uint16_t intr_index, uint8_t index_mask, bool is_global)
{
	struct dmar_entry invalidate_desc;

	invalidate_desc.hi_64 = 0UL;
	invalidate_desc.lo_64 = DMAR_INV_IEC_DESC;

	if (is_global) {
		invalidate_desc.lo_64 |= DMAR_IEC_GLOBAL_INVL;
	} else {
		invalidate_desc.lo_64 |= DMAR_IECI_INDEXED | dma_iec_index(intr_index, index_mask);
	}

	spinlock_obtain(&(dmar_unit->lock));

	dmar_issue_qi_request(dmar_unit, invalidate_desc);

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_invalid_iec_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_iec(dmar_unit, 0U, 0U, true);
}

static void dmar_set_root_table(struct dmar_drhd_rt *dmar_unit)
{
	uint64_t address;
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));

	/*
	 * dmar_set_root_table is called from init_iommu and
	 * resume_iommu. So NULL check on this pointer is needed
	 * so that we do not change the root table pointer in the
	 * resume flow.
	 */

	if (dmar_unit->root_table_addr == 0UL) {
		dmar_unit->root_table_addr = hva2hpa(get_root_table(dmar_unit->index));
	}

	/* Currently don't support extended root table */
	address = dmar_unit->root_table_addr;

	iommu_write64(dmar_unit, DMAR_RTADDR_REG, address);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_RTPS, false, &status);
	spinlock_release(&(dmar_unit->lock));
}

static void dmar_enable_qi(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));

	dmar_unit->qi_queue = hva2hpa(get_qi_queue(dmar_unit->index));
	iommu_write64(dmar_unit, DMAR_IQA_REG, dmar_unit->qi_queue);

	iommu_write32(dmar_unit, DMAR_IQT_REG, 0U);

	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_QIE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, false, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_prepare(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_set_root_table(dmar_unit);
	dmar_enable_qi(dmar_unit);
	dmar_set_intr_remap_table(dmar_unit);
}

static void dmar_enable(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_invalid_context_cache_global(dmar_unit);
	dmar_invalid_iotlb_global(dmar_unit);
	dmar_invalid_iec_global(dmar_unit);
	dmar_enable_translation(dmar_unit);
}

int32_t add_iommu_device(struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_entry *context;
	struct dmar_entry *root_entry;
	struct dmar_entry *context_entry;
	uint64_t hi_64;
	uint64_t lo_64 = 0UL;
	int32_t ret = 0;
	/* source id */
	union pci_bdf sid;

	sid.fields.bus = bus;
	sid.fields.devfun = devfun;

	dmar_unit = &dmar_drhd_units[1];

	root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);

	root_entry = root_table + bus;

	if (dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS) == 0UL) {
		/* create context table for the bus if not present */
		context_table_addr = hva2hpa(get_ctx_table(dmar_unit->index, bus));

		context_table_addr = context_table_addr >> PAGE_SHIFT;

		lo_64 = dmar_set_bitslice(
			lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS, context_table_addr);
		lo_64 = dmar_set_bitslice(lo_64, ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS, 1UL);

		root_entry->hi_64 = 0UL;
		root_entry->lo_64 = lo_64;
		iommu_flush_cache(root_entry, sizeof(struct dmar_entry));
	} else {
		context_table_addr =
			dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
	}

	context_table_addr = context_table_addr << PAGE_SHIFT;

	context = (struct dmar_entry *)hpa2hva(context_table_addr);
	context_entry = context + devfun;
	/* setup context entry for the devfun */
	hi_64 = 0UL;
	lo_64 = 0UL;
	/* TODO: add Device TLB support */
	hi_64 = dmar_set_bitslice(
		hi_64, CTX_ENTRY_UPPER_AW_MASK, CTX_ENTRY_UPPER_AW_POS, (uint64_t)width_to_agaw(domain->addr_width));
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_TT_MASK, CTX_ENTRY_LOWER_TT_POS, DMAR_CTX_TT_UNTRANSLATED);

	hi_64 = dmar_set_bitslice(hi_64, CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS,
		(uint64_t)vmid_to_domainid(domain->vm_id));
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_SLPTPTR_MASK, CTX_ENTRY_LOWER_SLPTPTR_POS,
		domain->trans_table_ptr >> PAGE_SHIFT);
	lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS, 1UL);

	context_entry->hi_64 = hi_64;
	context_entry->lo_64 = lo_64;
	iommu_flush_cache(context_entry, sizeof(struct dmar_entry));

	return ret;
}

int32_t remove_iommu_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_entry *context;
	struct dmar_entry *root_entry;
	struct dmar_entry *context_entry;
	/* source id */
	union pci_bdf sid;
	int32_t ret = 0;

	dmar_unit = &dmar_drhd_units[1];

	sid.fields.bus = bus;
	sid.fields.devfun = devfun;

	root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);
	root_entry = root_table + bus;

	context_table_addr = dmar_get_bitslice(root_entry->lo_64, ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
	context_table_addr = context_table_addr << PAGE_SHIFT;
	context = (struct dmar_entry *)hpa2hva(context_table_addr);

	context_entry = context + devfun;
	/* clear the present bit first */
	context_entry->lo_64 = 0UL;
	context_entry->hi_64 = 0UL;
	iommu_flush_cache(context_entry, sizeof(struct dmar_entry));

	dmar_invalid_context_cache(dmar_unit, vmid_to_domainid(domain->vm_id), sid.value, 0U, DMAR_CIRG_DEVICE);
	dmar_invalid_iotlb(dmar_unit, vmid_to_domainid(domain->vm_id), 0UL, 0U, false, DMAR_IIRG_DOMAIN);
	return ret;
}

/*
 * @pre action != NULL
 * As an internal API, VT-d code can guarantee action is not NULL.
 */
static void do_action_for_iommus(void (*action)(struct dmar_drhd_rt *))
{
	struct dmar_drhd_rt *dmar_unit;
	uint32_t i;

	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		dmar_unit = &dmar_drhd_units[i];
		if (!dmar_unit->drhd->ignore) {
			action(dmar_unit);
		} else {
			dev_dbg(ACRN_DBG_IOMMU, "ignore dmar_unit @0x%x", dmar_unit->drhd->reg_base_addr);
		}
	}
}

struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width)
{
	static struct iommu_domain iommu_domains[MAX_DOMAIN_NUM];
	struct iommu_domain *domain;
	/*
	 * A hypercall is called to create an iommu domain for a valid VM,
	 * and hv code limit the VM number to CONFIG_MAX_VM_NUM.
	 * So the array iommu_domains will not be accessed out of range.
	 */
	domain = &iommu_domains[vmid_to_domainid(vm_id)];

	domain->vm_id = vm_id;
	domain->trans_table_ptr = translation_table;
	domain->addr_width = addr_width;

	dev_dbg(ACRN_DBG_IOMMU, "create domain [%d]: vm_id = %hu, ept@0x%x", vmid_to_domainid(domain->vm_id),
		domain->vm_id, domain->trans_table_ptr);

	return domain;
}

/**
 * @pre domain != NULL
 */
void destroy_iommu_domain(struct iommu_domain *domain)
{

	/* TODO: check if any device assigned to this domain */
	(void)memset(domain, 0U, sizeof(*domain));
}

/*
 * @pre (from_domain != NULL) || (to_domain != NULL)
 */

void enable_iommu(void)
{
	do_action_for_iommus(dmar_enable);
}

/**
 * @post return != NULL
 * @post return->drhd_count > 0U
 */
static struct dmar_info *get_dmar_info(void)
{
	return &plat_dmar_info;
}

void init_iommu(void)
{
	platform_dmar_info = get_dmar_info();

	register_hrhd_units();

	do_action_for_iommus(dmar_prepare);
}

void dmar_assign_irte(struct intr_source intr_src, union dmar_ir_entry irte, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;
	uint64_t trigger_mode;

	dmar_unit = &dmar_drhd_units[1];
	sid.value = intr_src.src.msi.value;
	trigger_mode = 0x0UL;

	dmar_enable_intr_remapping(dmar_unit);
	irte.bits.svt = 0x1UL;
	irte.bits.sq = 0x0UL;
	irte.bits.sid = sid.value;
	irte.bits.present = 0x1UL;
	irte.bits.mode = 0x0UL;
	irte.bits.trigger_mode = trigger_mode;
	irte.bits.fpd = 0x0UL;
	ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
	ir_entry = ir_table + index;
	ir_entry->entry.hi_64 = irte.entry.hi_64;
	ir_entry->entry.lo_64 = irte.entry.lo_64;

	iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
	dmar_invalid_iec(dmar_unit, index, 0U, false);
}

void dmar_free_irte(struct intr_source intr_src, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;

	dmar_unit = &dmar_drhd_units[1];

	ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
	ir_entry = ir_table + index;
	ir_entry->bits.present = 0x0UL;

	iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
	dmar_invalid_iec(dmar_unit, index, 0U, false);
}

/**
 * @}
 */
