/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUID_H_
#define CPUID_H_

/**
 * @addtogroup hwmgmt_cpu
 *
 * @{
 */

/**
 * @file
 * @brief This file provides the information to be used for cpuid operations.
 *
  * Following functionalities are provided in this file:
 *
 * 1.Define the macros related to CPUID.
 * 2.Define CPUID APIs to execute cpuid instructions.
 *
 * This file is decomposed into the following functions:
 *
 * cpuid(leaf, eax, ebx, ecx, edx)			- Execute a CPUID instruction with specified \a leaf.
 * cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx)	- Execute a CPUID instruction with specified \a leaf and
 *                                                        \a subleaf.
 */

#include <types.h>

/**
 * @brief A bit representing whether the processor supports SSE3.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[0]
 */
#define CPUID_ECX_SSE3			(1U << 0U)
/**
 * @brief A bit representing whether the processor supports DS area using 64-bit layout.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[2]
 */
#define CPUID_ECX_DTES64		(1U << 2U)
/**
 * @brief A bit representing whether the processor supports MONITOR/MWAIT.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[3]
 */
#define CPUID_ECX_MONITOR		(1U << 3U)
/**
 * @brief A bit representing whether the processor supports the extensions to the Debug Store
 * feature to allow for branch message storage qualified by CPL.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[4]
 */
#define CPUID_ECX_DS_CPL		(1U << 4U)
/**
 * @brief A bit representing whether the processor supports Virtual Machine Extensions..
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[5]
 */
#define CPUID_ECX_VMX			(1U << 5U)
/**
 * @brief A bit representing whether the processor supports Safer Mode Extensions.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[6]
 */
#define CPUID_ECX_SMX			(1U << 6U)
/**
 * @brief A bit representing whether the processor supports Enhanced Intel SpeedStep technology.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[7].
 */
#define CPUID_ECX_EST			(1U << 7U)
/**
 * @brief A bit representing whether the processor supports Thermal Monitor 2.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[8].
 */
#define CPUID_ECX_TM2			(1U << 8U)
/**
 * @brief A bit representing whether the supports IA32_DEBUG_INTERFACE MSR for silicon debug.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[11].
 */
#define CPUID_ECX_SDBG			(1U << 11U)
/**
 * @brief A bit representing whether the processor supports Perfmon and Debug Capability.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[15].
 */
#define CPUID_ECX_PDCM			(1U << 15U)
/**
 * @brief A bit representing whether the processor supports XSAVE instructions and XSAVE supported features.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[26].
 */
#define CPUID_ECX_XSAVE			(1U << 26U)
/**
 * @brief A bit representing whether the CR4.OSXSAVE is set.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[27].
 *
 * A value of 1 indicates that the OS has set CR4.OSXSAVE[bit 18] to enable XSETBV/XGETBV instructions
 * to access XCR0 and to support processor extended state management using XSAVE/XRSTOR.
 */
#define CPUID_ECX_OSXSAVE		(1U << 27U)
/**
 * @brief A bit representing whether the processor supports Virtual 8086 Mode Enhancements.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[1].
 */
#define CPUID_EDX_VME			(1U << 1U)
/**
 * @brief A bit representing whether the processor supports Debugging Extensions.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[2].
 */
#define CPUID_EDX_DE			(1U << 2U)
/**
 * @brief A bit representing whether the processor supports Machine Check Exception.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[7].
 */
#define CPUID_EDX_MCE			(1U << 7U)
/**
 * @brief A bit representing whether the processor supports Memory Type Range Registers.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[12].
 */
#define CPUID_EDX_MTRR			(1U << 12U)
/**
 * @brief A bit representing whether the processor supports Machine Check Architecture.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[14].
 */
#define CPUID_EDX_MCA			(1U << 14U)
/**
 * @brief A bit representing whether the processor supports Debug Store.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[21].
 */
#define CPUID_EDX_DTES			(1U << 21U)
/**
 * @brief A bit representing whether the processor supports Thermal Monitor and Software Controlled Clock Facilities.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[22].
 */
#define CPUID_EDX_ACPI			(1U << 22U)
/**
 * @brief A bit representing whether Max APIC IDs reserved field is valid.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[28].
 *
 * A value of 0 for HTT indicate there is only a single logic processor in the package and software should assume only
 * a single APIC ID is reserved. A value of 1 for HTT indicates the value in CPUID.1.EBX[23:16] is valid for the
 * package.
 */
#define CPUID_EDX_HTT			(1U << 28U)
/**
 * @brief A bit representing whether the processor supports Thermal Monitor.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[29].
 */
#define CPUID_EDX_TM1			(1U << 29U)
/**
 * @brief A bit representing whether the processor supports Pending Break Enable.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):EDX[31].
 */
#define CPUID_EDX_PBE			(1U << 31U)
/**
 * @brief A bit representing whether the processor supports intel SGX Extensions.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[2].
 */
