/*
 * common definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file acrn_common.h
 *
 * @brief acrn common data structure for hypercall or ioctl
 */

#ifndef ACRN_COMMON_H
#define ACRN_COMMON_H

#include <types.h>

/*
 * Common structures for ACRN/VHM/DM
 */

/*
 * IO request
 */
#define VHM_REQUEST_MAX 16

#define REQ_STATE_PENDING	0
#define REQ_STATE_SUCCESS	1
#define REQ_STATE_PROCESSING	2
#define REQ_STATE_FAILED	-1

#define REQ_PORTIO	0
#define REQ_MMIO	1
#define REQ_PCICFG	2
#define REQ_WP		3

#define REQUEST_READ	0
#define REQUEST_WRITE	1

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

struct mmio_request {
	uint32_t direction;
	uint32_t reserved;
	int64_t address;
	int64_t size;
	int64_t value;
} __aligned(8);

struct pio_request {
	uint32_t direction;
	uint32_t reserved;
	int64_t address;
	int64_t size;
	int32_t value;
} __aligned(8);

struct pci_request {
	uint32_t direction;
	uint32_t reserved[3];/* need keep same header fields with pio_request */
	int64_t size;
	int32_t value;
	int32_t bus;
	int32_t dev;
	int32_t func;
	int32_t reg;
} __aligned(8);

/* vhm_request are 256Bytes aligned */
struct vhm_request {
	/* offset: 0bytes - 63bytes */
	union {
		uint32_t type;
		int32_t reserved0[16];
	};
	/* offset: 64bytes-127bytes */
	union {
		struct pio_request pio_request;
		struct pci_request pci_request;
		struct mmio_request mmio_request;
		int64_t reserved1[8];
	} reqs;

	/* True: valid req which need VHM to process.
	 * ACRN write, VHM read only
	 **/
	int32_t valid;

	/* the client which is distributed to handle this request */
	int32_t client;

	/* 1: VHM had processed and success
	 *  0: VHM had not yet processed
	 * -1: VHM failed to process. Invalid request
	 * VHM write, ACRN read only
	 */
	int32_t processed;
} __aligned(256);

struct vhm_request_buffer {
	union {
		struct vhm_request req_queue[VHM_REQUEST_MAX];
		int8_t reserved[4096];
	};
} __aligned(4096);

/**
 * @brief Info to create a VM, the parameter for HC_CREATE_VM hypercall
 */
struct acrn_create_vm {
	/** created vmid return to VHM. Keep it first field */
	int32_t vmid;

	/** VCPU numbers this VM want to create */
	uint32_t vcpu_num;

	/** the GUID of this VM */
	uint8_t	 GUID[16];

	/** whether Secure World is enabled for this VM */
	uint8_t	 secure_world_enabled;

	/** Reserved for future use*/
	uint8_t  reserved[31];
} __aligned(8);

/**
 * @brief Info to create a VCPU
 *
 * the parameter for HC_CREATE_VCPU hypercall
 */
struct acrn_create_vcpu {
	/** the virtual CPU ID for the VCPU created */
	uint32_t vcpu_id;

	/** the physical CPU ID for the VCPU created */
	uint32_t pcpu_id;
} __aligned(8);

/**
 * @brief Info to set ioreq buffer for a created VM
 *
 * the parameter for HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	/** guest physical address of VM request_buffer */
	uint64_t req_buf;
} __aligned(8);

/** Interrupt type for acrn_irqline: inject interrupt to IOAPIC */
#define	ACRN_INTR_TYPE_ISA	0

/** Interrupt type for acrn_irqline: inject interrupt to both PIC and IOAPIC */
#define	ACRN_INTR_TYPE_IOAPIC	1

/**
 * @brief Info to assert/deassert/pulse a virtual IRQ line for a VM
 *
 * the parameter for HC_ASSERT_IRQLINE/HC_DEASSERT_IRQLINE/HC_PULSE_IRQLINE
 * hypercall
 */
struct acrn_irqline {
	/** interrupt type which could be IOAPIC or ISA */
	uint32_t intr_type;

	/** reserved for alignment padding */
	uint32_t reserved;

	/** pic IRQ for ISA type */
	uint64_t pic_irq;

	/** ioapic IRQ for IOAPIC & ISA TYPE,
	 *  if -1 then this IRQ will not be injected
	 */
	uint64_t ioapic_irq;
} __aligned(8);

/**
 * @brief Info to inject a MSI interrupt to VM
 *
 * the parameter for HC_INJECT_MSI hypercall
 */
struct acrn_msi_entry {
	/** MSI addr[19:12] with dest VCPU ID */
	uint64_t msi_addr;

	/** MSI data[7:0] with vector */
	uint64_t msi_data;
} __aligned(8);

/**
 * @brief Info to inject a NMI interrupt for a VM
 */
struct acrn_nmi_entry {
	/** virtual CPU ID to inject */
	int64_t vcpu_id;
} __aligned(8);

/**
 * @brief Info to remap pass-through PCI MSI for a VM
 *
 * the parameter for HC_VM_PCI_MSIX_REMAP hypercall
 */
struct acrn_vm_pci_msix_remap {
	/** pass-through PCI device virtual BDF# */
	uint16_t virt_bdf;

	/** pass-through PCI device physical BDF# */
	uint16_t phys_bdf;

	/** pass-through PCI device MSI/MSI-X cap control data */
	uint16_t msi_ctl;

	/** reserved for alignment padding */
	uint16_t reserved;

	/** pass-through PCI device MSI address to remap, which will
	 * return the caller after remapping
	 */
	uint64_t msi_addr;		/* IN/OUT: msi address to fix */

	/** pass-through PCI device MSI data to remap, which will
	 * return the caller after remapping
	 */
	uint32_t msi_data;

	/** pass-through PCI device is MSI or MSI-X
	 *  0 - MSI, 1 - MSI-X
	 */
	int32_t msix;

	/** if the pass-through PCI device is MSI-X, this field contains
	 *  the MSI-X entry table index
	 */
	int32_t msix_entry_index;

	/** if the pass-through PCI device is MSI-X, this field contains
	 *  Vector Control for MSI-X Entry, field defined in MSI-X spec
	 */
	uint32_t vector_ctl;
} __aligned(8);

/**
 * @brief The guest config pointer offset.
 *
 * It's designed to support passing DM config data pointer, based on it,
 * hypervisor would parse then pass DM defined configuration to GUEST VCPU
 * when booting guest VM.
 * the address 0xd0000 here is designed by DM, as it arranged all memory
 * layout below 1M, DM should make sure there is no overlap for the address
 * 0xd0000 usage.
 */
#define GUEST_CFG_OFFSET	0xd0000

/**
 * @}
 */
#endif /* ACRN_COMMON_H */
