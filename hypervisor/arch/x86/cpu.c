/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <bits.h>
#include <page.h>
#include <e820.h>
#include <mmu.h>
#include <vtd.h>
#include <lapic.h>
#include <per_cpu.h>
#include <cpufeatures.h>
#include <cpu_caps.h>
#include <acpi.h>
#include <ioapic.h>
#include <trampoline.h>
#include <cpuid.h>
#include <version.h>
#include <vmx.h>
#include <msr.h>
#include <ptdev.h>
#include <ld_sym.h>
#include <logmsg.h>
#include <uart16550.h>

/**
 * @defgroup hwmgmt_cpu hwmgmt.cpu
 * @ingroup hwmgmt
 * @brief Provide macros, structures, and APIs related to CPU.
 *
 * Following functionalities are provided in this module:
 *
 * 1.Provide the macros related to registers, CPU features, CPUIDs, segment descriptors, MSRs.
 * 2.Provide the enumeration type to identify the architecturally defined registers.
 * 3.Provide the structure to store the information of descriptor table.
 * 4.Provide the structure to hold TSS descriptor and GDT descriptor.
 * 5.Provide the structure to hold all per CPU information.
 * 6.Provide the APIs to do the operation list below:
 *
 *   - Read/Write MSR
 *   - Load/Store IDT
 *   - Load/Store GDT
 *   - Pause/Halt CPU
 *   - Disable/Enable interrupt
 *   - Execute CPUID instruction
 *   - Get per-CPU region data
 *   - Initialize the environment before start APs
 *   - Perform the physical CPU's initialization
 *
 * This module interacts with with the following modules:
 *
 * - boot
 *
 *   Use the boot module to get the delta between actual load host virtual address and CONFIG_HV_RAM_START.
 *
 *   All the APIs used in boot module listing below:
 *
 *       + get_hv_image_delta

 * - hwmgmt.acpi
 *
 *   Use the hwmgmt.acpi module to set physical local APIC IDs and return the number of physical local APICs on
 *   current platform.
 *
 *   All the APIs used in hwmgmt.acpi module listing below:
 *
 *       + parse_madt
 *
 * - hwmgmt.apic
 *
 *   Use the hwmgmt.apic module to do the following operations:
 *
 *       1. Enable x2APIC mode
 *       2. Mask all IOAPIC pins
 *       3. Sends a INIT-SIPI sequence to APs
 *       4. Send INIT IPI message to the specified CPU
 *
 *   All the APIs used in hwmgmt.apic module listing below:
 *
 *       + early_init_lapic
 *       + ioapic_setup_irqs
 *       + send_startup_ipi
 *       + send_single_init
 *
 * - hwmgmt.cpu_caps
 *
 *   Use the hwmgmt.cpu_caps module to do the following operations:
 *
 *       1. Initialize an internal data structure that stores the CPU information data.
 *       2. Initialize processor model name.
 *       3. Check whether MONITOR/MWAIT instructions are supported by CPU.
 *
 *   All the APIs used in hwmgmt.cpu_caps module listing below:
 *
 *       + init_pcpu_capabilities
 *       + init_pcpu_model_name
 *       + has_monitor_cap
 *
 * - hwmgmt.configs
 *
 *   Use the hwmgmt.configs module to do the following operations:
 *
 *       1. Initialize physical E820 entries in hypervisor.
 *       2. Allocate buffer for trampoline code.
 *
 *   All the APIs used in hwmgmt.configs module listing below:
 *
 *       + init_e820
 *       + e820_alloc_low_memory
 *
 * - hwmgmt.irq
 *
 *   Use the hwmgmt.irq module to initialize interrupt related resources.
 *
 *   All the APIs used in hwmgmt.irq module listing below:
 *
 *       + init_interrupt
 *
 * - hwmgmt.mmu
 *
 *   Use the hwmgmt.mmu module to do the following operations:
 *
 *       1.Enable paging.
 *       2.Enable supervisor mode access prevention on the current processor.
 *       3.Enable supervisor mode execution prevention on the current processor.
 *       4.Set up the MMU page table.
 *       5.Flush cache line.
 *
 *   All the APIs used in hwmgmt.mmu module listing below:
 *
 *       + enable_paging
 *       + enable_smap
 *       + enable_smep
 *       + init_paging
 *       + cache_flush_invalidate_all
 *       + clflush
 *
 * - hwmgmt.page
 *
 *   Use the hwmgmt.page module to get the host virtual address corresponding to the given host physical address.
 *
 *   All the APIs used in hwmgmt.page module listing below:
 *
 *       + hpa2hva
 *
 * - hwmgmt.schedule
 *
 *   Use the hwmgmt.schedule module to initialize/deinitialize scheduler control block of the specified CPU.
 *
 *   All the APIs used in hwmgmt.schedule module listing below:
 *
 *       + init_sched
 *       + deinit_sched
 *
 * - hwmgmt.security
 *
 *   Use the hwmgmt.security module to do the following operations:
 *
 *       1. Initialize the stack protector context.
 *       2. Check the security capability for physical platform.
 *
 *   All the APIs used in hwmgmt.security module listing below:
 *
 *       + set_fs_base
 *       + check_cpu_security_cap
 *
 * - hwmgmt.time
 *
 *   Use the hwmgmt.time module to do the following operations:
 *
 *       1. Get current "Time Stamp Counter"
 *       2. Get TSC frequency
 *       3. Delay a specific number of microseconds
 *
 *   All the APIs used in hwmgmt.time module listing below:
 *
 *       + rdtsc
 *       + calibrate_tsc
 *       + udelay
 *
 * - hwmgmt.vmx
 *
 *   Use the hwmgmt.vmx module to leave VMX operation on current logical processor.
 *
 *   All the APIs used in hwmgmt.vmx module listing below:
 *
 *   + vmx_off
 *
 * - hwmgmt.vtd
 *
 *   Use the hwmgmt.vtd module to initialize/enable IOMMU.
 *
 *   All the APIs used in hwmgmt.vtd module listing below:
 *
 *       + init_iommu
 *       + enable_iommu
 *
 * - lib.util
 *
 *   Use the lib.util module to do the memory operation.
 *
 *   All the APIs used in lib.util module listing below:
 *
 *       + memset
 *       + memcpy_s
 *
 * - lib.bits
 *
 *   Use the lib.bits module to do bitwise operation.
 *
 *   All the APIs used in lib.bits module listing below:
 *
 *       + ffs64
 *       + bitmap_clear_nolock
 *       + bitmap_set_lock
 *       + bitmap_test_and_clear_lock
 *
 * - debug
 *
 *   Use the debug module to do the following operations:
 *
 *       1. Initialize the uart16550 which is solely for debugging.
 *       2. Enter safety state.
 *       3. Log message in debug version.
 *       4. Check a given condition for debugging. Pause the current physical processor if that condition is false.
 *
 *   All the APIs used in debug module listing below:
 *
 *       + uart16550_init
 *       + panic
 *       + pr_acrnlog
 *       + pr_dbg
 *       + pr_fatal
 *       + ASSERT
 *       + pr_err
 *       + printf
 *
 * This module is decomposed into the following files:
 *
 * cpu.c          - Provide the information to be used for CPU operations.
 * cpu.h          - Define macros, data structures and APIs related to the register and the CPU status.
 * cpuid.h        - Provide the information to be used for CPUID instruction.
 * gdt.c          - Implement APIs to set TSS descriptor and load GTDR and TR.
 * trampoline.c   - Define variables and implement trampoline APIs to initialize the environment before start APs.
 * per_cpu.h      - Define structures and implement per-CPU APIs to hold per CPU information.
 * cpufeatures.h  - Define the macros related to CPU features.
 * gdt.h          - Define structures and macros, declare APIs to hold and maintain descriptors.
 * msr.h          - Define macros related to MSRs.
 * trampoline.h   - Declare trampoline APIs used to initialize the environment before start APs.
 * @{
 */

