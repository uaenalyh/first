/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vcpu.h
 *
 * @brief public APIs for vcpu operations
 */

#ifndef VCPU_H
#define VCPU_H

/**
 * @addtogroup vp-base_vcpu
 *
 * @{
 */

/**
 * @file
 * @brief this file declares the macros, data structure, internal functions and external APIs of the vcpu module.
 *
 */

#ifndef ASSEMBLER

#include <acrn_common.h>
#include <guest_memory.h>
#include <virtual_cr.h>
#include <vlapic.h>
#include <schedule.h>
#include <io_req.h>
#include <msr.h>
#include <cpu.h>

/**
 * @brief Request for exception injection
 */
#define ACRN_REQUEST_EXCP 0U

/**
 * @brief Request for non-maskable interrupt
 */
#define ACRN_REQUEST_NMI 3U

/**
 * @brief Request for EOI exit bitmap update
 */
#define ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE 4U

/**
 * @brief Request for EPT flush
 */
#define ACRN_REQUEST_EPT_FLUSH 5U

/**
 * @brief Request for triple fault
 */
#define ACRN_REQUEST_TRP_FAULT 6U

/**
 * @brief Request for VPID TLB flush
 */
#define ACRN_REQUEST_VPID_FLUSH 7U

/**
 * @brief Request for initilizing VMCS
 */
#define ACRN_REQUEST_INIT_VMCS 8U

/**
 * @brief Request for resetting LAPIC
 */
#define ACRN_REQUEST_LAPIC_RESET 9U

/**
 * @brief This macro is used to pre-define load segment function which set selector, base, limit and
 *  attribute of specified selector.
 *
 * @return None
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
#define load_segment(seg, SEG_NAME)                     \
	{                                                   \
		exec_vmwrite16(SEG_NAME##_SEL, (seg).selector); \
		exec_vmwrite(SEG_NAME##_BASE, (seg).base);      \
		exec_vmwrite32(SEG_NAME##_LIMIT, (seg).limit);  \
		exec_vmwrite32(SEG_NAME##_ATTR, (seg).attr);    \
	}

/** Define segments constants for guest */
#define REAL_MODE_BSP_INIT_CODE_SEL (0xf000U)    /** code segment selector of real mode operation*/
#define REAL_MODE_DATA_SEG_AR       (0x0093U)    /** data segment attributes of real mode operation*/
#define REAL_MODE_CODE_SEG_AR       (0x009fU)    /** code segment attributes of real mode operation*/
#define PROTECTED_MODE_DATA_SEG_AR  (0xc093U)    /** data segment attributes of protected mode operation*/
#define PROTECTED_MODE_CODE_SEG_AR  (0xc09bU)    /** code segment attributes of protected mode operation*/
#define REAL_MODE_SEG_LIMIT         (0xffffU)    /** segment limit of real mode operation*/
#define PROTECTED_MODE_SEG_LIMIT    (0xffffffffU)/** segment limit of protected mode operation*/
#define DR7_INIT_VALUE              (0x400UL)    /** initial value of DR7*/
#define LDTR_AR                     (0x0082U)    /** attributes of LDTR */
#define TR_AR                       (0x008bU)    /** attributes of TR */

/**
 * @brief This macro expands to a for-loop statement with one if statement as loop body. The for-loop
 * iterates over the vCPUs of @vm in ascending order of the vcpu_id field in each vcpu.
 * For each idx ranging from 0 to vm->hw.created_vcpus [with a step of 1]. It is used to probe each
 * vCPU in the given VM.
 * @return None
 *
 * @pre  None
 *
 * @post None
 *
 * @mode HV_SUBMODE_INIT_ROOT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
 #define foreach_vcpu(idx, vm, vcpu)                                                         \
	for ((idx) = 0U, (vcpu) = &((vm)->hw.vcpu_array[(idx)]); (idx) < (vm)->hw.created_vcpus; \
		(idx)++, (vcpu) = &((vm)->hw.vcpu_array[(idx)]))                                     \
		if (vcpu->state != VCPU_OFFLINE)

/** This enum is used to define vcpu state */
enum vcpu_state {
	VCPU_INIT,          /** vCPU under initialization */
	VCPU_RUNNING,       /** vCPU launched vCPU under initialization */
	VCPU_PAUSED,        /** vCPU temporarily paused but can be resumed later */
	VCPU_ZOMBIE,        /** vCPU stopped and wait for being deinitialized */
	VCPU_OFFLINE,       /** vCPU deinitialized vCPU under initialization */
	VCPU_UNKNOWN_STATE, /** non-defined vCPU state */
};

