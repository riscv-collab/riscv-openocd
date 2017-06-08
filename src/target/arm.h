/*
 * Copyright (C) 2005 by Dominic Rath
 * Dominic.Rath@gmx.de
 *
 * Copyright (C) 2008 by Spencer Oliver
 * spen@spen-soft.co.uk
 *
 * Copyright (C) 2009 by Øyvind Harboe
 * oyvind.harboe@zylin.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENOCD_TARGET_ARM_H
#define OPENOCD_TARGET_ARM_H

#include <helper/command.h>
#include "target.h"


/**
 * @file
 * Holds the interface to ARM cores.
 *
 * At this writing, only "classic ARM" cores built on the ARMv4 register
 * and mode model are supported.  The Thumb2-only microcontroller profile
 * support has not yet been integrated, affecting Cortex-M parts.
 */

/**
 * Represent state of an ARM core.
 *
 * Most numbers match the five low bits of the *PSR registers on
 * "classic ARM" processors, which build on the ARMv4 processor
 * modes and register set.
 *
 * ARM_MODE_ANY is a magic value, often used as a wildcard.
 *
 * Only the microcontroller cores (ARMv6-M, ARMv7-M) support ARM_MODE_THREAD,
 * ARM_MODE_USER_THREAD, and ARM_MODE_HANDLER.  Those are the only modes
 * they support.
 */
enum arm_mode {
	ARM_MODE_USR = 16,
	ARM_MODE_FIQ = 17,
	ARM_MODE_IRQ = 18,
	ARM_MODE_SVC = 19,
	ARM_MODE_MON = 22,
	ARM_MODE_ABT = 23,
	ARM_MODE_UND = 27,
	ARM_MODE_1176_MON = 28,
	ARM_MODE_SYS = 31,

	ARM_MODE_THREAD = 0,
	ARM_MODE_USER_THREAD = 1,
	ARM_MODE_HANDLER = 2,

	/* shift left 4 bits for armv8 64 */
	ARMV8_64_EL0T = 0x0F,
	ARMV8_64_EL1T = 0x4F,
	ARMV8_64_EL1H = 0x5F,
	ARMV8_64_EL2T = 0x8F,
	ARMV8_64_EL2H = 0x9F,
	ARMV8_64_EL3T = 0xCF,
	ARMV8_64_EL3H = 0xDF,

	ARM_MODE_ANY = -1
};

const char *arm_mode_name(unsigned psr_mode);
bool is_arm_mode(unsigned psr_mode);

/** The PSR "T" and "J" bits define the mode of "classic ARM" cores. */
enum arm_state {
	ARM_STATE_ARM,
	ARM_STATE_THUMB,
	ARM_STATE_JAZELLE,
	ARM_STATE_THUMB_EE,
	ARM_STATE_AARCH64,
};

#define ARM_COMMON_MAGIC 0x0A450A45

/**
 * Represents a generic ARM core, with standard application registers.
 *
 * There are sixteen application registers (including PC, SP, LR) and a PSR.
 * Cortex-M series cores do not support as many core states or shadowed
 * registers as traditional ARM cores, and only support Thumb2 instructions.
 */
struct arm {
	int common_magic;
	struct reg_cache *core_cache;

	/** Handle to the PC; valid in all core modes. */
	struct reg *pc;

	/** Handle to the CPSR/xPSR; valid in all core modes. */
	struct reg *cpsr;

	/** Handle to the SPSR; valid only in core modes with an SPSR. */
	struct reg *spsr;

	/** Support for arm_reg_current() */
	const int *map;

	/**
	 * Indicates what registers are in the ARM state core register set.
	 * ARM_MODE_ANY indicates the standard set of 37 registers,
	 * seen on for example ARM7TDMI cores.  ARM_MODE_MON indicates three
	 * more registers are shadowed, for "Secure Monitor" mode.
	 * ARM_MODE_THREAD indicates a microcontroller profile core,
	 * which only shadows SP.
	 */
	enum arm_mode core_type;

	/** Record the current core mode: SVC, USR, or some other mode. */
	enum arm_mode core_mode;

	/** Record the current core state: ARM, Thumb, or otherwise. */
	enum arm_state core_state;

	/** Flag reporting unavailability of the BKPT instruction. */
	bool is_armv4;