/**
 * @file
 * @brief This file provides the information to be used for CPU operations.
 *
 * 1.Define timeout related macros to limit the time to wait during start physical CPU or offline physical CPU.
 * 2.Define variables to provide the following data:
 *
 *     - Basic data of per CPU.
 *     - Shared data between CPU used as inter-process message.
 *     - Count of physical CPU.
 *     - Start address of APs.
 *
 * 3.Implementation of CPU APIS to do CPU operations list below:
 *     - Initialize per-CPU region data.
 *     - Startup all APs.
 *     - Manage CPU lifecycle.
 *     - Basic initialization before start vm.
 *
 * This file is decomposed into the following functions:
 *
 * init_percpu_lapic_id                    - Initialize lapic_id field of per_cpu_data.
 * pcpu_set_current_state(pcpu_id, state)  - Set the state of CPU to \a state.
 * get_pcpu_nums                           - The function returns the number of CPU cores detected by the hypervisor.
 * init_pcpu_pre                           - Perform the physical CPU's early stage initialization.
 * init_pcpu_post                          - Perform the physical CPU's late stage initialization.
 * get_pcpu_id_from_lapic_id(lapic_id)     - Get physical CPU id whose local APIC id is equal to \a lapic_id
 * start_pcpu(pcpu_id)                     - Start the physical CPU whose CPU id is \a pcpu_id.
 * start_pcpus                             - Start all cpus if the bit is set in mask except itself
 * make_pcpu_offline                       - Submit a request to offline the target CPU.
 * need_offline                            - Test and clear the NEED_OFFLINE bit of the given CPU.
 * is_any_pcpu_active                      - If there is any physical CPU still active.
 * wait_pcpus_offline                      - Wait for physical CPU offline with a timeout of 100ms.
 * cpu_do_idle                             - Do idle operation
 * cpu_dead                                - Put the current physical CPU in halt state.
 * set_current_pcpu_id                     - Write current physical CPU ID to MSR_IA32_TSC_AUX.
 * print_hv_banner                         - Print the boot message.
 * asm_monitor                             - Set up a linear address range to be monitored by hardware and activate
 *                                           the monitor.
 * asm_mwait                               - Enter an implementation-dependent optimized state.
 * wait_sync_change(sync, wake_sync)       - Wait until \a *sync is equal to \a wake_sync.
 * init_pcpu_xsave                         - Initialize physical CPU XSAVE features.
 */

/**
 * @brief The expiration time of waiting for the CPU to be startup,
 * and the unit is millisecond.
 */
#define CPU_UP_TIMEOUT   100U /* millisecond */
/**
 * @brief The expiration time of waiting for the CPU to be offline,
 * and the unit is millisecond.
 */
#define CPU_DOWN_TIMEOUT 100U /* millisecond */

/**
 * @brief An array that contains information will be holden by per CPU.
 */
struct per_cpu_region per_cpu_data[MAX_PCPU_NUM] __aligned(PAGE_SIZE);
/**
 * @brief Number of physical CPUs on the platform.
 */
static uint16_t phys_cpu_num = 0U;
/**
 * @brief An integer value representing inter-processor message.
 *
 * 0 : indicating that all expected APs has been signaled to wake up and APs could continue execution in init_pcpu_post.
 * 1 : indicating that BP has set up the environment and it is ready to wake up and initialize APs.
 */
static uint64_t pcpu_sync = 0UL;
/**
 * @brief An integer value representing the startup address of APs.
 */
static uint64_t startup_paddr = 0UL;

static void init_pcpu_xsave(void);
static void set_current_pcpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id);