/** This enum is used to define the cpu mode of specified vm */
enum vm_cpu_mode {
	CPU_MODE_REAL,          /** CPU real-address mode of operation vCPU under initialization */
	CPU_MODE_PROTECTED,     /** CPU protected or virtual 8086 mode of operation */
	CPU_MODE_COMPATIBILITY, /* CPU IA-32e compatibility mode of operation */
	CPU_MODE_64BIT,         /* CPU IA-32e 64-bit mode of operation */
};

#define NUM_WORLD_MSRS  2U
#define NUM_COMMON_MSRS 15U
#define NUM_GUEST_MSRS  (NUM_WORLD_MSRS + NUM_COMMON_MSRS)

struct segment_sel {
	uint16_t selector;
	uint64_t base;
	uint32_t limit;
	uint32_t attr;
};

/**
 * @brief registers info saved for vcpu running context
 */
struct run_context {
	/* Contains the guest register set.
	 * NOTE: This must be the first element in the structure, so that the offsets
	 * in vmx_asm.S match
	 */
	union cpu_regs_t {
		struct acrn_gp_regs regs;
		uint64_t longs[NUM_GPRS];
	} cpu_regs;

	/** The guests CR registers 0, 2, 3 and 4. */
	uint64_t cr0;

	/* CPU_CONTEXT_OFFSET_CR2 =
	 * offsetof(struct run_context, cr2) = 136
	 */
	uint64_t cr2;
	uint64_t cr4;

	uint64_t rip;
	uint64_t rflags;

	/* CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL =
	 * offsetof(struct run_context, ia32_spec_ctrl) = 168
	 */
	uint64_t ia32_spec_ctrl;
	uint64_t ia32_efer;
};

union xsave_header {
	uint64_t value[XSAVE_HEADER_AREA_SIZE / sizeof(uint64_t)];
	struct {
		/* bytes 7:0 */
		uint64_t xstate_bv;
		/* bytes 15:8 */
		uint64_t xcomp_bv;
	} hdr;
};

struct xsave_area {
	uint64_t legacy_region[XSAVE_LEGACY_AREA_SIZE / sizeof(uint64_t)];
	union xsave_header xsave_hdr;
	uint64_t extend_region[XSAVE_EXTEND_AREA_SIZE / sizeof(uint64_t)];
} __aligned(64);

/*
 * extended context does not save/restore during vm exit/entry, it's mainly
 * used in trusty world switch
 */
struct ext_context {
	uint64_t cr3;

	/* segment registers */
	struct segment_sel idtr;
	struct segment_sel ldtr;
	struct segment_sel gdtr;
	struct segment_sel tr;
	struct segment_sel cs;
	struct segment_sel ss;
	struct segment_sel ds;
	struct segment_sel es;
	struct segment_sel fs;
	struct segment_sel gs;

	uint64_t ia32_star;
	uint64_t ia32_lstar;
	uint64_t ia32_fmask;
	uint64_t ia32_kernel_gs_base;

	struct xsave_area xs_area;
	uint64_t xcr0;
	uint64_t xss;
};

/**
 * @brief This structure is used to store cpu run context and extended context.
 *
 * @consistency None
 *
 * @remark None
 */
struct guest_cpu_context {
	struct run_context run_ctx;  /** structure which records vcpu run_context */
	struct ext_context ext_ctx;  /** structure which records vcpu extend context */
};

/* Intel SDM 24.8.2, the address must be 16-byte aligned */
/**
 * @brief An entry in the VM-entry MSR-load area, VM-exit MSR-load area or VM-exit MSRstore area
 *
 * @consistency None
 *
 * @alignment 16
 *
 * @remark None
 */