	/** Flag reporting armv6m based core. */
	bool is_armv6m;

	/** Flag reporting whether semihosting is active. */
	bool is_semihosting;

	/** Flag reporting whether semihosting fileio is active. */
	bool is_semihosting_fileio;

	/** Flag reporting whether semihosting fileio operation is active. */
	bool semihosting_hit_fileio;

	/** Current semihosting operation. */
	int semihosting_op;

	/** Current semihosting result. */
	int semihosting_result;

	/** Value to be returned by semihosting SYS_ERRNO request. */
	int semihosting_errno;

	int (*setup_semihosting)(struct target *target, int enable);

	/** Backpointer to the target. */
	struct target *target;

	/** Handle for the debug module, if one is present. */
	struct arm_dpm *dpm;

	/** Handle for the Embedded Trace Module, if one is present. */
	struct etm_context *etm;

	/* FIXME all these methods should take "struct arm *" not target */

	/** Retrieve all core registers, for display. */
	int (*full_context)(struct target *target);

	/** Retrieve a single core register. */
	int (*read_core_reg)(struct target *target, struct reg *reg,
			int num, enum arm_mode mode);
	int (*write_core_reg)(struct target *target, struct reg *reg,
			int num, enum arm_mode mode, uint8_t *value);

	/** Read coprocessor register.  */
	int (*mrc)(struct target *target, int cpnum,
			uint32_t op1, uint32_t op2,
			uint32_t CRn, uint32_t CRm,
			uint32_t *value);

	/** Write coprocessor register.  */
	int (*mcr)(struct target *target, int cpnum,
			uint32_t op1, uint32_t op2,
			uint32_t CRn, uint32_t CRm,
			uint32_t value);

	void *arch_info;

	/** For targets conforming to ARM Debug Interface v5,
	 * this handle references the Debug Access Port (DAP)
	 * used to make requests to the target.
	 */
	struct adiv5_dap *dap;
};

/** Convert target handle to generic ARM target state handle. */
static inline struct arm *target_to_arm(struct target *target)
{
	assert(target != NULL);
	return target->arch_info;
}

static inline bool is_arm(struct arm *arm)
{
	assert(arm != NULL);
	return arm->common_magic == ARM_COMMON_MAGIC;
}

struct arm_algorithm {
	int common_magic;

	enum arm_mode core_mode;
	enum arm_state core_state;
};

struct arm_reg {
	int num;
	enum arm_mode mode;
	struct target *target;
	struct arm *arm;
	uint8_t value[8];
};

struct reg_cache *arm_build_reg_cache(struct target *target, struct arm *arm);
struct reg_cache *armv8_build_reg_cache(struct target *target);

extern const struct command_registration arm_command_handlers[];

int arm_arch_state(struct target *target);
int arm_get_gdb_reg_list(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class);
int armv8_get_gdb_reg_list(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class);

int arm_init_arch_info(struct target *target, struct arm *arm);

/* REVISIT rename this once it's usable by ARMv7-M */
int armv4_5_run_algorithm(struct target *target,
		int num_mem_params, struct mem_param *mem_params,
		int num_reg_params, struct reg_param *reg_params,
		target_addr_t entry_point, target_addr_t exit_point,
		int timeout_ms, void *arch_info);
int armv4_5_run_algorithm_inner(struct target *target,
		int num_mem_params, struct mem_param *mem_params,
		int num_reg_params, struct reg_param *reg_params,
		uint32_t entry_point, uint32_t exit_point,
		int timeout_ms, void *arch_info,
		int (*run_it)(struct target *target, uint32_t exit_point,
				int timeout_ms, void *arch_info));

int arm_checksum_memory(struct target *target,
		target_addr_t address, uint32_t count, uint32_t *checksum);
int arm_blank_check_memory(struct target *target,
		target_addr_t address, uint32_t count, uint32_t *blank, uint8_t erased_value);

void arm_set_cpsr(struct arm *arm, uint32_t cpsr);
struct reg *arm_reg_current(struct arm *arm, unsigned regnum);
struct reg *armv8_reg_current(struct arm *arm, unsigned regnum);

extern struct reg arm_gdb_dummy_fp_reg;
extern struct reg arm_gdb_dummy_fps_reg;

#endif /* OPENOCD_TARGET_ARM_H */