/**
 * @brief Initialize lapic_id field of per_cpu_data.
 *
 * @return A Boolean value representing whether the initialization of lapic_id field of per_cpu_data is succeeded.
 *
 * @retval true The initialization of lapic_id field of per_cpu_data is succeeded.
 * @retval false The initialization of lapic_id field of per_cpu_data isn't succeeded.
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static bool init_percpu_lapic_id(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the index of per_cpu_data, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type uint32_t [].
	 *  - lapic_id_array representing an array which stores uint32_t type values representing local APIC id, where
	 *    the array has MAX_PCPU_NUM elements, not initialized. */
	uint32_t lapic_id_array[MAX_PCPU_NUM];
	/** Declare the following local variables of type bool.
	 *  - success representing whether the initialization of lapic_id field of per_cpu_data is succeed, initialized
	 *    as false. */
	bool success = false;

	/** Set phys_cpu_num to the return value of parse_madt(lapic_id_array),
	 *  which is number of physical local APICs on current platform */
	phys_cpu_num = parse_madt(lapic_id_array);

	/** If phys_cpu_num is not equal to 0 and phys_cpu_num is less than or equal to MAX_PCPU_NUM */
	if ((phys_cpu_num != 0U) && (phys_cpu_num <= MAX_PCPU_NUM)) {
		/** For each i ranging from 0 to 'phys_cpu_num - 1' [with a step of 1] */
		for (i = 0U; i < phys_cpu_num; i++) {
			/** Set lapic_id field of the (i+1)-th element in the per-CPU region to the (i+1)-th entry of
			 *  lapic_id_array. */
			per_cpu(lapic_id, i) = lapic_id_array[i];
		}
		/** Set 'success' to true */
		success = true;
	}

	/** Return success */
	return success;
}

/**
 * @brief Set the state of CPU to \a state.
 *
 * @param[in]    pcpu_id The ID of the physical CPU whose state will be set.
 * @param[inout] state The state that will be written to the CPU's state field, where the CPU's id is \a pcpu_id.
 *
 * @return None
 *
 * @pre pcpu_id < MAX_PCPU_NUM
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void pcpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state)
{
	/** If \a state is equal to PCPU_STATE_RUNNING. */
	if (state == PCPU_STATE_RUNNING) {
		/* Save this CPU's logical ID to the TSC AUX MSR */
		/** Call set_current_pcpu_id with the following parameters, in order to write \a pcpu_id
		 *  to MSR_IA32_TSC_AUX.
		 *  - pcpu_id */
		set_current_pcpu_id(pcpu_id);
	}

	/** Set boot_state field of the (pcpu_id+1)-th element in the per-CPU region to \a state, where the CPU's id is
	 *  \a pcpu_id. */
	per_cpu(boot_state, pcpu_id) = state;
}

/**
 * @brief The function returns the number of CPU cores detected by the hypervisor.
 *
 * @return The number of physical CPUs.
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @remark This API can be called only after init_pcpu_pre() has been called once on any processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
uint16_t get_pcpu_nums(void)
{
	/** Return phys_cpu_num. */
	return phys_cpu_num;
}

/**
 * @brief Perform the physical CPU's early stage initialization.
 *
 * @param[in]    is_bsp A Boolean value representing whether the current physical CPU is BP.
 *
 * @return None
 *
 * @pre boot_regs[0] == MULTIBOOT_INFO_MAGIC
 * @pre (((struct multiboot_info *)boot_regs[1])->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0
 * @pre Calling of this function may happen before a proper page table has been set up. Thus the function hpa2hva
 * cannot be used here.
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark The first entry of Processor Local APIC Structure in ACPI table shall be for the physical BP.
 * @remark The physical platform shall guarantee that first instruction of ACRN hypervisor is executed on the physical
 * BP.
 * @remark The physical platform shall guarantee each physical AP waits for the first INIT IPI.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void init_pcpu_pre(bool is_bsp)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing physical CPU ID, not initialized. */
	uint16_t pcpu_id;

	/** If is_bsp is true */
	if (is_bsp) {
		/** Set pcpu_id to BOOT_CPU_ID. */
		pcpu_id = BOOT_CPU_ID;

		/** Call memset with the following parameters, in order to initialize BSS section as all 0s.
		 *  - &ld_bss_start
		 *  - 0
		 *  - &ld_bss_end - &ld_bss_start */
		(void)memset(&ld_bss_start, 0U, (size_t)(&ld_bss_end - &ld_bss_start));
		/*
		 * Enable UART as early as possible.
		 * Then we could use printf for debugging on early boot stage.
		 */
		/** Call uart16550_init with the following parameters, in order to initialize the uart16550.
		 *  - true, which let hypervisor initialize the PCI memory region once
		 *  before do initialization for physical UART. */
		uart16550_init(true);

		/** Call bsp_init with the following parameters,
		 *  in order to let BSP (Board Support Package) do the per-processor initialization. */
		bsp_init();

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		/** Call init_pcpu_capabilities with the following parameters, in order to initialize an internal data
		 *  structure that stores the CPU information data. */
		init_pcpu_capabilities();

		/** Call init_pcpu_model_name in order to initialize processor model name. */
		init_pcpu_model_name();

		/* Initialize the hypervisor paging */
		/** Call init_e820 in order to initialize physical E820 entries in hypervisor. */
		init_e820();
		/** Call init_paging in order to set up the MMU page table. */
		init_paging();

		/*
		 * Need update uart_base_address here for vaddr2paddr mapping may changed
		 * WARNNING: DO NOT CALL PRINTF BETWEEN ENABLE PAGING IN init_paging AND HERE!
		 */
		/** Call uart16550_init with the following parameters, in order to initialize the uart16550.
		 *  - false, which don't let hypervisor initialize the PCI memory region once
		 *  before do initialization for physical UART. */
		uart16550_init(false);

		/** Call early_init_lapic in order to enable x2APIC mode. */
		early_init_lapic();

		/** If the return value of init_percpu_lapic_id() is false,
		 *  indicating that fails to initialize lapic_id field of per_cpu_data */
		if (!init_percpu_lapic_id()) {
			/** Call panic in order to enter safety mode. */
			panic("failed to init_percpu_lapic_id!");
		}
	} else {
		/** Call bsp_init with the following parameters,
		 *  in order to let BSP (Board Support Package) do the per-processor initialization. */
		bsp_init();

		/* Switch this CPU to use the same page tables set-up by the
		 * primary/boot CPU
		 */
		/** Call enable_paging in order to enable paging. */
		enable_paging();

		/** Call early_init_lapic in order to enable x2APIC mode. */
		early_init_lapic();

		/** Set pcpu_id to the return value of get_pcpu_id_from_lapic_id(get_cur_lapic_id()),
		 *  which is physical CPU ID whose local APIC ID is equal to current CPU's local APIC ID. */
		pcpu_id = get_pcpu_id_from_lapic_id(get_cur_lapic_id());
		/** If pcpu_id is greater than or equal to MAX_PCPU_NUM. */
		if (pcpu_id >= MAX_PCPU_NUM) {
			/** Call panic in order to enter safety mode. */
			panic("Invalid pCPU ID!");
		}
	}

	/** Call pcpu_set_current_state with the following parameters, in order to
	 *  set the state of the CPU (whose ID is \a pcpu_id) to PCPU_STATE_RUNNING
	 *  - pcpu_id
	 *  - PCPU_STATE_RUNNING */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_RUNNING);
}

