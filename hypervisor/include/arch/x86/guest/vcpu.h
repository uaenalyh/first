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
 * The macro set seg.selector to encoding SSG_NAME\#\# SEL, seg.base to encoding SEG_NAME\#\# _BASE, seg.limit
 * to encoding SEG_NAME\#\# _LIMIT and set seg.attr to encoding SEG_NAME\#\# _ATTR of the VMCS fields.
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
 * @ThreadSafety Yes
 */
#define load_segment(seg, SEG_NAME)                     \
	{                                                   \
		exec_vmwrite16(SEG_NAME##_SEL, (seg).selector); \
		exec_vmwrite(SEG_NAME##_BASE, (seg).base);      \
		exec_vmwrite32(SEG_NAME##_LIMIT, (seg).limit);  \
		exec_vmwrite32(SEG_NAME##_ATTR, (seg).attr);    \
	}

#define REAL_MODE_BSP_INIT_CODE_SEL (0xf000U)    /**< code segment selector of real mode operation */
#define REAL_MODE_DATA_SEG_AR       (0x0093U)    /**< data segment attributes of real mode operation */
#define REAL_MODE_CODE_SEG_AR       (0x009fU)    /**< code segment attributes of real mode operation */
#define PROTECTED_MODE_DATA_SEG_AR  (0xc093U)    /**< data segment attributes of protected mode operation */
#define PROTECTED_MODE_CODE_SEG_AR  (0xc09bU)    /**< code segment attributes of protected mode operation */
#define REAL_MODE_SEG_LIMIT         (0xffffU)    /**< segment limit of real mode operation */
#define PROTECTED_MODE_SEG_LIMIT    (0xffffffffU)/**< segment limit of protected mode operation */
#define DR7_INIT_VALUE              (0x400UL)    /**< initial value of DR7 */
#define LDTR_AR                     (0x0082U)    /**< attributes of LDTR */
#define TR_AR                       (0x008bU)    /**< attributes of TR */

/**
 * @brief This macro expands to a for-loop statement with one if statement as loop body.
 *
 * The for-loop iterates over the vCPUs of \a vm in ascending order of the vcpu_id field in each vcpu.
 * For each idx ranging from 0 to vm->hw.created_vcpus [with a step of 1]. It is used to probe each
 * vCPU in the given VM.
 *
 * @param[out] idx The loop counter to be used
 * @param[in] vm A pointer to the VM structure whose vCPUs will be iterated
 * @param[out] vcpu A pointer to the iterated vCPU structure
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
 #define foreach_vcpu(idx, vm, vcpu)                                                         \
	for ((idx) = 0U, (vcpu) = &((vm)->hw.vcpu_array[(idx)]); (idx) < (vm)->hw.created_vcpus; \
		(idx)++, (vcpu) = &((vm)->hw.vcpu_array[(idx)]))                                     \
		if (vcpu->state != VCPU_OFFLINE)

/**
 * @brief This enum is used to define vcpu state.
 *
 */
enum vcpu_state {
	VCPU_INIT,          /**< vCPU under initialization */
	VCPU_RUNNING,       /**< vCPU launched vCPU under initialization */
	VCPU_PAUSED,        /**< vCPU temporarily paused but can be resumed later */
	VCPU_ZOMBIE,        /**< vCPU stopped and wait for being deinitialized */
	VCPU_OFFLINE,       /**< vCPU deinitialized vCPU under initialization */
	VCPU_UNKNOWN_STATE, /**< non-defined vCPU state */
};

/**
 * @brief This enum is used to define the cpu mode of specified vm
 *
 */
enum vm_cpu_mode {
	CPU_MODE_REAL,          /**< CPU real-address mode of operation vCPU under initialization */
	CPU_MODE_PROTECTED,     /**< CPU protected or virtual 8086 mode of operation */
	CPU_MODE_COMPATIBILITY, /**< CPU IA-32e compatibility mode of operation */
	CPU_MODE_64BIT,         /**< CPU IA-32e 64-bit mode of operation */
};

/**
 * @brief The number of MSRs different between normal world and secure world.
 */
#define NUM_WORLD_MSRS  2U
/**
 * @brief The number of common MSRs.
 */
#define NUM_COMMON_MSRS 15U
/**
 * @brief The number of guest MSRs.
 */
#define NUM_GUEST_MSRS  (NUM_WORLD_MSRS + NUM_COMMON_MSRS)

/**
 * @brief This structure is used to store segment register.
 *
 * @consistency None
 *
 * @alignment None
 *
 * @remark None
 */
struct segment_sel {
	uint16_t selector;    /**< segment selector of the segment register */
	uint64_t base;        /**< base address of the segment register */
	uint32_t limit;       /**< limit of the segment register */
	uint32_t attr;        /**< attribute of the segment register */
};

/**
 * @brief This structure is used to store vcpu run context.
 *
 * @consistency None
 *
 * @alignment 16
 *
 * @remark None
 */
struct run_context {
	/**
	 * @brief This union to store the guest general purpose registers.
	 *
	 * This must be the first element in the structure, so that the offsets
	 * in vmx_asm.S match
	 */
	union cpu_regs_t {
		struct acrn_gp_regs regs; /**< the structure to store vcpu general purpose registers. */
		uint64_t longs[NUM_GPRS]; /**< the array to store vcpu general purpose registers. */
	} cpu_regs;

	uint64_t cr0;      /**< guest CR register 0 */

	uint64_t cr2;      /**< guest CR register 2 */
	uint64_t cr4;      /**< guest CR register 4 */

	uint64_t rip;      /**< guest RIP */
	uint64_t rflags;   /**< guest RFLAGS */

	uint64_t ia32_spec_ctrl;   /**< guest IA32_SPEC_CTRL MSR */
	uint64_t ia32_efer;        /**< guest EFER */
};

/**
 * @brief This union represents an XSAVE area where attributes indicating
 * the valid contents and layout of an XSS area are stored.
 *
 * @consistency None
 *
 * @alignment None
 *
 * @remark None
 */
union xsave_header {
	/**
	 * @brief An array representing all the raw contents in an XSS header.
	 */
	uint64_t value[XSAVE_HEADER_AREA_SIZE / sizeof(uint64_t)];
	/**
	 * @brief A structure that helps decoding specific fields in an XSS header.
	 */
	struct {
		/**
		 * @brief The XSTATE_BV field in the XSAVE header which identifies the state
		 * components that are stored in an XSAVE area.
		 */
		uint64_t xstate_bv;
		/**
		 * @brief The XCOMP_BV field in the XSAVE header which specifies whether the
		 * state components are stored in standard or compact format.
		 */
		uint64_t xcomp_bv;
	} hdr;
};

/**
 * @brief This structure is used to store register values of defined state components.
 *
 * @consistency None
 *
 * @alignment 64
 *
 * @remark None
 */
struct xsave_area {
	/**
	 * @brief The 512 byte legacy region in an XSAVE area.
	 */
	uint64_t legacy_region[XSAVE_LEGACY_AREA_SIZE / sizeof(uint64_t)];
	/**
	 * @brief The XSAVE header.
	 */
	union xsave_header xsave_hdr;
	/**
	 * @brief the extended region of an XSAVE area.
	 */
	uint64_t extend_region[XSAVE_EXTEND_AREA_SIZE / sizeof(uint64_t)];
} __aligned(64);

/**
 * @brief This structure is used to store extended contexts that are not
 * saved/restored during vm exit/entry.
 *
 * @consistency None
 *
 * @alignment None
 *
 * @remark None
 */
struct ext_context {
	uint64_t cr3; /**< guest CR3 */

	struct segment_sel idtr; /**< guest interrupt description table register */
	struct segment_sel ldtr; /**< guest local description table register */
	struct segment_sel gdtr; /**< guest global description table register */
	struct segment_sel tr;   /**< guest task register */
	struct segment_sel cs;   /**< guest code segment register */
	struct segment_sel ss;   /**< guest stack segment register */
	struct segment_sel ds;   /**< guest data segment register */
	struct segment_sel es;   /**< guest ES register */
	struct segment_sel fs;   /**< guest FS register */
	struct segment_sel gs;   /**< guest GS register */

	uint64_t ia32_star;      /**< guest IA32_STAR MSR */
	uint64_t ia32_lstar;     /**< guest IA32_LSTAR MSR */
	uint64_t ia32_fmask;     /**< guest IA32_FMASK MSR */
	uint64_t ia32_kernel_gs_base;  /**< guest IA32_KERNEL_GS_BASE  MSR */
	/**
	 * @brief An XSAVE area saving contents in the guest state components
	 * that are enabled by the vCPU.
	 */
	struct xsave_area xs_area;
	uint64_t xcr0; /**< guest XCR0 */
	uint64_t xss;  /**< guest IA32_XSS MSR */
};

/**
 * @brief This structure is used to store vcpu run context and extended context.
 *
 * @consistency None
 *
 * @alignment None
 *
 * @remark None
 */
struct guest_cpu_context {
	struct run_context run_ctx;  /**< structure which records vcpu run_context */
	struct ext_context ext_ctx;  /**< structure which records vcpu extend context */
};

/**
 * @brief An entry in the VM-entry MSR-load area, VM-exit MSR-load area or VM-exit MSR store area
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

/**
 * @brief This enum is used to define the MSR area.
 *
 */
enum {
	MSR_AREA_TSC_AUX = 0, /**< Indicate MSR area of MSR_IA32_TSC_AUX */
	MSR_AREA_COUNT,       /**< The number of MSR area */
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
	/**
	 * @brief VM-entry MSR-load area which also acts as the VM-exit MSR-store area.
	 */
	struct msr_store_entry guest[MSR_AREA_COUNT];
	struct msr_store_entry host[MSR_AREA_COUNT];  /**< VM-exit MSR-load area */
};

/**
 * @brief This structure is used to store vcpu arch information for each vcpu.
 *
 * The detailed arch information includes vmcs pagesize, msr bitmap pagesize, vlapic structure,
 * current context, cpu_context structures, guest msr array, vpid, exception_info structure,
 * irq_window_enabled flag, nrexit, vcpu context information which including exit_reason,
 * idt_vector, exit_qualification and inst_len, cpu_mode information, nr_sipi, interrupt injection
 * information, msr_store_area structure and eoi_exit_bitmap .
 *
 * @consistency None
 *
 * @alignment 4096
 *
 * @remark None
 */
struct acrn_vcpu_arch {
	/**
	 * @brief vmcs region for this vcpu, MUST be 4KB-aligned.
	 */
	uint8_t vmcs[PAGE_SIZE];

	/**
	 * @brief MSR bitmap region for this vcpu, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[PAGE_SIZE];

	/**
	 * @brief per vcpu lapic */
	struct acrn_vlapic vlapic;

	/**
	 * @brief the structure records cpu_context information */
	struct guest_cpu_context context;

	/**
	 * @brief the array records guest msrs */
	uint64_t guest_msrs[NUM_GUEST_MSRS];

	/**
	 * @brief virtual processor identifier */
	uint16_t vpid;

	/**
	 * @brief the structure records exception raise and error numbers. */
	struct {
		/**
		 * @brief The number of the exception to raise. */
		uint32_t exception;

		/**
		 * @brief The error number for the exception. */
		uint32_t error;
	} exception_info;

	bool irq_window_enabled; /**< whether the irq window is enabled. */
	uint32_t nrexits;      /**< vmexit number */

	uint32_t exit_reason;        /**< vmexit number */
	uint32_t idt_vectoring_info; /**< idt vector information */
	uint64_t exit_qualification; /**< vmexit qualification information */
	uint32_t inst_len;           /**< length of the instruction which causes vmexit */

	enum vm_cpu_mode cpu_mode;   /**< mode of vcpu */
	uint8_t nr_sipi;             /**< number of SIPI */

	uint64_t pending_req;        /**< id of pending request */

	/**
	 *@brief List of MSRS to be stored and loaded on VM exits or VM entries.
	 */
	struct msr_store_area msr_area;

} __aligned(PAGE_SIZE);

struct acrn_vm;
/**
 * @brief This structure is used to store vm information.
 *
 * This detailed vm information includes stack information, vcpu_arch structure, vcpu
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
	/**
	 * @brief stack to store the context of the vCPU */
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);

	/**
	 * @brief Architecture specific definitions for this vCPU */
	struct acrn_vcpu_arch arch;
	/**
	 * @brief ID of the virtual CPU which is bound to pcpu identified by acrn_vcpu->pcpu_id */
	uint16_t vcpu_id;
	/**
	 * @brief ID of virtual machine which is bound to vcpu identified by acrn_vcpu->vcpu_id */
	struct acrn_vm *vm;

	/**
	 * @brief State of this vCPU before being paused */
	enum vcpu_state prev_state;
	enum vcpu_state state; /**< State of this vCPU */

	/**
	 * @brief structure which records schedule information */
	struct thread_object thread_obj;
	bool launched; /**< Whether the vcpu is launched on target pcpu */
	bool running; /**< vcpu is picked up and run? */

	struct io_request req; /**< io request structure */

	/**
	 * @brief bitmap indicating the registers whose values have been cached in the
	 * vCPU context structures. */
	uint64_t reg_cached;
	/**
	 * @brief bitmap indicating the registers whose values have been updated since
	 * the last VM exit. */
	uint64_t reg_updated;
} __aligned(PAGE_SIZE);

/**
 * @brief This function is used to judge if vcpu is bsp.
 *
 * @return A Boolean value indicating if the vCPU is bsp.
 * @retval true when vcpu->vcpu_id == BOOT_CPU_ID, Otherwise return false
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Yes
 */
static inline bool is_vcpu_bsp(const struct acrn_vcpu *vcpu)
{
	/** Return true when vcpu->vcpu_id == BOOT_CPU_ID. Otherwise return false */
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
 * @ThreadSafety Yes
 */
static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	/** Return vcpu->arch.cpu_mode */
	return vcpu->arch.cpu_mode;
}

/**
 * @brief This function is used to retain the vCPU rip.
 *
 * Upon VM entries, a vCPU by default executes the instruction next to the one triggering the
 * last VM exit event. This function can be called to force the vCPU re-execute the same
 * instruction for the next VM entry. This function is idempotent.
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
 * @ThreadSafety When \a vcpu is different among multiple invocation
 */
static inline void vcpu_retain_rip(struct acrn_vcpu *vcpu)
{
	/** Set length of instruction which causes vm exit to 0 */
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
 * @mode HV_OPERATIONAL, HV_SUBMODE_INIT_ROOT
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety When \a vcpu is different among parallel invocation
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
 * @brief This function is used to judge if the vcpu is in long mode .
 *
 * @return A Boolean value indicating if the vCPU is currently in long mode or not.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Yes
 */
static inline bool is_long_mode(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_get_efer() with vcpu being parameter, in order to
	 *  get value of EFER register of the target vCPU and return 1 if LMA bit of EFER
	 *  is true. Otherwise return false. */
	return (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) != 0UL;
}

/**
 * @brief This function is used to judge if paging is enabled .
 *
 * @return A Boolean value indicating if paging is enabled or not.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Yes
 */
static inline bool is_paging_enabled(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_get_cr0() with vcpu being parameter, in order to get CR0 value
	 *  of the target vCPU and return true if PG bit of CR0 is 1. Otherwise return false.
	 */
	return (vcpu_get_cr0(vcpu) & CR0_PG) != 0UL;
}

/**
 * @brief This function is used to judge if PAE is enabled .
 *
 * @return A Boolean value indicating if PAE is enabled or not.
 *
 * @pre vcpu != NULL
 *
 * @post None
 *
 * @mode HV_INIT, HV_OPERATIONAL
 *
 * @reentrancy Unspecified
 *
 * @ThreadSafety Yes
 */
static inline bool is_pae(struct acrn_vcpu *vcpu)
{
	/** Call vcpu_get_cr4() with vcpu being parameter, in order to get CR4 value
	 *  of the target vCPU and return true if PAE bit in CR4 is 1. Otherwise return false.
	 */
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
