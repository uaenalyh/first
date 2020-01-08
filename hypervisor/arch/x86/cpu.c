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
#include <cat.h>
#include <sgx.h>
#include <uart16550.h>

/**
 * @defgroup hwmgmt_cpu hwmgmt.cpu
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

#define CPU_UP_TIMEOUT   100U /* millisecond */
#define CPU_DOWN_TIMEOUT 100U /* millisecond */

struct per_cpu_region per_cpu_data[MAX_PCPU_NUM] __aligned(PAGE_SIZE);
static uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static uint64_t startup_paddr = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
static volatile uint64_t pcpu_active_bitmap = 0UL;

static void init_pcpu_xsave(void);
static void set_current_pcpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id);
static uint64_t start_tsc __attribute__((__section__(".bss_noinit")));

/**
 * @pre phys_cpu_num <= MAX_PCPU_NUM
 */
static bool init_percpu_lapic_id(void)
{
	uint16_t i;
	uint32_t lapic_id_array[MAX_PCPU_NUM];
	bool success = false;

	/* Save all lapic_id detected via parse_mdt in lapic_id_array */
	phys_cpu_num = parse_madt(lapic_id_array);

	if ((phys_cpu_num != 0U) && (phys_cpu_num <= MAX_PCPU_NUM)) {
		for (i = 0U; i < phys_cpu_num; i++) {
			per_cpu(lapic_id, i) = lapic_id_array[i];
		}
		success = true;
	}

	return success;
}

static void pcpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state)
{
	/* Check if state is initializing */
	if (state == PCPU_STATE_INITIALIZING) {

		/* Save this CPU's logical ID to the TSC AUX MSR */
		set_current_pcpu_id(pcpu_id);
	}

	/* Set state for the specified CPU */
	per_cpu(boot_state, pcpu_id) = state;
}

/*
 * @post return <= MAX_PCPU_NUM
 */
uint16_t get_pcpu_nums(void)
{
	return phys_cpu_num;
}

bool is_pcpu_active(uint16_t pcpu_id)
{
	return bitmap_test(pcpu_id, &pcpu_active_bitmap);
}

void init_pcpu_pre(bool is_bsp)
{
	uint16_t pcpu_id;
	int32_t ret;

	if (is_bsp) {
		pcpu_id = BOOT_CPU_ID;
		start_tsc = rdtsc();

		/* Clear BSS */
		(void)memset(&ld_bss_start, 0U, (size_t)(&ld_bss_end - &ld_bss_start));
		/*
		 * Enable UART as early as possible.
		 * Then we could use printf for debugging on early boot stage.
		 */
		uart16550_init(true);

		bsp_init();

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		init_pcpu_capabilities();

		init_pcpu_model_name();

		/* Initialize the hypervisor paging */
		init_e820();
		init_paging();

		/*
		 * Need update uart_base_address here for vaddr2paddr mapping may changed
		 * WARNNING: DO NOT CALL PRINTF BETWEEN ENABLE PAGING IN init_paging AND HERE!
		 */
		uart16550_init(false);

		early_init_lapic();

		if (!init_percpu_lapic_id()) {
			panic("failed to init_percpu_lapic_id!");
		}

		ret = init_ioapic_id_info();
		if (ret != 0) {
			panic("System IOAPIC info is incorrect!");
		}

	} else {
		bsp_init();

		/* Switch this CPU to use the same page tables set-up by the
		 * primary/boot CPU
		 */
		enable_paging();

		early_init_lapic();

		pcpu_id = get_pcpu_id_from_lapic_id(get_cur_lapic_id());
		if (pcpu_id >= MAX_PCPU_NUM) {
			panic("Invalid pCPU ID!");
		}
	}

	bitmap_set_lock(pcpu_id, &pcpu_active_bitmap);

	/* Set state for this CPU to initializing */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);
}

void init_pcpu_post(uint16_t pcpu_id)
{
#ifdef STACK_PROTECTOR
	set_fs_base();
#endif
	load_gdtr_and_tr();

	init_pcpu_xsave();

	if (pcpu_id == BOOT_CPU_ID) {
		/* Print Hypervisor Banner */
		print_hv_banner();

		/* Calibrate TSC Frequency */
		calibrate_tsc();

		pr_acrnlog("HV version %s-%s-%s %s (daily tag:%s) build by %s%s, start time %luus", HV_FULL_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE, HV_DAILY_TAG, HV_BUILD_USER, HV_CONFIG_TOOL,
			ticks_to_us(start_tsc));

		pr_acrnlog("API version %u.%u", HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);

		pr_acrnlog("Detect processor: %s", (get_pcpu_info())->model_name);

		pr_dbg("Core %hu is up", BOOT_CPU_ID);

		/* Warn for security feature not ready */
		if (!check_cpu_security_cap()) {
			pr_fatal("SECURITY WARNING!!!!!!");
			pr_fatal("Please apply the latest CPU uCode patch!");
		}

		/* Initialize interrupts */
		init_interrupt(BOOT_CPU_ID);

		setup_notification();
		setup_posted_intr_notification();

		init_iommu();

		/* Start all secondary cores */
		startup_paddr = prepare_trampoline();
		if (!start_pcpus(AP_MASK)) {
			panic("Failed to start all secondary cores!");
		}

		ASSERT(get_pcpu_id() == BOOT_CPU_ID, "");
	} else {
		pr_dbg("Core %hu is up", pcpu_id);

		/* Initialize secondary processor interrupts. */
		init_interrupt(pcpu_id);

		/* Wait for boot processor to signal all secondary cores to continue */
		wait_sync_change(&pcpu_sync, 0UL);
	}

	init_sched(pcpu_id);

	enable_smep();

	enable_smap();
}