/**
 * @brief Perform the pcpu's late stage initialization.
 *
 * @param[in]    pcpu_id ID of the pcpu to be initialized.
 *
 * @return None
 *
 * @pre pcpu_id == get_pcpu_id()
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @remark This API can be called only after init_pcpu_pre()has been called once on the current logical processor.
 * @remark This API can be called on logical processors with \a pcpu_id != 0H only after
 * this has been called once on the logical processor with pcpu_id==0H.
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void init_pcpu_post(uint16_t pcpu_id)
{
#ifdef STACK_PROTECTOR
	/** Call set_fs_base in order to initialize the per-CPU stack canary structure for the current physical CPU. */
	set_fs_base();
#endif
#ifndef QEMU
	/** Call msr_write with the following parameters, in order to enable RTM force abort mode.
	 * - MSR_TSX_FORCE_ABORT
	 * - 1 */
	msr_write(MSR_TSX_FORCE_ABORT, 1U);
#endif

	/** Call load_gdtr_and_tr in order to load GDTR and TR. */
	load_gdtr_and_tr();

	/** Call init_pcpu_xsave in order to initialize physical CPU XSAVE features. */
	init_pcpu_xsave();

	/** If pcpu_id is equal to BOOT_CPU_ID. */
	if (pcpu_id == BOOT_CPU_ID) {
		/* Print Hypervisor Banner */
		/** Call print_hv_banner in order to print the boot message. */
		print_hv_banner();

		/* Calibrate TSC Frequency */
		/** Call calibrate_tsc in order to initialize the frequency of TSC. */
		calibrate_tsc();

		/** Logging the following information with a log level of LOG_ACRN.
		 *  - HV_FULL_VERSION
		 *  - HV_BUILD_TIME
		 *  - HV_BUILD_VERSION
		 *  - HV_BUILD_TYPE
		 *  - HV_DAILY_TAG
		 *  - HV_BUILD_USER
		 *  - HV_CONFIG_TOOL */
		pr_acrnlog("HV version %s-%s-%s %s (daily tag:%s) build by %s%s", HV_FULL_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE, HV_DAILY_TAG, HV_BUILD_USER, HV_CONFIG_TOOL);

		/** Logging the following information with a log level of LOG_ACRN.
		 *  - HV_API_MAJOR_VERSION
		 *  - HV_API_MINOR_VERSION */
		pr_acrnlog("API version %u.%u", HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);

		/** Logging the following information with a log level of LOG_ACRN.
		 *  - (get_pcpu_info())->model_name */
		pr_acrnlog("Detect processor: %s", (get_pcpu_info())->model_name);

		/** Logging the following information with a log level of LOG_DEBUG.
		 *  - BOOT_CPU_ID */
		pr_dbg("Core %hu is up", BOOT_CPU_ID);

		/* Warn for security feature not ready */
		/** If the return value of check_cpu_security_cap() is false,
		 *  indicating that security system software interfaces are not supported
		 *  by physical platform for known CPU vulnerabilities mitigation. */
		if (!check_cpu_security_cap()) {
			/** Logging a message with a log level of LOG_FATAL to
			 *  indicate CPU has vulnerabilities. */
			pr_fatal("SECURITY WARNING!!!!!!");
			/** Logging a message with a log level of LOG_FATAL to
			 *  indicate CPU need to apply the latest CPU uCode patch. */
			pr_fatal("Please apply the latest CPU uCode patch!");
		}

		/* Initialize interrupts */
		/** Call init_interrupt with the following parameters, in order to initialize interrupt related
		 *  resources for current physical CPU (whose ID is BOOT_CPU_ID).
		 *  - BOOT_CPU_ID */
		init_interrupt(BOOT_CPU_ID);

		/** Call init_iommu in order to initialize IOMMU. */
		init_iommu();
		/** Call enable_iommu in order to enable IOMMU. */
		enable_iommu();

		/* Start all secondary cores */
		/** If the return value of start_pcpus(AP_MASK) is false,
		 *  indicating that there are any CPUs set in mask aren't started */
		if (!start_pcpus(AP_MASK)) {
			/** Call panic in order to enter safety mode. */
			panic("Failed to start all secondary cores!");
		}

		/** Call ASSERT with the following parameters, in order to
		 *  assert if the ID of the current physical CPU is not BOOT_CPU_ID
		 *  - get_pcpu_id() == BOOT_CPU_ID
		 *  - "" */
		ASSERT(get_pcpu_id() == BOOT_CPU_ID, "");
	} else {
		/** Logging the following information with a log level of LOG_DEBUG.
		 *  - pcpu_id */
		pr_dbg("Core %hu is up", pcpu_id);

		/* Initialize secondary processor interrupts. */
		/** Call init_interrupt with the following parameters, in order to initialize interrupt related
		 *  resources for current physical CPU (whose ID is \a pcpu_id).
		 *  - pcpu_id */
		init_interrupt(pcpu_id);

		/** Call wait_sync_change with the following parameters, in order to wait until \a pcpu_sync is equal
		 *  to 0.
		 *  - &pcpu_sync
		 *  - 0 */
		wait_sync_change(&pcpu_sync, 0UL);
	}

	/** Call init_sched with the following parameters, in order to initialize scheduler control block of the CPU
	 *  whose CPU id is pcpu_id.
	 *  - pcpu_id */
	init_sched(pcpu_id);

	/** Call enable_smep in order to enable supervisor mode execution prevention on the current processor. */
	enable_smep();

	/** Call enable_smap in order to enable supervisor mode access prevention on the current processor. */
	enable_smap();
}

