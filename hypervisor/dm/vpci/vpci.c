/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <vm.h>
#include <vtd.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief {TBD brief description}
 *
 * {TBD detailed description, including purposes, designed usages, usage remarks and dependency justification}
 */

#define PCI_USB_CONTROLLER 0x0CU
#define PCI_ETH_CONTROLLER 0x02U

struct cap_info {
	uint8_t id;
	uint8_t length;
};

struct unused_fields {
	uint8_t offset;
	uint8_t length;
};

static void vpci_init_vdevs(struct acrn_vm *vm);
static void deinit_prelaunched_vm_vpci(struct acrn_vm *vm);
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static void pci_cfgaddr_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	uint32_t val = ~0U;
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		val = cfg_addr->value;
	}

	pio_req->value = val;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static void pci_cfgaddr_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/* unmask reserved fields: BITs 24-30 and BITs 0-1 */
		cfg_addr->value = val & (~0x7f000003U);
	}
}

static inline bool vpci_is_valid_access_offset(uint32_t offset, uint32_t bytes)
{
	return ((offset & (bytes - 1U)) == 0U);
}

static inline bool vpci_is_valid_access_byte(uint32_t bytes)
{
	return ((bytes == 1U) || (bytes == 2U) || (bytes == 4U));
}

static inline bool vpci_is_valid_access(uint32_t offset, uint32_t bytes)
{
	return (vpci_is_valid_access_byte(bytes) && vpci_is_valid_access_offset(offset, bytes));
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SOS_VM)
 */
static void pci_cfgdata_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint32_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		uint32_t target_reg = cfg_addr.bits.reg_num + offset;
		if (vpci_is_valid_access(target_reg, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			read_cfg(vpci, bdf, cfg_addr.bits.reg_num + offset, bytes, &val);
		}
	}

	pio_req->value = val;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SOS_VM)
 */
static void pci_cfgdata_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint32_t offset = addr - PCI_CONFIG_DATA;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		uint32_t target_reg = cfg_addr.bits.reg_num + offset;
		if (vpci_is_valid_access(target_reg, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			write_cfg(vpci, bdf, target_reg, bytes, val);

		}
	}

}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_init(struct acrn_vm *vm)
{
	struct vm_io_range pci_cfgaddr_range = { .base = PCI_CONFIG_ADDR, .len = 1U };

	struct vm_io_range pci_cfgdata_range = { .base = PCI_CONFIG_DATA, .len = 4U };

	vm->vpci.vm = vm;
	vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	/* Build up vdev list for vm */
	vpci_init_vdevs(vm);

	/*
	 * SOS: intercept port CF8 only.
	 * UOS or pre-launched VM: register handler for CF8 only and I/O requests to CF9/CFA/CFB are
	 * not handled by vpci.
	 */
	register_pio_emulation_handler(
		vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range, pci_cfgaddr_io_read, pci_cfgaddr_io_write);

	/* Intercept and handle I/O ports CFC -- CFF */
	register_pio_emulation_handler(
		vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range, pci_cfgdata_io_read, pci_cfgdata_io_write);

	spinlock_init(&vm->vpci.lock);
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_cleanup(struct acrn_vm *vm)
{
	/* deinit function for both SOS and pre-launched VMs (consider sos also as pre-launched) */
	deinit_prelaunched_vm_vpci(vm);
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 */
static void assign_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;

	ret = add_iommu_device(vm->iommu, (uint8_t)vdev->pbdf.bits.b, (uint8_t)(vdev->pbdf.value & 0xFFU));
	if (ret != 0) {
		panic("failed to assign iommu device!");
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 */
static void remove_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;

	ret = remove_iommu_device(vm->iommu, (uint8_t)vdev->pbdf.bits.b, (uint8_t)(vdev->pbdf.value & 0xFFU));
	if (ret != 0) {
		/*
		 *TODO
		 * panic needs to be removed here
		 * Currently unassign_pt_device can fail for multiple reasons
		 * Once all the reasons and methods to avoid them can be made sure
		 * panic here is not necessary.
		 */
		panic("failed to unassign iommu device!");
	}
}

/* read  some specific fields from physical configure space */
static void init_default_cfg(struct pci_vdev *vdev)
{
	const struct cap_info caps[2] = { {0x01U, 8U}, {0x13U, 4U} };
	const struct unused_fields eth_unused[] = { {0x90U, 16U} };
	const struct unused_fields usb_unused[] = { {0x9CU, 4U}, {0xA0U, 2U}, {0xA8U, 8U}, {0xf8U, 4U} };

	uint8_t offset, index;
	uint8_t max_index = (PCI_REGMAX + 1U) >> 2U;
	uint32_t val;

	for (index = 0U; index < max_index; index++) {
		/* read default physical configure space values except BARs */
		if ((index < (PCIR_BARS/4U)) || (index > (PCIR_BARS/4U + PCI_BAR_COUNT - 1U))) {
			offset = (uint8_t)(index << 2U);
			val = pci_pdev_read_cfg(vdev->pbdf, offset, 4U);
			pci_vdev_write_cfg(vdev, offset, 4U, val);
		}
	}

	/* just handle two PCI devices: USB/network controller, hide unused fields */
	val = pci_vdev_read_cfg(vdev, PCIR_CLASS, 1U);
	if (val == PCI_USB_CONTROLLER) {
		for (index = 0U; index < ARRAY_SIZE(usb_unused); index++) {
			memset(vdev->cfgdata.data_8 + usb_unused[index].offset, 0U, usb_unused[index].length);
		}
	} else if (val == PCI_ETH_CONTROLLER) {
		for (index = 0U; index < ARRAY_SIZE(eth_unused); index++) {
			memset(vdev->cfgdata.data_8 + eth_unused[index].offset, 0U, eth_unused[index].length);
		}
	}

	/* hide other CAPs except MSI, just for USB/ETH controller */
	offset = pci_vdev_read_cfg(vdev, PCIR_CAP_PTR, 1U);
	while ((offset != 0U) && (offset != 0xFFU)) {
		uint8_t cap = pci_vdev_read_cfg(vdev, offset + PCICAP_ID, 1U);
		uint8_t next = pci_vdev_read_cfg(vdev, offset + PCICAP_NEXTPTR, 1U);

		/* just expose MSI */
		if (cap == PCIY_MSI) {
			pci_vdev_write_cfg(vdev, PCIR_CAP_PTR, 1U, offset);
		} else {
			/* hide other CAPs, set all its fields 0U */
			if (cap == caps[0].id) {
				index = 0U;
			} else if (cap == caps[1].id) {
				index = 1U;
			} else {
				pr_fatal("CAP: %d, not handled, please check!\n", cap);
			}

			memset(vdev->cfgdata.data_8 + offset, 0U, caps[index].length);
		}

		offset = next;
	}

	/* write 0U to MSI-CAP-->next CAP, hide others */
	offset = pci_vdev_read_cfg(vdev, PCIR_CAP_PTR, 1U);
	pci_vdev_write_cfg(vdev, offset + PCICAP_NEXTPTR, 1U, 0U);

}

static void vpci_init_pt_dev(struct pci_vdev *vdev)
{
	init_default_cfg(vdev);

    /*
	 * Here init_vdev_pt() needs to be called after init_vmsix() for the following reason:
	 * init_vdev_pt() will indirectly call has_msix_cap(), which
	 * requires init_vmsix() to be called first.
	 */
	init_vmsi(vdev);
	init_vdev_pt(vdev);

	assign_vdev_pt_iommu_domain(vdev);
}

static void vpci_deinit_pt_dev(struct pci_vdev *vdev)
{
	remove_vdev_pt_iommu_domain(vdev);
	deinit_vmsi(vdev);
}

static inline uint32_t pci_bar_index(uint32_t offset)
{
	return (offset - PCIR_BARS) >> 2U;
}

static int32_t vpci_write_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	if (vbar_access(vdev, offset)) {
		/* bar write access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			vdev_pt_write_vbar(vdev, pci_bar_index(offset), val);
		}
	} else if (msicap_access(vdev, offset)) {
		vmsi_write_cfg(vdev, offset, bytes, val);
	} else if (offset == PCIR_COMMAND) {
		/* for command register write to physical space, but INTx is disabled by default */
		pci_pdev_write_cfg(vdev->pbdf, offset, bytes, val | 0x400U);
	} else {
		/* ignore other writing */
		pr_dbg("pci write: bdf=%d, offset=0x%x, val=0x%x, bytes=%d\n",
			vdev->pbdf, offset, val, bytes);
	}

	return 0;
}

static int32_t vpci_read_pt_dev_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	if (vbar_access(vdev, offset)) {
		/* bar access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			*val = pci_vdev_read_bar(vdev, pci_bar_index(offset));
		} else {
			*val = ~0U;
		}
	} else if ((offset >= PCIR_COMMAND) && (offset < PCIR_REVID)) {
		/* for command / status registers read from physical space */
		*val = pci_pdev_read_cfg(vdev->pbdf, offset, bytes);
	} else {
		/* others read from virtual space */
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
	}

	return 0;
}

