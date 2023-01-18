/* SPDX-License-Identifier: GPL-2.0-or-later */

/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 ***************************************************************************/

#ifndef OPENOCD_TARGET_BREAKPOINTS_H
#define OPENOCD_TARGET_BREAKPOINTS_H

#include <stdint.h>

#include "helper/types.h"

struct target;

enum breakpoint_type {
	BKPT_HARD,
	BKPT_SOFT,
};

enum watchpoint_rw {
	WPT_READ = 0, WPT_WRITE = 1, WPT_ACCESS = 2
};

struct breakpoint {
	target_addr_t address;
	uint32_t asid;
	int length;
	enum breakpoint_type type;
	bool is_set;
	unsigned int number;
	uint8_t *orig_instr;
	struct breakpoint *next;
	uint32_t unique_id;
	int linked_brp;
};

struct watchpoint {
	target_addr_t address;
	uint32_t length;
	uint32_t mask;
	uint32_t value;
	enum watchpoint_rw rw;
	bool is_set;
	unsigned int number;
	struct watchpoint *next;
	uint32_t unique_id;
};

void breakpoint_clear_target(struct target *target);
int breakpoint_add(struct target *target,
		target_addr_t address, uint32_t length, enum breakpoint_type type);
int context_breakpoint_add(struct target *target,
		uint32_t asid, uint32_t length, enum breakpoint_type type);
int hybrid_breakpoint_add(struct target *target,
		target_addr_t address, uint32_t asid, uint32_t length, enum breakpoint_type type);
void breakpoint_remove(struct target *target, target_addr_t address);
void breakpoint_remove_all(struct target *target);

struct breakpoint *breakpoint_find(struct target *target, target_addr_t address);

static inline void breakpoint_hw_set(struct breakpoint *breakpoint, unsigned int hw_number)
{
	breakpoint->is_set = true;
	breakpoint->number = hw_number;
}

void watchpoint_clear_target(struct target *target);
int watchpoint_add(struct target *target,
		target_addr_t address, uint32_t length,
		enum watchpoint_rw rw, uint32_t value, uint32_t mask);
void watchpoint_remove(struct target *target, target_addr_t address);

/* report type and address of just hit watchpoint */
int watchpoint_hit(struct target *target, enum watchpoint_rw *rw,
		target_addr_t *address);

static inline void watchpoint_set(struct watchpoint *watchpoint, unsigned int number)
{
	watchpoint->is_set = true;
	watchpoint->number = number;
}

#endif /* OPENOCD_TARGET_BREAKPOINTS_H */