/**
 * @brief Get physical CPU id whose local APIC id is equal to \a lapic_id.
 *
 * @param[in]    lapic_id Local APIC id to be used as a key to find the physical CPU id.
 *
 * @return Physical CPU id whose local APIC id is \a lapic_id.
 *
 * @retval INVALID_CPU_ID Not find any physical CPU whose local APIC id is \a lapic_id.
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the index of per_cpu_data, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing the physical CPU id, initialized as INVALID_CPU_ID, not initialized. */
	uint16_t pcpu_id = INVALID_CPU_ID;

	/** For each i ranging from 0 to 'phys_cpu_num - 1' [with a step of 1] */
	for (i = 0U; i < phys_cpu_num; i++) {
		/** If per_cpu_data[i].lapic_id is equal to lapic_id. */
		if (per_cpu(lapic_id, i) == lapic_id) {
			/** Set pcpu_id to i */
			pcpu_id = i;
			/** Terminate the loop */
			break;
		}
	}

	/** Return pcpu_id. */
	return pcpu_id;
}

/**
 * @brief Start the physical CPU whose CPU id is \a pcpu_id.
 *
 * @param[in]    pcpu_id The CPU id of the CPU that will be started.
 *
 * @return Whether the physical CPU is start successful, where the physical CPU's id is \a pcpu_id.
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety When \a pcpu_id is different among parallel invocation.
 */
static bool start_pcpu(uint16_t pcpu_id)
{
	/** Declare the following local variables of type uint32_t.
	 *  - timeout representing the milliseconds to be waited until status of the CPU enter to PCPU_STATE_RUNNING
	 *    mode, the time unit is millisecond, not initialized. */
	uint32_t timeout;
	/** Declare the following local variables of type bool.
	 *  - status representing whether the physical CPU is start successful, initialized as true, where the physical
	 *    CPU's id is \a pcpu_id, initialized as true. */
	bool status = true;

	/* Update the stack for pcpu */
	/** Call stac in order to set the AC flag bit in the EFLAGS register and make the supervisor-mode data have
	 *  rights to access the user-mode pages even if the SMAP bit is set in the CR4 register. */
	stac();
	/** Call write_trampoline_stack_sym with the following parameters, in order to set up the stack for AP.
	 *  - pcpu_id */
	write_trampoline_stack_sym(pcpu_id);
	/** Call clac in order to clear the AC bit in the EFLAGS register and make the supervisor-mode data do not
	 *  have rights to access the user-mode pages */
	clac();

	/** Call send_startup_ipi with the following parameters, in order to send a INIT-SIPI sequence to target
	 *  logical processor whose CPU id is \a pcpu_id.
	 *  - INTR_CPU_STARTUP_USE_DEST
	 *  - pcpu_id
	 *  - startup_paddr */
	send_startup_ipi(INTR_CPU_STARTUP_USE_DEST, pcpu_id, startup_paddr);

	/* Wait until the pcpu with pcpu_id is running and set the active bitmap or
	 * configured time-out has expired
	 */
	/** Set timeout to the value calculated by multiplying CPU_UP_TIMEOUT by 1000. */
	timeout = CPU_UP_TIMEOUT * 1000U;
	/** Until any of the following conditions hold:
	 *  - per_cpu_data[pcpu_id].boot_state is equal to PCPU_STATE_RUNNING
	 *  - timeout is equal to 0. */
	while ((per_cpu_data[pcpu_id].boot_state != PCPU_STATE_RUNNING) && (timeout != 0U)) {
		/** Call udelay with the following parameters, in order to delay 10us.
		 *  - 10 */
		udelay(10U);

		/** Decrement timeout by 10. */
		timeout -= 10U;
	}

	/** If per_cpu_data[pcpu_id].boot_state isn't equal to PCPU_STATE_RUNNING. */
	if (per_cpu_data[pcpu_id].boot_state != PCPU_STATE_RUNNING) {
		/** Logging the following information with a log level of LOG_FATAL.
		 *  - pcpu_id */
		pr_fatal("Secondary CPU%hu failed to come up", pcpu_id);
		/** Call pcpu_set_current_state with the following parameters, in order to
		 *  set the state of the CPU (whose ID is \a pcpu_id) to PCPU_STATE_RUNNING
		 *  - pcpu_id
		 *  - PCPU_STATE_DEAD */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
		/** Set status to false. */
		status = false;
	}

	/** Return status. */
	return status;
}