static const struct pci_vdev_ops pci_pt_dev_ops = {
	.init_vdev = vpci_init_pt_dev,
	.deinit_vdev = vpci_deinit_pt_dev,
	.write_vdev_cfg = vpci_write_pt_dev_cfg,
	.read_vdev_cfg = vpci_read_pt_dev_cfg,
};

/**
 * @pre vpci != NULL
 */
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = pci_find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	}
	spinlock_release(&vpci->lock);
}

/**
 * @pre vpci != NULL
 */
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = pci_find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	}
	spinlock_release(&vpci->lock);
}

/**
 * @pre vpci != NULL
 * @pre vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config)
{
	struct pci_vdev *vdev = &vpci->pci_vdevs[vpci->pci_vdev_cnt];

	vpci->pci_vdev_cnt++;
	vdev->vpci = vpci;
	vdev->bdf.value = dev_config->vbdf.value;
	vdev->pbdf = dev_config->pbdf;
	vdev->pci_dev_config = dev_config;

	if (dev_config->vdev_ops != NULL) {
		vdev->vdev_ops = dev_config->vdev_ops;
	} else {
		vdev->vdev_ops = &pci_pt_dev_ops;
		ASSERT(dev_config->emu_type == PCI_DEV_TYPE_PTDEV,
			"Only PCI_DEV_TYPE_PTDEV could not configure vdev_ops");
		ASSERT(dev_config->pdev != NULL, "PCI PTDev is not present on platform!");
	}

	vdev->vdev_ops->init_vdev(vdev);
}

/**
 * @pre vm != NULL
 */
static void vpci_init_vdevs(struct acrn_vm *vm)
{
	uint32_t idx;
	struct acrn_vpci *vpci = &(vm->vpci);
	const struct acrn_vm_config *vm_config = get_vm_config(vpci->vm->vm_id);

	for (idx = 0U; idx < vm_config->pci_dev_num; idx++) {
		vpci_init_vdev(vpci, &vm_config->pci_devs[idx]);
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 * @pre is_sos_vm(vm) || is_prelaunched_vm(vm)
 */
static void deinit_prelaunched_vm_vpci(struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i;

	for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *)&(vm->vpci.pci_vdevs[i]);

		vdev->vdev_ops->deinit_vdev(vdev);
	}
}

/**
 * @}
 */