#define CPUID_EBX_SGX			(1U << 2U)
/**
 * @brief A bit representing whether the processor supports intel Memory Protection Extensions.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[14].
 */
#define CPUID_EBX_MPX			(1U << 14U)
/**
 * @brief A bit representing whether the processor supports SGX Launch Configuration.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):ECX[30].
 */
#define CPUID_ECX_SGX_LC		(1U << 30U)
/**
 * @brief A bit representing whether the processor supports INVPCID instructions.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[10].
 */
#define CPUID_EBX_INVPCID		(1U << 10U)
/**
 * @brief A bit representing whether the processor supports Intel Resource Director Technology Monitoring.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[12].
 */
#define CPUID_EBX_PQM			(1U << 12U)
/**
 * @brief A bit representing whether the processor supports Intel Resource Director Technology Allocation.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[15].
 */
#define CPUID_EBX_PQE			(1U << 15U)
/**
 * @brief A bit representing whether the processor supports Intel Processor Trace.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EBX[25].
 */
#define CPUID_EBX_PROC_TRC		(1U << 25U)
/**
 * @brief A bit representing whether the MSR TSX_FORCE_ABORT exists.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EDX[13].
 */
#define CPUID_EDX_TSX_FORCE_ABORT	(1U << 13U)
/**
 * @brief A bit representing whether the processor supports single thread indirect branch predictors.
 *
 * This bit is fetched from CPUID(EAX=7H,ECX=0H):EDX[27].
 */
#define CPUID_EDX_STIBP			(1U << 27U)
/**
 * @brief A bit representing whether the processor supports PCIDs.
 *
 * This bit is fetched from CPUID(EAX=1H,ECX=0H):ECX[17].
 */
#define CPUID_ECX_PCID			(1U << 17U)
/**
 * @brief A bit representing whether the processor supports XCR0.BNDREGS[3].
 *
 * This bit is fetched from CPUID(EAX=DH,ECX=0H):EAX[3].
 **/
#define CPUID_EAX_XCR0_BNDREGS		(1U << 3U)
/**
 * @brief A bit representing whether the processor supports XCR0.BNDCSR[4].
 *
 * This bit is fetched from CPUID(EAX=DH,ECX=0H):EAX[4].
 **/
#define CPUID_EAX_XCR0_BNDCSR		(1U << 4U)
/**
 * @brief A bit representing whether the processor supports Execute Disable Bit feature.
 *
 * This bit is fetched from CPUID(EAX=80000001H,ECX=0H):EDX[20].
 *
 * If the MSR IA32_MISC_ENABLE[34] bit is set, the bit of CPUID(EAX=80000001H,ECX=0H):EDX[20] will be 0.
 **/
#define CPUID_EDX_XD_BIT_AVIL		(1U << 20U)

/**
 * @brief A bit representing whether the processor supports XSAVES/XRSTORS and IA32_XSS.
 *
 * This bit is fetched from CPUID(EAX=DH,ECX=1H):EAX[3].
 **/
#define CPUID_EAX_XSAVES		(1U << 3U)
/**
 * @brief A bit representing whether the processor supports PT state.
 *
 * This bit is fetched from CPUID(EAX=DH,ECX=1H):ECX[8].
 **/
#define CPUID_ECX_PT_STATE		(1U << 8U)

/**
 * @brief A bit representing whether the processor supports APIC-Timer-always-running feature.
 *
 * This bit is fetched from CPUID(EAX=6H,ECX=0H):EAX[2].
 **/
#define CPUID_EAX_ARAT			(1U << 2U)

/**
 * @brief A bit position of L2 Associativity field.
 *
 * The L2 Associativity field is fetched from CPUID(EAX=80000006H,ECX=0H):ECX[12:15].
 **/
#define CPUID_ECX_L2_ASSOCIATIVITY_FIELD_POS	12U
/**
 * @brief A bit mask of L2 Associativity field.
 *
 * The L2 Associativity field is fetched from CPUID(EAX=80000006H,ECX=0H):ECX[12:15].
 **/
#define CPUID_ECX_L2_ASSOCIATIVITY_FIELD_MASK	(0xFU << CPUID_ECX_L2_ASSOCIATIVITY_FIELD_POS)

/**
 * @brief A CPUID entry provide vendor string.
 *
 * CPUID(EAX=0H,ECX=0H) will set following registers list below:
 *
 * - EAX: Maximum Input Value for Basic CPUID Information
 * - EBX: "Genu"
 * - ECX: "ntel"
 * - EDX: "inel"
 **/
#define CPUID_VENDORSTRING		0U
/**
 * @brief A CPUID entry provide features.
 *
 *
 * CPUID(EAX=1H,ECX=0H) will set following registers list below:
 *
 * - EAX: Version INformation: Type, Family, Model, Stepping ID.
 * - EBX, ECX and EDX provided features and capability.
 **/