/**
 * @brief Start all cpus if the bit is set in mask except itself.
 *
 * @param[in] mask Bits mask of cpus which should be started.
 *
 * @return Whether all mask of cpus is started successfully.
 *
 * @retval true if all cpus set in mask are started
 * @retval false if there are any cpus set in mask aren't started
 *
 * @pre (mask & ~(1 << get_pcpu_nums()) - 1) == 0
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @remark The physical platform shall guarantee that each module
 * load physical address is higher than 100000H.
 * @remark The physical platform shall guarantee that
 * there is no overlap among hypervisor image and boot module images.
 * @remark This interface can be called only after early_init_lapic
 * has been called once on the current logical processor.
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
bool start_pcpus(uint64_t mask)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i the least significant bit set in the specified mask,
	 *  indicating the next to-be-started physical CPU, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing current physical CPU ID, initialized as get_pcpu_id(). */
	uint16_t pcpu_id = get_pcpu_id();
	/** Declare the following local variables of type uint64_t.
	 *  - expected_start_mask representing bits mask of cpus which should be started, initialized as mask. */
	uint64_t expected_start_mask = mask;
	/** Declare the following local variables of type bool.
	 *  - status representing whether all CPUs specified in \a mask are started successfully, initialized as
	 *    true. */
	bool status = true;

	/** Set startup_paddr to the the return value of prepare_trampoline(),
	 *  which is the start address of the trampoline code. */
	startup_paddr = prepare_trampoline();

	/** Set pcpu_sync to 1. */
	pcpu_sync = 1UL;
	/** Call cpu_write_memory_barrier with the following parameters, in order to synchronize all write and read
	 *  accesses to memory. */
	cpu_write_memory_barrier();

	/** Set i to ffs64(expected_start_mask). */
	i = ffs64(expected_start_mask);
	/** Until i is equal to INVALID_BIT_INDEX. */
	while (i != INVALID_BIT_INDEX) {
		/** Call bitmap_clear_nolock with the following parameters, in order to clear the (i+1)-th bit in the
		 *  expected_start_mask.
		 *  - i
		 *  - &expected_start_mask */
		bitmap_clear_nolock(i, &expected_start_mask);

		/** If pcpu_id is equal to i. */
		if (pcpu_id == i) {
			/** Set i to the return value of 'ffs64(expected_start_mask)'. */
			i = ffs64(expected_start_mask);
			/** Continue to next iteration */
			continue; /* Avoid start itself */
		}

		/** If the return value of start_pcpu(i) is false, indicating that the start-up of the physical
		 *  CPU (CPU ID is i) is failed. */
		if (start_pcpu(i) == false) {
			/** Set status to false */
			status = false;
			/** Terminate the loop */
			break;
		}
		/** Set i to ffs64(expected_start_mask). */
		i = ffs64(expected_start_mask);
	}

	/* Trigger event to allow secondary CPUs to continue */
	/** Set pcpu_sync to 0. */
	pcpu_sync = 0UL;

	/** Return status. */
	return status;
}

/**
 * @brief Submit a request to offline the target CPU.
 *
 * @param[in]    pcpu_id The CPU id of the target CPU.
 *
 * @pre N/A
 * @post N/A
 *
 * @return None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void make_pcpu_offline(uint16_t pcpu_id)
{
	/** Call bitmap_set_lock with the following parameters, in order to set pcpu_flag of
	 *  per_cpu_data[pcpu_id].pcpu_flag to NEED_OFFLINE
	 *  - NEED_OFFLINE
	 *  - &per_cpu(pcpu_flag, pcpu_id) */
	bitmap_set_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
	/** If current physical CPU's id is equal to pcpu_id. */
	if (get_pcpu_id() != pcpu_id) {
		/** Call send_single_init with the following parameters, in order to send INIT IPI message to CPU whose
		 *  CPU id is pcpu_id.
		 *  - pcpu_id */
		send_single_init(pcpu_id);
	}
}

/**
 * @brief Check whether the specified CPU is requested to be offlined.
 *
 * Test and clear the NEED_OFFLINE bit of pcpu_flag field of CPU(whose CPU id is \a pcpu_id) per-CPU region.
 *
 * @param[in]    pcpu_id The CPU id of the target CPU whose pcpu_flag field of per-CPU region will be test and clear.
 *
 * @pre N/A
 * @post N/A
 *
 * @return Whether the specified CPU is requested to be offlined.
 *
 * @retval true The specified CPU is requested to be offlined.
 * @retval false The specified CPU isn't requested to be offlined.
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety when \a pcpu_id is different among parallel invocation.
 */
bool need_offline(uint16_t pcpu_id)
{
	/** Return the return value of bitmap_test_and_clear_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id)),
	 *  which indicates whether the NEED_OFFLINE bit is set in the pcpu_flag field of per-CPU region of CPU before
	 *  clearing NEED_OFFLINE bit, where the CPU's id is cpu_id */
	return bitmap_test_and_clear_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
}

/**
 * @brief Check whether there is any pcpu still active.
 *
 * @param[in] mask Bits mask of pcpus whose active status need to be checked.
 *
 * @return Whether there is any pcpu still active.
 *
 * @retval true There is pcpu still active.
 * @retval false There is no pcpu active.
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static bool is_any_pcpu_active(uint64_t mask)
{
	/** Declare the following local variables of type uint16_t.
	 *  - i representing the index of per_cpu_data, not initialized. */
	uint16_t i;
	/** Declare the following local variables of type uint64_t.
	 *  - pcpu_mask representing bits mask of pcpus whose active status need to be checked, initialized as mask. */
	uint64_t pcpu_mask = mask;
	/** Declare the following local variables of type bool.
	 *  - status representing whether there is any pcpu still active, initialized as false. */
	bool status = false;

	/** Set i to ffs64(pcpu_mask). */
	i = ffs64(pcpu_mask);
	/** Until i is equal to INVALID_BIT_INDEX. */
	while (i != INVALID_BIT_INDEX) {
		/** If per_cpu_data[i].boot_state is equal to PCPU_STATE_RUNNING. */
		if (per_cpu_data[i].boot_state == PCPU_STATE_RUNNING) {
			/** Set status to true */
			status = true;
			/** Terminate the loop */
			break;
		}
		/** Call bitmap_clear_nolock with the following parameters, in order to clear the (i+1)-th bit in
		 *  pcpu_mask.
		 *  - i
		 *  - &pcpu_mask */
		bitmap_clear_nolock(i, &pcpu_mask);
		/** Set i to ffs64(pcpu_mask). */
		i = ffs64(pcpu_mask);
	}

	/** Return status */
	return status;
}