static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id)
{
	uint16_t i;
	uint16_t pcpu_id = INVALID_CPU_ID;

	for (i = 0U; i < phys_cpu_num; i++) {
		if (per_cpu(lapic_id, i) == lapic_id) {
			pcpu_id = i;
			break;
		}
	}

	return pcpu_id;
}

static void start_pcpu(uint16_t pcpu_id)
{
	uint32_t timeout;

	/* Update the stack for pcpu */
	stac();
	write_trampoline_stack_sym(pcpu_id);
	clac();

	send_startup_ipi(INTR_CPU_STARTUP_USE_DEST, pcpu_id, startup_paddr);

	/* Wait until the pcpu with pcpu_id is running and set the active bitmap or
	 * configured time-out has expired
	 */
	timeout = CPU_UP_TIMEOUT * 1000U;
	while (!is_pcpu_active(pcpu_id) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	/* Check to see if expected CPU is actually up */
	if (!is_pcpu_active(pcpu_id)) {
		pr_fatal("Secondary CPU%hu failed to come up", pcpu_id);
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
	}
}

/**
 * @brief Start all cpus if the bit is set in mask except itself
 *
 * @param[in] mask bits mask of cpus which should be started
 *
 * @return true if all cpus set in mask are started
 * @return false if there are any cpus set in mask aren't started
 */
bool start_pcpus(uint64_t mask)
{
	uint16_t i;
	uint16_t pcpu_id = get_pcpu_id();
	uint64_t expected_start_mask = mask;

	/* secondary cpu start up will wait for pcpu_sync -> 0UL */
	pcpu_sync = 1UL;
	cpu_write_memory_barrier();

	i = ffs64(expected_start_mask);
	while (i != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(i, &expected_start_mask);

		if (pcpu_id == i) {
			continue; /* Avoid start itself */
		}

		start_pcpu(i);
		i = ffs64(expected_start_mask);
	}

	/* Trigger event to allow secondary CPUs to continue */
	pcpu_sync = 0UL;

	return ((pcpu_active_bitmap & mask) == mask);
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

bool need_offline(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
}

void wait_pcpus_offline(uint64_t mask)
{
	uint32_t timeout;

	timeout = CPU_DOWN_TIMEOUT * 1000U;
	while (((pcpu_active_bitmap & mask) != 0UL) && (timeout != 0U)) {
		udelay(10U);
		timeout -= 10U;
	}
}

void cpu_do_idle(void)
{
	asm_pause();
}

/**
 * only run on current pcpu
 */
void cpu_dead(void)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int32_t halt = 1;
	uint16_t pcpu_id = get_pcpu_id();

	deinit_sched(pcpu_id);
	if (bitmap_test(pcpu_id, &pcpu_active_bitmap)) {
		/* clean up native stuff */
		vmx_off();
		cache_flush_invalidate_all();

		/* Set state to show CPU is dead */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
		bitmap_clear_lock(pcpu_id, &pcpu_active_bitmap);

		/* Halt the CPU */
		do {
			asm_hlt();
		} while (halt != 0);
	} else {
		pr_err("pcpu%hu already dead", pcpu_id);
	}
}

static void set_current_pcpu_id(uint16_t pcpu_id)
{
	/* Write TSC AUX register */
	msr_write(MSR_IA32_TSC_AUX, (uint64_t)pcpu_id);
}

static void print_hv_banner(void)
{
	const char *boot_msg = "ACRN Hypervisor\n\r";

	/* Print the boot message */
	printf(boot_msg);
}

static inline void asm_monitor(volatile const uint64_t *addr, uint64_t ecx, uint64_t edx)
{
	asm volatile("monitor\n" : : "a"(addr), "c"(ecx), "d"(edx));
}

static inline void asm_mwait(uint64_t eax, uint64_t ecx)
{
	asm volatile("mwait\n" : : "a"(eax), "c"(ecx));
}

/* wait until *sync == wake_sync */
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync)
{
	if (has_monitor_cap()) {
		/* Wait for the event to be set using monitor/mwait */
		while ((*sync) != wake_sync) {
			asm_monitor(sync, 0UL, 0UL);
			if ((*sync) != wake_sync) {
				asm_mwait(0UL, 0UL);
			}
		}
	} else {
		while ((*sync) != wake_sync) {
			asm_pause();
		}
	}
}

static void init_pcpu_xsave(void)
{
	uint64_t val64;
	struct cpuinfo_x86 *cpu_info;
	uint32_t eax, ecx, unused, xsave_area_size;

	CPU_CR_READ(cr4, &val64);
	val64 |= CR4_OSXSAVE;
	CPU_CR_WRITE(cr4, val64);

	if (get_pcpu_id() == BOOT_CPU_ID) {
		cpuid(CPUID_FEATURES, &unused, &unused, &ecx, &unused);

		/* if set, update it */
		if ((ecx & CPUID_ECX_OSXSAVE) != 0U) {
			cpu_info = get_pcpu_info();
			cpu_info->cpuid_leaves[FEAT_1_ECX] |= CPUID_ECX_OSXSAVE;

			write_xcr(0, XCR0_INIT);
			msr_write(MSR_IA32_XSS, XSS_INIT);

			/* get xsave area size, containing all the state components
			 * corresponding to bits currently set in XCR0 | IA32_XSS */
			cpuid_subleaf(CPUID_XSAVE_FEATURES, 1U, &eax, &xsave_area_size, &ecx, &unused);
			if (xsave_area_size > XSAVE_STATE_AREA_SIZE) {
				panic("XSAVE area (%d bytes) exceeds the pre-allocated 4K region\n", xsave_area_size);
			}
		}
	}
}

/**
 * @}
 */
