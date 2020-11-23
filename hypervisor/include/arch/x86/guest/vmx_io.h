/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_EMUL_H
#define IO_EMUL_H

/**
 * @addtogroup vp-dm_io-req
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the external MACROs and APIs to handle VM exit
 * on I/O instruction and EPT violation.
 */

#include <types.h>
#include <vcpu.h>

/**
* @brief  Index to the port I/O handler descriptor for the registers of the
*	  master PIC in the port I/O handler array of VMs.  Reserved for future use.
*
*/
#define PIC_MASTER_PIO_IDX  0U

/**
 * @brief Index to the port I/O handler descriptor for the registers of the slave
 *	  PIC in the port I/O handler array of VMs. Reserved for future use.
 */
#define PIC_SLAVE_PIO_IDX   (PIC_MASTER_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the PIC ELC register in the
 *	  port I/O handler array of VMs.  Reserved for future use.
 */
#define PIC_ELC_PIO_IDX     (PIC_SLAVE_PIO_IDX + 1U)

/**
 * @brief  Index to the port I/O handler descriptor for the PCI configuration
 *	   address register in the port I/O handler array of VMs.
 */
#define PCI_CFGADDR_PIO_IDX (PIC_ELC_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the PCI configuration
 *	  address register in the port I/O handler array of VMs.
 */
#define PCI_CFGDATA_PIO_IDX (PCI_CFGADDR_PIO_IDX + 1U)
/**
 * When MAX_VUART_NUM_PER_VM is larger than 2, UART_PIO_IDXn should also be added here
 */

/**
 * @brief Index to the port I/O handler descriptor for the registers of
 *	  the first UART in the port I/O handler array of VMs. Reserved for future use.
 */
#define UART_PIO_IDX0            (PCI_CFGDATA_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the registers of the second UART
 *	  in the port I/O handler array of VMs. Reserved for future use.
 */
#define UART_PIO_IDX1            (UART_PIO_IDX0 + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the ACPI PM1a event registers
 *	  in the port I/O handler array of VMs. Reserved for future use.
 */
#define PM1A_EVT_PIO_IDX         (UART_PIO_IDX1 + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the ACPI PM1b control registers
 *	  in the port I/O handler array of VMs. Reserved for future use.
 */
#define PM1A_CNT_PIO_IDX         (PM1A_EVT_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the ACPI PM1b event registers
 *	  in the port I/O handler array of VMs. Reserved for future use.
 */
#define PM1B_EVT_PIO_IDX         (PM1A_CNT_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the ACPI PM1b control registers
 *	   in the port I/O handler array of VMs. Reserved for future use.
 */
#define PM1B_CNT_PIO_IDX         (PM1B_EVT_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the registers of RTC in the
 *	  port I/O handler array of VMs.
 */
#define RTC_PIO_IDX              (PM1B_CNT_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the virtual ACPI PM1a control
 *	  registers in the port I/O handler array of VMs. Reserved for future use.
 */
#define VIRTUAL_PM1A_CNT_PIO_IDX (RTC_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the keyboard control/status
 *	  register in the port I/O handler array of VMs. Reserved for future use.
 */
#define KB_PIO_IDX               (VIRTUAL_PM1A_CNT_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for I/O port 0xCF9 in the port
 *	  I/O handler array of VMs. For test use only.
 */
#define CF9_PIO_IDX              (KB_PIO_IDX + 1U)

/**
 * @brief Index to the port I/O handler descriptor for the reset register in the
 *	  port I/O handler array of VMs. Reserved for future use.
 */
#define PIO_RESET_REG_IDX        (CF9_PIO_IDX + 1U)
/**
 * @brief Size of the port I/O handler array of VMs
 */
#define EMUL_PIO_IDX_MAX         (PIO_RESET_REG_IDX + 1U)

/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param [in] vcpu Pointer to the instance of struct acrn_vcpu, which triggers
 *		    the VM exit on I/O instruction.
 *
 * @return 0, Emulation for I/O instruction shall be always handled successfully.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu is
 *		 different among parallel invocations.
 *
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @brief The handler of VM exits on EPT violation
 *
 * @param [in] vcpu Pointer to the instance of struct acrn_vcpu,
 *		    which triggers the VM exit on EPT violation.
 *
 * @return 0, EPT violation shall always be handled successfully.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy unspecified
 *
 * @threadsafety This function is thread-safe under condition that /p vcpu is
 *		 different among parallel invocations.
 *
 */
int32_t ept_violation_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @}
 */

#endif /* IO_EMUL_H */