#define CPUID_FEATURES			1U
/**
 * @brief A CPUID entry provide structured extended features.
 *
 * CPUID(EAX=7H,ECX=0H) will set following registers list below:
 *
 * - EAX: Report the maximum input value for supported leaf 7 sub-leaves.
 * - EBX, ECX, EDX provided structured extended features and capability.
 **/
#define CPUID_EXTEND_FEATURE		7U
/**
 * @brief A CPUID entry of the processor extended state enumeration.
 **/
#define CPUID_XSAVE_FEATURES		0xDU
/**
 * @brief A CPUID entry that used to get extended function.
 *
 * CPUID(EAX=80000000H,ECX=0H) will set following registers list below:
 *
 * - EAX: Maximum input value for extended function CPUID Information.
 * - EBX, ECX and EDX:Reserved.
 **/
#define CPUID_MAX_EXTENDED_FUNCTION	0x80000000U
/**
 * @brief A CPUID entry of extended function 1.
 **/
#define CPUID_EXTEND_FUNCTION_1		0x80000001U
/**
 * @brief A CPUID entry of extended function 2.
 **/
#define CPUID_EXTEND_FUNCTION_2		0x80000002U
/**
 * @brief A CPUID entry of extended function 3.
 **/
#define CPUID_EXTEND_FUNCTION_3		0x80000003U
/**
 * @brief A CPUID entry of extended function 4.
 **/
#define CPUID_EXTEND_FUNCTION_4		0x80000004U
/**
 * @brief A CPUID entry used to verify whether the processors supports Invariant TSC.
 **/
#define CPUID_EXTEND_INVA_TSC		0x80000007U
/**
 * @brief A CPUID entry used to fetch Linear/Physical Address size.
 **/
#define CPUID_EXTEND_ADDRESS_SIZE	0x80000008U

/**
 * @brief Execute a CPUID instruction with specified \a leaf.
 *
 * @param[in]     leaf The contents of EAX register upon execution of a CPUID instruction.
 * @param[out]  eax The pointer which points to an address that stores the contents of EAX register after execution
 * of a CPUID instruction.
 * @param[out]  ebx The pointer which points to an address that stores the contents of EBX register after execution
 * of a CPUID instruction.
 * @param[out]  ecx The pointer which points to an address that stores the contents of ECX register after execution
 * of a CPUID instruction.
 * @param[out]  edx The pointer which points to an address that stores the contents of EDX register after execution
 * of a CPUID instruction.
 *
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 * @post N/A
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety \a eax, \a ebx, \a ecx, \a edx are different among parallel invocation.
 */
static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/** Declare the following local variables of type uint32_t.
	 *  - subleaf representing the 'cpuid' instruction subleaf value, initialized as 0 */
	uint32_t subleaf = 0U;

	/**
	 * Execute cpuid instruction in order to fetch information.
	 * - Input operands: RAX holds \a leaf, RCX holds \a subleaf.
	 * - Output operands: The content of EAX register is stored to the memory area pointed by \a eax,
	 *                    The content of EBX register is stored to the memory area pointed by \a ebx,
	 *                    The content of ECX register is stored to the memory area pointed by \a ecx.
	 *                    The content of EDX register is stored to the memory area pointed by \a edx.
	 * - Clobbers: memory */
	asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf) : "memory");
}

/**
 * @brief Execute a CPUID instruction with specified \a leaf and \a subleaf.
 *
 * @param[in]     leaf The contents of EAX register upon execution of a CPUID instruction.
 * @param[in]     subleaf The contents of ECX register upon execution of a CPUID instruction.
 * @param[out]  eax The pointer which points to an address that stores the contents of EAX register after execution
 * of a CPUID instruction.
 * @param[out]  ebx The pointer which points to an address that stores the contents of EBX register after execution
 * of a CPUID instruction.
 * @param[out]  ecx The pointer which points to an address that stores the contents of ECX register after execution
 * of a CPUID instruction.
 * @param[out]  edx The pointer which points to an address that stores the contents of EDX register after execution
 * of a CPUID instruction.
 *
 * @pre eax != NULL
 * @pre ebx != NULL
 * @pre ecx != NULL
 * @pre edx != NULL
 * @post N/A
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety \a eax, \a ebx, \a ecx, \a edx are different among parallel invocation.
 */
static inline void cpuid_subleaf(
	uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/**
	 * Execute cpuid instruction in order to fetch information.
	 * - Input operands: RAX holds \a leaf, RCX holds \a subleaf.
	 * - Output operands: The content of EAX register is stored to the memory area pointed by \a eax,
	 *                    The content of EBX register is stored to the memory area pointed by \a ebx,
	 *                    The content of ECX register is stored to the memory area pointed by \a ecx.
	 *                    The content of EDX register is stored to the memory area pointed by \a edx.
	 * - Clobbers: memory */
	asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf) : "memory");
}

/**
 * @}
 */

#endif /* CPUID_H_ */