/**
 * @brief Wait for pcpu offline with a timeout of 100ms.
 *
 * Wait until all CPUs specified by \a mask are offlined or or timeout expired.
 *
 * @param[in]    mask Bits mask of pcpus that needs to wait to offline.
 *
 * @return None
 *
 * @pre (mask & ~(1 << get_pcpu_nums()) - 1) == 0
 * @post N/A
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void wait_pcpus_offline(uint64_t mask)
{
	/** Declare the following local variables of type uint32_t.
	 *  - timeout representing the time to expire, not initialized. */
	uint32_t timeout;

	/** Set timeout to the value calculated by multiplying CPU_DOWN_TIMEOUT by 1000. */
	timeout = CPU_DOWN_TIMEOUT * 1000U;
	/** Until any one of the following conditions holds:
	 *  1. is_any_pcpu_active(mask) == false
	 *  2. timeout is equal to 0. */
	while (is_any_pcpu_active(mask) && (timeout != 0U)) {
		/** Call udelay with the following parameters, in order to delay 10 microseconds.
		 *  - 10 */
		udelay(10U);
		/** Decrement timeout by 10. */
		timeout -= 10U;
	}
}

/**
 * @brief Put current physical processor in idle state.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void cpu_do_idle(void)
{
	/** Call asm_pause in order to pause the current CPU. */
	asm_pause();
}

/**
 * @brief The function puts the current physical CPU in halt state.
 *
 * @return None
 *
 * @pre N/A
 * @post N/A
 *
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void cpu_dead(void)
{
	/** Declare the following local variables of type uint16_t.
	 *  - pcpu_id representing current physical CPU ID, initialized as get_pcpu_id(). */
	uint16_t pcpu_id = get_pcpu_id();

	/** Call deinit_sched with the following parameters, in order to deinitialize scheduler control block of the
	 *  current physical CPU.
	 *  - pcpu_id */
	deinit_sched(pcpu_id);
	/** If the boot_state field of per-CPU region of current physical CPU is equal to PCPU_STATE_RUNNING. */
	if (per_cpu_data[pcpu_id].boot_state == PCPU_STATE_RUNNING) {
		/* clean up native stuff */
		/** Call vmx_off in order to leave VMX operation on current logical processor. */
		vmx_off();
		/** Call cache_flush_invalidate_all in order to writes back all modified cache lines in the processor's
		 *  internal cache to main memory and invalidates (flushes) the internal caches. */
		cache_flush_invalidate_all();

		/* Set state to show CPU is dead */
		/** Call pcpu_set_current_state with the following parameters, in order to
		 *  set the state of the CPU (whose ID is \a pcpu_id) to PCPU_STATE_DEAD
		 *  - pcpu_id
		 *  - PCPU_STATE_DEAD */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);

		/* Halt the CPU */
		/** Dead loop. */
		do {
			/** Call asm_hlt in order to stop instruction execution and places the processor in a HALT
			 *  state. */
			asm_hlt();
		} while (1);
	} else {
		/** Logging the following information with a log level of LOG_ERROR.
		 *  - pcpu_id */
		pr_err("pcpu%hu already dead", pcpu_id);
	}
}

/**
 * @brief Write the given physical CPU ID to MSR_IA32_TSC_AUX.
 *
 * @param[in]    pcpu_id Physical CPU ID will be written.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_INIT_POST_SMP, HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static void set_current_pcpu_id(uint16_t pcpu_id)
{
	/* Write TSC AUX register */
	/** Call msr_write with the following parameters,
	 *  in order to write \a pcpu_id to physical MSR MSR_IA32_TSC_AUX.
	 *  - MSR_IA32_TSC_AUX
	 *  - pcpu_id */
	msr_write(MSR_IA32_TSC_AUX, (uint64_t)pcpu_id);
}

/**
 * @brief Print the boot message.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void print_hv_banner(void)
{
	/** Declare the following local variables of type const char *.
	 *  - boot_msg representing the message will be printed, initialized as "ACRN Hypervisor\n\r". */
	const char *boot_msg = "ACRN Hypervisor\n\r";

	/** Call printf with the following parameters, in order to print the boot message.
	 *  - boot_msg */
	printf(boot_msg);
}

/**
 * @brief Set up a linear address range to be monitored by hardware and activate the monitor.
 *
 * @param[in]    addr The start address to be monitored.
 * @param[in]    ecx Optional extensions of MONITOR instruction.
 * @param[in]    edx Optional hints of MONITOR instruction.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void asm_monitor(volatile const uint64_t *addr, uint64_t ecx, uint64_t edx)
{
	/** Execute monitor in order to set up a linear address range to be monitored by hardware and activate the
	 *  monitor.
	 *  - Input operands: RAX holds addr, RCX holds ecx, RDX holds edx
	 *  - Output operands: None
	 *  - Clobbers: None */
	asm volatile("monitor\n" : : "a"(addr), "c"(ecx), "d"(edx));
}

/**
 * @brief Enter an implementation-dependent optimized state.
 *
 * Make processor to stop instruction execution and enter an implementation-dependent optimized state until occurrence
 * of a class of events.
 *
 * @param[in]    eax A value contain hints such as the preferred optimized state the processor should enter.
 * @param[in]    ecx Optional extensions for the MWAIT instruction.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
static inline void asm_mwait(uint64_t eax, uint64_t ecx)
{
	/** Execute mwait in order to enter an implementation-dependent optimized state.
	 *  - Input operands: RAX holds eax, RCX holds ecx.
	 *  - Output operands: None
	 *  - Clobbers:None */
	asm volatile("mwait\n" : : "a"(eax), "c"(ecx));
}