struct msr_store_entry {
	uint32_t msr_index;  /** Index of the MSR to be loaded */
	uint64_t value;      /** Value to be loaded into the specified MSR during VMentry */
} __aligned(16);

enum {
	MSR_AREA_TSC_AUX = 0, /** Indicate MSR area of MSR_IA32_TSC_AUX */
	MSR_AREA_COUNT,       /** The number of MSR area */
};

/**
 * @brief The MSR load/store areas used to automatically load/store certain MSRs during VM-entry and VM-exit.
 *
 * @consistency None
 *
 * @alignment 16
 *
 * @remark None
 */
struct msr_store_area {
	/* VM-entry MSR-load area which also acts as the VM-exit MSR-store area */
	struct msr_store_entry guest[MSR_AREA_COUNT];
	struct msr_store_entry host[MSR_AREA_COUNT];  /* VM-exit MSR-load area */
};

/**
 * @brief This structure is used to store vmcs pagesize, msr bitmap pagesize, vlapic structure,
 * current context, cpu_context structures, guest msr array, vpid, exception_info structure,
 * irq_window_enabled flag, nrexit, vcpu context information which including exit_reason,
 * idt_vector, exit_qualification and inst_len, cpu_mode information, nr_sipi, interrupt injection
 * information, msr_store_area structure and eoi_exit_bitmap for each vcpu.
 *
 * @consistency None
 *
 * @alignment 4096
 *
 * @remark None
 */
struct acrn_vcpu_arch {
	/** vmcs region for this vcpu, MUST be 4KB-aligned */
	uint8_t vmcs[PAGE_SIZE];

	/** MSR bitmap region for this vcpu, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[PAGE_SIZE];

	/** per vcpu lapic */
	struct acrn_vlapic vlapic;

	/** the structure records cpu_context information */
	struct guest_cpu_context context;

	/* the array records guest msrs */
	uint64_t guest_msrs[NUM_GUEST_MSRS];

	/* virtual processor identifier */
	uint16_t vpid;

	/* the structure records exception raise and error numbers. */
	struct {
		/** The number of the exception to raise. */
		uint32_t exception;

		/** The error number for the exception. */
		uint32_t error;
	} exception_info;

	bool irq_window_enabled; /** whether the irq window is enabled. */
	uint32_t nrexits;      /** vmexit number */

	/* VCPU context state information */
	uint32_t exit_reason;        /** vmexit number */
	uint32_t idt_vectoring_info; /** idt vector information */
	uint64_t exit_qualification; /** vmexit qualification information */
	uint32_t inst_len;           /** length of the instruction which causes vmexit */

	/* Information related to secondary / AP VCPU start-up */
	enum vm_cpu_mode cpu_mode;   /** mode of vcpu */
	uint8_t nr_sipi;             /** number of SIPI */

	/** interrupt injection information */
	uint64_t pending_req;        /** id of pending request */

	/* List of MSRS to be stored and loaded on VM exits or VM entries */
	struct msr_store_area msr_area;

} __aligned(PAGE_SIZE);

struct acrn_vm;
/**
 * @brief This structure is used to store stack information, vcpu_arch structure, vcpu
 * prev_state, vcpu current state, sched_object structure, launched flag, io_request
 * structure, register cached flag and register updated flag for each vcpu.
 *
 * @consistency For struct acrn_vcpu p and 0<=i<p.vm->hw.created_vcpus, there exists unique i,
 *              p.vm->hw.vcpu_array[i] == p and p.vcpu_id == i
 * @consistency For any struct acrn_vcpu p, p.arch.vlapic.vcpu=p
 *
 * @alignment 4096
 *
 * @remark None
 */
struct acrn_vcpu {
	/** stack to store the context of the vCPU */
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);

	/* Architecture specific definitions for this vCPU */
	struct acrn_vcpu_arch arch;
	/* ID of the virtual CPU which is bound to pcpu identified by acrn_vcpu->pcpu_id */
	uint16_t vcpu_id;
	/* ID of virtual machine which may contain one or more vcpus identified by acrn_vcpu->vcpu_id */
	struct acrn_vm *vm;

	/** State of this vCPU before suspend */
	enum vcpu_state prev_state;
	enum vcpu_state state; /** State of this vCPU */

	/** structure which records schedule informationd */
	struct thread_object thread_obj;
	bool launched; /** Whether the vcpu is launched on target pcpu */
	bool running; /** vcpu is picked up and run? */

	struct io_request req; /** io request structure */

	uint64_t reg_cached;   /** information which records if register is updated */
	uint64_t reg_updated;  /** information which records if register is cached */
} __aligned(PAGE_SIZE);

