/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMEXIT_H_
#define VMEXIT_H_

#include <vcpu.h>

/**
 * @addtogroup vp-base_hv_main
 *
 * @{
 */

/**
 * @file
 * @brief brief this file declares the data struct, static inline functions and external APIs
 * of the VM exit handling.
 *
 */

#include <types.h>
#include <vcpu.h>

/**
 * @brief This structure is used to store vm exit handler and qualification information.
 *
 * @consistency None
 *
 * @alignment 8
 *
 * @remark None
 */
struct vm_exit_dispatch {
	/**
	 * @brief A function pointer to handle the specified VM exit.
	 *
	 */
	int32_t (*handler)(struct acrn_vcpu *vcpu);
	/**
	 * @brief Indicating whether the exit qualification information is necessary.
	 *
	 */
	uint32_t need_exit_qualification;
};

int32_t vmexit_handler(struct acrn_vcpu *vcpu);
int32_t cpuid_vmexit_handler(struct acrn_vcpu *vcpu);

extern void vm_exit(void);

/**
 * @brief This function intercepts bit[msb:lsb] of the exit_qual.
 *
 * @param[in] exit_qual The VM exit qualification.
 * @param[in] msb The most significant intercepted bit.
 * @param[in] lsb The least significant intercepted bit.
 *
 * @return A 64 bit value whose bits [msb:lsb] is equal to bits [msb:lsb]
 * of exit_qual and all the other bits are 0.
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_qualification_bit_mask(uint64_t exit_qual, uint32_t msb, uint32_t lsb)
{
	/** Return 'bit[msb:lsb] of \a exit_qual' << lsb */
	return (exit_qual & (((1UL << (msb + 1U)) - 1UL) - ((1UL << lsb) - 1UL)));
}

/**
 * @brief This function returns index of control register when vmexit happens.
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit[3:0](number of control register) of the exit_qual
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_cr_access_cr_num(uint64_t exit_qual)
{
	/** Return bit[3:0](number of control register) of the exit_qual for CR access */
	return (vm_exit_qualification_bit_mask(exit_qual, 3U, 0U) >> 0U);
}

/**
 * @brief This function returns access type of control register access when vm exit happens
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit[5:4](access type) of the exit_qual
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_cr_access_type(uint64_t exit_qual)
{
	/** Return bit[5:4](access type) of the exit_qual for CR access*/
	return (vm_exit_qualification_bit_mask(exit_qual, 5U, 4U) >> 4U);
}

/**
 * @brief This function returns index of general purpose register when vm exit happened.
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit[11:8](the general purpose register index) of the exit_qual
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_cr_access_reg_idx(uint64_t exit_qual)
{
	/** Return bit[11:8](the general purpose register index) of the exit_qual for CR access*/
	return (vm_exit_qualification_bit_mask(exit_qual, 11U, 8U) >> 8U);
}

/**
 * @brief This function returns size of attempted access of IO instruction when vm exit happens.
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit[2:0](the size of access) of the exit_qual
 *
 * @pre  exit_qual&7H == 0 || exit_qual&7H == 1 || exit_qual&7H == 3
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_io_instruction_size(uint64_t exit_qual)
{
	/** Return bit[2:0](the size of access) of the exit_qual for I/O instruction */
	return (vm_exit_qualification_bit_mask(exit_qual, 2U, 0U) >> 0U);
}

/**
 * @brief This function returns direction of attempted access of IO instruction when vm exit happens.
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit3(the direction of access) of the exit_qual
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_io_instruction_access_direction(uint64_t exit_qual)
{
	/** Return bit3(the direction of access) of the exit_qual for I/O instruction */
	return (vm_exit_qualification_bit_mask(exit_qual, 3U, 3U) >> 3U);
}

/**
 * @brief This function returns port number of attempted access of IO instruction when vm exit happens.
 *
 * @param[in] exit_qual The VM exit qualification.
 *
 * @return bit[31:16](port number) of the exit_qual
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @threadsafety Yes
 */
static inline uint64_t vm_exit_io_instruction_port_number(uint64_t exit_qual)
{
	/** Return bit[31:16](port number) of the exit_qual for I/O instruction */
	return (vm_exit_qualification_bit_mask(exit_qual, 31U, 16U) >> 16U);
}

/**
 * @}
 */

#endif /* VMEXIT_H_ */