/**
 * @brief Wait until \a *sync is equal to \a wake_sync.
 *
 * @param[in]    sync The pointer which points to the value that will be watched to verify whether is equal to
 * \a wake_sync.
 * @param[in] wake_sync The value that is used to be compared with \a sync.
 *
 * @return None
 *
 * @pre sync != NULL
 *
 * @mode HV_SUBMODE_INIT_POST_SMP
 *
 * @reentrancy Unspecified
 * @threadsafety Yes
 */
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync)
{
	/** If the return value of has_monitor_cap() is true,
	 *  indicating that the CPU supports MONITOR/MWAIT instructions */
	if (has_monitor_cap()) {
		/* Wait for the event to be set using monitor/mwait */
		/** Until *sync is equal to wake_sync */
		while ((*sync) != wake_sync) {
			/** Call asm_monitor with the following parameters, in order to set up a linear address range
			 *  to let the address range (64-bit) pointed by \a sync be monitored by hardware and activate
			 *  the monitor.
			 *  - sync
			 *  - 0
			 *  - 0 */
			asm_monitor(sync, 0UL, 0UL);
			/** If *sync is equal to wake_sync. */
			if ((*sync) != wake_sync) {
				/** Call asm_mwait with the following parameters, in order to enter an
				 *  implementation-dependent optimized state.
				 *  - 0
				 *  - 0 */
				asm_mwait(0UL, 0UL);
			}
		}
	} else {
		/** Until *sync is equal to wake_sync. */
		while ((*sync) != wake_sync) {
			/** Call asm_pause with the following parameters, in order to pause the current CPU. */
			asm_pause();
		}
	}
}

/**
 * @brief Initialize physical CPU XSAVE features.
 *
 * @return None
 *
 * @mode HV_SUBMODE_INIT_PRE_SMP, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
static void init_pcpu_xsave(void)
{
	/** Declare the following local variables of type uint64_t.
	 *  - val64 representing the value will be written into CR4, not initialized. */
	uint64_t val64;
	/** Declare the following local variables of type struct cpuinfo_x86 *.
	 *  - cpu_info representing the CPU information of BP, not initialized. */
	struct cpuinfo_x86 *cpu_info;
	/** Declare the following local variables of type uint32_t.
	 *  - eax representing the eax value used as output parameter of cpuid function call, not initialized.
	 *  - ecx representing the ecx value used as output parameter of cpuid function call, not initialized.
	 *  - unused representing the unused value and only used as output parameter of cpuid function call, not
	 *    initialized.
	 *  - xsave_area_size representing the size in bytes of the XSAVE area containing all states enabled by
	 *    XCR0 | IA32_XSS, not initialized. */
	uint32_t eax, ecx, unused, xsave_area_size;

	/** Call CPU_CR_READ with the following parameters, in order to read CR4 value and saved the content to val64.
	 *  - cr4
	 *  - val64 */
	CPU_CR_READ(cr4, &val64);
	/** Bitwise OR val64 by CR4_OSXSAVE */
	val64 |= CR4_OSXSAVE;
	/** Call CPU_CR_WRITE with the following parameters, in order to write val64 into the CR4
	 *  - cr4
	 *  - val64. */
	CPU_CR_WRITE(cr4, val64);

	/** If the current CPU ID is BOOT_CPU_ID */
	if (get_pcpu_id() == BOOT_CPU_ID) {
		/** Call cpuid with the following parameters, in order to get feature information of the BP.
		 *  - CPUID_FEATURES
		 *  - &unused
		 *  - &unused
		 *  - &ecx
		 *  - &unused */
		cpuid(CPUID_FEATURES, &unused, &unused, &ecx, &unused);

		/** If OSXSAVE Bit (Bit 27) of 'ecx' is 1. */
		if ((ecx & CPUID_ECX_OSXSAVE) != 0U) {
			/** Set cpu_info to the return value of get_pcpu_info(), which contains CPU information data. */
			cpu_info = get_pcpu_info();
			/** Set OSXSAVE Bit (Bit 27) of 'cpu_info->cpuid_leaves[FEAT_1_ECX]' to 1 */
			cpu_info->cpuid_leaves[FEAT_1_ECX] |= CPUID_ECX_OSXSAVE;

			/** Call write_xcr with the following parameters, in order to write XCR0_INIT to physical XCR0.
			 *  - 0
			 *  - XCR0_INIT */
			write_xcr(0, XCR0_INIT);
			/** Call msr_write with the following parameters, in order to write XSS_INIT to physical
			 *  MSR_IA32_XSS.
			 *  - MSR_IA32_XSS
			 *  - XSS_INIT */
			msr_write(MSR_IA32_XSS, XSS_INIT);

			/** Call cpuid_subleaf with the following parameters,
			 *  in order to get the physical processor information for CPUID.(EAX=DH,ECX=1H)
			 *  and set 'xsave_area_size' to physical CPUID.(EAX=DH,ECX=1H):EBX.
			 *  - CPUID_XSAVE_FEATURES
			 *  - 1
			 *  - &eax
			 *  - &xsave_area_size
			 *  - &ecx
			 *  - &unused */
			cpuid_subleaf(CPUID_XSAVE_FEATURES, 1U, &eax, &xsave_area_size, &ecx, &unused);
			/** If xsave_area_size is greater than XSAVE_STATE_AREA_SIZE */
			if (xsave_area_size > XSAVE_STATE_AREA_SIZE) {
				/** Call panic in order to enter safety state. */
				panic("XSAVE area (%d bytes) exceeds the pre-allocated 4K region\n", xsave_area_size);
			}
		}
	}
}

/**
 * @}
 */