/**
 * @brief This function is used to judge if vcpu is bsp.
 *
 * @return true when vcpu->vcpu_id == BOOT_CPU_ID, else return false
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline bool is_vcpu_bsp(const struct acrn_vcpu *vcpu)
{
	/** Return true when vcpu->vcpu_id == BOOT_CPU_ID, else return false */
	return (vcpu->vcpu_id == BOOT_CPU_ID);
}

/**
 * @brief This function is used to get the vCPU mode.
 *
 * @return vcpu->arch.cpu_mode
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	/** Return vcpu->arch.cpu_mode */
	return vcpu->arch.cpu_mode;
}

/**
 * @brief This function is used to retain the vCPU rip.
 * do not update Guest RIP for next VM Enter
 *
 * @return vcpu->arch.cpu_mode
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline void vcpu_retain_rip(struct acrn_vcpu *vcpu)
{
	/** Set length of instruction which caused vm exit to 0 */
	(vcpu)->arch.inst_len = 0U;
}

/**
 * @brief This function is used to return the vLAPIC structure of a vCPU.
 *
 * @return &(vcpu->arch.vlapic)
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline struct acrn_vlapic *vcpu_vlapic(struct acrn_vcpu *vcpu)
{
	/** Return address of the vlapic */
	return &(vcpu->arch.vlapic);
}

uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu);
void default_idle(__unused struct thread_object *obj);
void vcpu_thread(struct thread_object *obj);

int32_t vmx_vmrun(struct run_context *context, int32_t ops);

/* External Interfaces */

uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg);

void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val);

uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu);

void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val);

void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val);

uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu);

void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val);

uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu);

void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val);

uint64_t vcpu_get_guest_msr(const struct acrn_vcpu *vcpu, uint32_t msr);

void vcpu_set_guest_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val);

void set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs);

void reset_vcpu_regs(struct acrn_vcpu *vcpu);

void init_vcpu_protect_mode_regs(struct acrn_vcpu *vcpu, uint64_t vgdt_base_gpa);

void set_vcpu_startup_entry(struct acrn_vcpu *vcpu, uint64_t entry);

/**
 * @brief This function is used to judge if the vcpu is long mode .
 *
 * @return judgement result
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline bool is_long_mode(struct acrn_vcpu *vcpu)
{
	/** Return 1 if LMA bit of EFER in the target vCPU is 1, else return 0 */
	return (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) != 0UL;
}

/**
 * @brief This function is used to judge if paging is enabled .
 *
 * @return judgement result
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline bool is_paging_enabled(struct acrn_vcpu *vcpu)
{
	/** Return 1 if PG bit of CR0 in the target vCPU is 1, else return 0 */
	return (vcpu_get_cr0(vcpu) & CR0_PG) != 0UL;
}

/**
 * @brief This function is used to judge if pae is enabled .
 *
 * @return judgement result
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Unspecified
 */
static inline bool is_pae(struct acrn_vcpu *vcpu)
{
	/** Return 1 if PAE bit of CR4 in the target vCPU is 1, else return 0 */
	return (vcpu_get_cr4(vcpu) & CR4_PAE) != 0UL;
}

void save_xsave_area(struct ext_context *ectx);
void rstore_xsave_area(const struct ext_context *ectx);

int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle);

int32_t run_vcpu(struct acrn_vcpu *vcpu);

void offline_vcpu(struct acrn_vcpu *vcpu);

void reset_vcpu(struct acrn_vcpu *vcpu);

void pause_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state);

void launch_vcpu(struct acrn_vcpu *vcpu);

void kick_vcpu(const struct acrn_vcpu *vcpu);

int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id);

uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask);

#endif /* ASSEMBLER */

/**
 * @}
 */

#endif /* VCPU_H */
