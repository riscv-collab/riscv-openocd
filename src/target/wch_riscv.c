/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <helper/log.h>
#include <helper/time_support.h>
#include <target/target.h>
#include <target/algorithm.h>
#include <target/target_type.h>
#include <target/smp.h>
#include  <target/register.h>
#include <target/breakpoints.h>
#include <helper/base64.h>
#include <helper/time_support.h>
#include <target/riscv/gdb_regs.h>
#include <rtos/rtos.h>
#include <target/riscv/debug_defines.h>
#include <helper/bits.h>
#include "wch_riscv.h"


static int wch_riscv_flush_registers(struct target *target);
 struct trigger {
	uint64_t address;
	uint32_t length;
	uint64_t mask;
	uint64_t value;
	bool read, write, execute;
	int unique_id;
};
extern int write_flash_data(struct target *target, target_addr_t address,uint32_t size, uint32_t count, uint8_t *buffer);


static enum {
	RO_NORMAL,
	RO_REVERSED
} resume_order;

static int riscv_resume_go_all_harts(struct target *target);
extern unsigned char riscvchip;
extern unsigned  int  chip_type;

static struct target_type *get_target_type(struct target *target)
{
	riscv_info_t *info = (riscv_info_t *) target->arch_info;

	if (!info) {
		LOG_ERROR("Target has not been initialized");
		return NULL;
	}

	switch (info->dtm_version) {
		case 0:
		case 1:
			return &wch_riscv013_target;
		default:
			LOG_ERROR("[%s] Unsupported DTM version: %d",
					target_name(target), info->dtm_version);
			return NULL;
	}
}

static int wch_riscv_create_target(struct target *target, Jim_Interp *interp)
{
	LOG_DEBUG("wch_riscv_create_target()");
	target->arch_info = calloc(1, sizeof(riscv_info_t));
	if (!target->arch_info) {
		LOG_ERROR("Failed to allocate RISC-V target structure.");
		return ERROR_FAIL;
	}
	riscv_info_init(target, target->arch_info);
	return ERROR_OK;
}

static int wch_riscv_init_target(struct command_context *cmd_ctx,
		struct target *target)
{
	LOG_DEBUG("wch_riscv_init_target()");
	RISCV_INFO(info);
	info->cmd_ctx = cmd_ctx;

	target->debug_reason = DBG_REASON_DBGRQ;

	return ERROR_OK;
}

static void riscv_free_registers(struct target *target)
{
	/* Free the shared structure use for most registers. */
	if (target->reg_cache) {
		if (target->reg_cache->reg_list) {
			free(target->reg_cache->reg_list[0].arch_info);
			/* Free the ones we allocated separately. */
			for (unsigned i = GDB_REGNO_COUNT; i < target->reg_cache->num_regs; i++)
				free(target->reg_cache->reg_list[i].arch_info);
			for (unsigned int i = 0; i < target->reg_cache->num_regs; i++)
				free(target->reg_cache->reg_list[i].value);
			free(target->reg_cache->reg_list);
		}
		free(target->reg_cache);
	}
}

static void wch_riscv_deinit_target(struct target *target)
{
	LOG_DEBUG("wch_riscv_deinit_target()");

	riscv_info_t *info = target->arch_info;
	struct target_type *tt = get_target_type(target);

	if (wch_riscv_flush_registers(target) != ERROR_OK)
		LOG_ERROR("[%s] Failed to flush registers. Ignoring this error.", target_name(target));

	if (tt && info->version_specific)
		tt->deinit_target(target);

	riscv_free_registers(target);

	range_list_t *entry, *tmp;
	list_for_each_entry_safe(entry, tmp, &info->expose_csr, list) {
		free(entry->name);
		free(entry);
	}

	list_for_each_entry_safe(entry, tmp, &info->expose_custom, list) {
		free(entry->name);
		free(entry);
	}

	free(info->reg_names);
	free(target->arch_info);

	target->arch_info = NULL;
}

static void trigger_from_breakpoint(struct trigger *trigger,
		const struct breakpoint *breakpoint)
{
	trigger->address = breakpoint->address;
	trigger->length = breakpoint->length;
	trigger->mask = ~0LL;
	trigger->read = false;
	trigger->write = false;
	trigger->execute = true;
	/* unique_id is unique across both breakpoints and watchpoints. */
	trigger->unique_id = breakpoint->unique_id;
}

static int maybe_add_trigger_t1(struct target *target,
		struct trigger *trigger, uint64_t tdata1)
{
	RISCV_INFO(r);

	const uint32_t bpcontrol_x = 1<<0;
	const uint32_t bpcontrol_w = 1<<1;
	const uint32_t bpcontrol_r = 1<<2;
	const uint32_t bpcontrol_u = 1<<3;
	const uint32_t bpcontrol_s = 1<<4;
	const uint32_t bpcontrol_h = 1<<5;
	const uint32_t bpcontrol_m = 1<<6;
	const uint32_t bpcontrol_bpmatch = 0xf << 7;
	const uint32_t bpcontrol_bpaction = 0xff << 11;

	if (tdata1 & (bpcontrol_r | bpcontrol_w | bpcontrol_x)) {
		/* Trigger is already in use, presumably by user code. */
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	tdata1 = set_field(tdata1, bpcontrol_r, trigger->read);
	tdata1 = set_field(tdata1, bpcontrol_w, trigger->write);
	tdata1 = set_field(tdata1, bpcontrol_x, trigger->execute);
	tdata1 = set_field(tdata1, bpcontrol_u,
			!!(r->misa & BIT('U' - 'A')));
	tdata1 = set_field(tdata1, bpcontrol_s,
			!!(r->misa & BIT('S' - 'A')));
	tdata1 = set_field(tdata1, bpcontrol_h,
			!!(r->misa & BIT('H' - 'A')));
	tdata1 |= bpcontrol_m;
	tdata1 = set_field(tdata1, bpcontrol_bpmatch, 0); /* exact match */
	tdata1 = set_field(tdata1, bpcontrol_bpaction, 0); /* cause bp exception */

	riscv_set_register(target, GDB_REGNO_TDATA1, tdata1);

	riscv_reg_t tdata1_rb;
	if (riscv_get_register(target, &tdata1_rb, GDB_REGNO_TDATA1) != ERROR_OK)
		return ERROR_FAIL;
	LOG_DEBUG("tdata1=0x%" PRIx64, tdata1_rb);

	if (tdata1 != tdata1_rb) {
		LOG_DEBUG("Trigger doesn't support what we need; After writing 0x%"
				PRIx64 " to tdata1 it contains 0x%" PRIx64,
				tdata1, tdata1_rb);
		riscv_set_register(target, GDB_REGNO_TDATA1, 0);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	riscv_set_register(target, GDB_REGNO_TDATA2, trigger->address);

	return ERROR_OK;
}

static int maybe_add_trigger_t2(struct target *target,
		struct trigger *trigger, uint64_t tdata1)
{
	RISCV_INFO(r);

	/* tselect is already set */
	if (tdata1 & (CSR_MCONTROL_EXECUTE | CSR_MCONTROL_STORE | CSR_MCONTROL_LOAD)) {
		/* Trigger is already in use, presumably by user code. */
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	/* address/data match trigger */
	
	tdata1 = set_field(tdata1, CSR_MCONTROL_ACTION,
			CSR_MCONTROL_ACTION_DEBUG_MODE);
	tdata1 = set_field(tdata1, CSR_MCONTROL_MATCH, CSR_MCONTROL_MATCH_EQUAL);
	tdata1 |= CSR_MCONTROL_M;
	if (r->misa & (1 << ('S' - 'A')))
		tdata1 |= CSR_MCONTROL_S;
	if (r->misa & (1 << ('U' - 'A')))
		tdata1 |= CSR_MCONTROL_U;

	if (trigger->execute)
		tdata1 |= CSR_MCONTROL_EXECUTE;
	if (trigger->read)
		tdata1 |= CSR_MCONTROL_LOAD;
	if (trigger->write)
		tdata1 |= CSR_MCONTROL_STORE;

	riscv_set_register(target, GDB_REGNO_TDATA1, tdata1);

	uint64_t tdata1_rb;
	int result = riscv_get_register(target, &tdata1_rb, GDB_REGNO_TDATA1);
	if (result != ERROR_OK)
		return result;
	LOG_DEBUG("tdata1=0x%" PRIx64, tdata1_rb);

	if (tdata1 != tdata1_rb) {
		LOG_DEBUG("Trigger doesn't support what we need; After writing 0x%"
				PRIx64 " to tdata1 it contains 0x%" PRIx64,
				tdata1, tdata1_rb);
		riscv_set_register(target, GDB_REGNO_TDATA1, 0);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	riscv_set_register(target, GDB_REGNO_TDATA2, trigger->address);

	return ERROR_OK;
}

static int maybe_add_trigger_t6(struct target *target,
		struct trigger *trigger, uint64_t tdata1)
{
	RISCV_INFO(r);

	/* tselect is already set */
	if (tdata1 & (CSR_MCONTROL6_EXECUTE | CSR_MCONTROL6_STORE | CSR_MCONTROL6_LOAD)) {
		/* Trigger is already in use, presumably by user code. */
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	/* address/data match trigger */
	tdata1 |= CSR_MCONTROL6_DMODE(riscv_xlen(target));
	tdata1 = set_field(tdata1, CSR_MCONTROL6_ACTION,
			CSR_MCONTROL6_ACTION_DEBUG_MODE);
	tdata1 = set_field(tdata1, CSR_MCONTROL6_MATCH, CSR_MCONTROL6_MATCH_EQUAL);
	tdata1 |= CSR_MCONTROL6_M;
	if (r->misa & (1 << ('H' - 'A')))
		tdata1 |= CSR_MCONTROL6_VS | CSR_MCONTROL6_VU;
	if (r->misa & (1 << ('S' - 'A')))
		tdata1 |= CSR_MCONTROL6_S;
	if (r->misa & (1 << ('U' - 'A')))
		tdata1 |= CSR_MCONTROL6_U;

	if (trigger->execute)
		tdata1 |= CSR_MCONTROL6_EXECUTE;
	if (trigger->read)
		tdata1 |= CSR_MCONTROL6_LOAD;
	if (trigger->write)
		tdata1 |= CSR_MCONTROL6_STORE;

	riscv_set_register(target, GDB_REGNO_TDATA1, tdata1);

	uint64_t tdata1_rb;
	int result = riscv_get_register(target, &tdata1_rb, GDB_REGNO_TDATA1);
	if (result != ERROR_OK)
		return result;
	LOG_DEBUG("tdata1=0x%" PRIx64, tdata1_rb);

	if (tdata1 != tdata1_rb) {
		LOG_DEBUG("Trigger doesn't support what we need; After writing 0x%"
				PRIx64 " to tdata1 it contains 0x%" PRIx64,
				tdata1, tdata1_rb);
		riscv_set_register(target, GDB_REGNO_TDATA1, 0);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	riscv_set_register(target, GDB_REGNO_TDATA2, trigger->address);

	return ERROR_OK;
}

static int add_trigger(struct target *target, struct trigger *trigger)
{
	RISCV_INFO(r);

	if (riscv_enumerate_triggers(target) != ERROR_OK)
		return ERROR_FAIL;

	riscv_reg_t tselect;
	if (riscv_get_register(target, &tselect, GDB_REGNO_TSELECT) != ERROR_OK)
		return ERROR_FAIL;

	unsigned int i;
	for (i = 0; i < r->trigger_count; i++) {
		if (r->trigger_unique_id[i] != -1)
			continue;

		riscv_set_register(target, GDB_REGNO_TSELECT, i);

		uint64_t tdata1;
		int result = riscv_get_register(target, &tdata1, GDB_REGNO_TDATA1);
		if (result != ERROR_OK)
			return result;
		int type = get_field(tdata1, CSR_TDATA1_TYPE(riscv_xlen(target)));
		
		result = ERROR_OK;
		switch (type) {
			case 1:
				result = maybe_add_trigger_t1(target, trigger, tdata1);
				break;
			case 2:
				result = maybe_add_trigger_t2(target, trigger, tdata1);
				break;
			case 6:
				result = maybe_add_trigger_t6(target, trigger, tdata1);
				break;
			default:
				LOG_DEBUG("trigger %d has unknown type %d", i, type);
				continue;
		}

		if (result != ERROR_OK)
			continue;

		LOG_DEBUG("[%d] Using trigger %d (type %d) for bp %d", target->coreid,
				i, type, trigger->unique_id);
		r->trigger_unique_id[i] = trigger->unique_id;
		break;
	}

	riscv_set_register(target, GDB_REGNO_TSELECT, tselect);

	if (i >= r->trigger_count) {
		LOG_ERROR("Couldn't find an available hardware trigger.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	return ERROR_OK;
}

/**
 * Write one memory item of given "size". Use memory access of given "access_size".
 * Utilize read-modify-write, if needed.
 * */
static int write_by_given_size(struct target *target, target_addr_t address,
		uint32_t size, uint8_t *buffer, uint32_t access_size)
{
	assert(size == 1 || size == 2 || size == 4 || size == 8);
	assert(access_size == 1 || access_size == 2 || access_size == 4 || access_size == 8);

	if (access_size <= size && address % access_size == 0)
		/* Can do the memory access directly without a helper buffer. */
		return target_write_memory(target, address, access_size, size / access_size, buffer);

	unsigned int offset_head = address % access_size;
	unsigned int n_blocks = ((size + offset_head) <= access_size) ? 1 : 2;
	uint8_t helper_buf[n_blocks * access_size];

	/* Read from memory */
	if (target_read_memory(target, address - offset_head, access_size, n_blocks, helper_buf) != ERROR_OK)
		return ERROR_FAIL;

	/* Modify and write back */
	memcpy(helper_buf + offset_head, buffer, size);
	return target_write_memory(target, address - offset_head, access_size, n_blocks, helper_buf);
}

/**
 * Read one memory item of given "size". Use memory access of given "access_size".
 * Read larger section of memory and pick out the required portion, if needed.
 * */
static int read_by_given_size(struct target *target, target_addr_t address,
	uint32_t size, uint8_t *buffer, uint32_t access_size)
{
	assert(size == 1 || size == 2 || size == 4 || size == 8);
	assert(access_size == 1 || access_size == 2 || access_size == 4 || access_size == 8);

	if (access_size <= size && address % access_size == 0)
		/* Can do the memory access directly without a helper buffer. */
		return target_read_memory(target, address, access_size, size / access_size, buffer);

	unsigned int offset_head = address % access_size;
	unsigned int n_blocks = ((size + offset_head) <= access_size) ? 1 : 2;
	uint8_t helper_buf[n_blocks * access_size];

	/* Read from memory */
	if (target_read_memory(target, address - offset_head, access_size, n_blocks, helper_buf) != ERROR_OK)
		return ERROR_FAIL;

	/* Pick the requested portion from the buffer */
	memcpy(buffer, helper_buf + offset_head, size);
	return ERROR_OK;
}

int wch_riscv_read_by_any_size(struct target *target, target_addr_t address, uint32_t size, uint8_t *buffer)
{
	
	assert(size == 1 || size == 2 ||  size == 4 || size == 8);
	/* Find access size that correspond to data size and the alignment. */
	unsigned int preferred_size = size;
	while (address % preferred_size != 0)
		preferred_size /= 2;

	/* First try the preferred (most natural) access size. */
	if (read_by_given_size(target, address, size, buffer, preferred_size) == ERROR_OK)
		return ERROR_OK;

	/* On failure, try other access sizes.
	   Minimize the number of accesses by trying first the largest size. */
	for (unsigned int access_size = 8; access_size > 0; access_size /= 2) {
		if (access_size == preferred_size)
			/* Already tried this size. */
			continue;

		if (read_by_given_size(target, address, size, buffer, access_size) == ERROR_OK)
			return ERROR_OK;
	}

	/* No access attempt succeeded. */
	return ERROR_FAIL;
}
/**
 * Write one memory item using any memory access size that will work.
 * Utilize read-modify-write, if needed.
 * */
int wch_riscv_write_by_any_size(struct target *target, target_addr_t address, uint32_t size, uint8_t *buffer)
{
	assert(size == 1 || size == 2 ||  size == 4 || size == 8);
    if(address<0x20000000){
			if(address>=0x08000000)
				address -= 0x08000000;
		
		int ret=write_flash_data(target,address,size,1,buffer);
		return ret;
    }
	/* Find access size that correspond to data size and the alignment. */
	unsigned int preferred_size = size;
	while (address % preferred_size != 0)
		preferred_size /= 2;

	/* First try the preferred (most natural) access size. */
	if (write_by_given_size(target, address, size, buffer, preferred_size) == ERROR_OK)
		return ERROR_OK;

	/* On failure, try other access sizes.
	   Minimize the number of accesses by trying first the largest size. */
	for (unsigned int access_size = 8; access_size > 0; access_size /= 2) {
		if (access_size == preferred_size)
			/* Already tried this size. */
			continue;

		if (write_by_given_size(target, address, size, buffer, access_size) == ERROR_OK)
			return ERROR_OK;
	}

	/* No access attempt succeeded. */
	return ERROR_FAIL;
}


int wch_riscv_add_breakpoint(struct target *target, struct breakpoint *breakpoint)
{
	LOG_TARGET_DEBUG(target, "@0x%" TARGET_PRIxADDR, breakpoint->address);
	
	assert(breakpoint);
	if((riscvchip==6 ||(((uint16_t)chip_type) ==0x050c)) && breakpoint->type == BKPT_HARD && target->breakpoints ){
			struct breakpoint *p= target->breakpoints->next;
			int len=0;
			while(p){
				len++;
				p=p->next;
			}
			if(len>2)
				breakpoint->type = BKPT_SOFT;		
		}
	if(riscvchip==1||riscvchip==2||riscvchip==3 ||riscvchip==7 ||riscvchip==9 ||riscvchip==0x0a ||((uint16_t)chip_type) ==0x0500){
			breakpoint->type = BKPT_SOFT;
		}
	if (breakpoint->type == BKPT_SOFT) {
		/** @todo check RVC for size/alignment */
		if (!(breakpoint->length == 4 || breakpoint->length == 2)) {
			LOG_ERROR("Invalid breakpoint length %d", breakpoint->length);
			return ERROR_FAIL;
		}

		if (0 != (breakpoint->address % 2)) {
			LOG_ERROR("Invalid breakpoint alignment for address 0x%" TARGET_PRIxADDR, breakpoint->address);
			return ERROR_FAIL;
		}

		/* Read the original instruction. */
		if (wch_riscv_read_by_any_size(
				target, breakpoint->address, breakpoint->length, breakpoint->orig_instr) != ERROR_OK) {
			LOG_ERROR("Failed to read original instruction at 0x%" TARGET_PRIxADDR,
					breakpoint->address);
			return ERROR_FAIL;
		}

		uint8_t buff[4] = { 0 };
		buf_set_u32(buff, 0, breakpoint->length * CHAR_BIT, breakpoint->length == 4 ? ebreak() : ebreak_c());
		/* Write the ebreak instruction. */
		if (wch_riscv_write_by_any_size(target, breakpoint->address, breakpoint->length, buff) != ERROR_OK) {
			LOG_ERROR("Failed to write %d-byte breakpoint instruction at 0x%"
					TARGET_PRIxADDR, breakpoint->length, breakpoint->address);
			return ERROR_FAIL;
		}

	} else if (breakpoint->type == BKPT_HARD) {
		struct trigger trigger;
		trigger_from_breakpoint(&trigger, breakpoint);
		int const result = add_trigger(target, &trigger);
		if (result != ERROR_OK)
			return result;
	} else {
		LOG_INFO("OpenOCD only supports hardware and software breakpoints.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	breakpoint->is_set = true;
	return ERROR_OK;
}

static int remove_trigger(struct target *target, struct trigger *trigger)
{
	RISCV_INFO(r);

	if (riscv_enumerate_triggers(target) != ERROR_OK)
		return ERROR_FAIL;

	unsigned int i;
	for (i = 0; i < r->trigger_count; i++) {
		if (r->trigger_unique_id[i] == trigger->unique_id)
			break;
	}
	if (i >= r->trigger_count) {
		LOG_ERROR("Couldn't find the hardware resources used by hardware "
				"trigger.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}
	LOG_DEBUG("[%d] Stop using resource %d for bp %d", target->coreid, i,
			trigger->unique_id);

	riscv_reg_t tselect;
	int result = riscv_get_register(target, &tselect, GDB_REGNO_TSELECT);
	if (result != ERROR_OK)
		return result;
	riscv_set_register(target, GDB_REGNO_TSELECT, i);
	riscv_set_register(target, GDB_REGNO_TDATA1, 0);
	riscv_set_register(target, GDB_REGNO_TSELECT, tselect);
	r->trigger_unique_id[i] = -1;

	return ERROR_OK;
}

int wch_riscv_remove_breakpoint(struct target *target,
		struct breakpoint *breakpoint)
{
	if (breakpoint->type == BKPT_SOFT) {
		/* Write the original instruction. */
		if (wch_riscv_write_by_any_size(
				target, breakpoint->address, breakpoint->length, breakpoint->orig_instr) != ERROR_OK) {
			LOG_ERROR("Failed to restore instruction for %d-byte breakpoint at "
					"0x%" TARGET_PRIxADDR, breakpoint->length, breakpoint->address);
			return ERROR_FAIL;
		}

	} else if (breakpoint->type == BKPT_HARD) {
		struct trigger trigger;
		trigger_from_breakpoint(&trigger, breakpoint);
		int result = remove_trigger(target, &trigger);
		if (result != ERROR_OK)
			return result;

	} else {
		LOG_INFO("OpenOCD only supports hardware and software breakpoints.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	breakpoint->is_set = false;

	return ERROR_OK;
}

static void trigger_from_watchpoint(struct trigger *trigger,
		const struct watchpoint *watchpoint)
{
	trigger->address = watchpoint->address;
	trigger->length = watchpoint->length;
	trigger->mask = watchpoint->mask;
	trigger->value = watchpoint->value;
	trigger->read = (watchpoint->rw == WPT_READ || watchpoint->rw == WPT_ACCESS);
	trigger->write = (watchpoint->rw == WPT_WRITE || watchpoint->rw == WPT_ACCESS);
	trigger->execute = false;
	/* unique_id is unique across both breakpoints and watchpoints. */
	trigger->unique_id = watchpoint->unique_id;
}

int wch_riscv_add_watchpoint(struct target *target, struct watchpoint *watchpoint)
{
	struct trigger trigger;
	trigger_from_watchpoint(&trigger, watchpoint);

	int result = add_trigger(target, &trigger);
	if (result != ERROR_OK)
		return result;
	watchpoint->is_set = true;

	return ERROR_OK;
}

int wch_riscv_remove_watchpoint(struct target *target,
		struct watchpoint *watchpoint)
{
	LOG_DEBUG("[%d] @0x%" TARGET_PRIxADDR, target->coreid, watchpoint->address);

	struct trigger trigger;
	trigger_from_watchpoint(&trigger, watchpoint);

	int result = remove_trigger(target, &trigger);
	if (result != ERROR_OK)
		return result;
	watchpoint->is_set = false;

	return ERROR_OK;
}

/**
 * Look at the trigger hit bits to find out which trigger is the reason we're
 * halted.  Sets *unique_id to the unique ID of that trigger. If *unique_id is
 * ~0, no match was found.
 */
static int riscv_hit_trigger_hit_bit(struct target *target, uint32_t *unique_id)
{
	RISCV_INFO(r);

	riscv_reg_t tselect;
	if (riscv_get_register(target, &tselect, GDB_REGNO_TSELECT) != ERROR_OK)
		return ERROR_FAIL;

	*unique_id = ~0;
	for (unsigned int i = 0; i < r->trigger_count; i++) {
		if (r->trigger_unique_id[i] == -1)
			continue;

		if (riscv_set_register(target, GDB_REGNO_TSELECT, i) != ERROR_OK)
			return ERROR_FAIL;

		uint64_t tdata1;
		if (riscv_get_register(target, &tdata1, GDB_REGNO_TDATA1) != ERROR_OK)
			return ERROR_FAIL;
		int type = get_field(tdata1, CSR_TDATA1_TYPE(riscv_xlen(target)));

		uint64_t hit_mask = 0;
		switch (type) {
			case 1:
				/* Doesn't support hit bit. */
				break;
			case 2:
				hit_mask = CSR_MCONTROL_HIT;
				break;
			case 6:
				hit_mask = CSR_MCONTROL6_HIT;
				break;
			default:
				LOG_DEBUG("trigger %d has unknown type %d", i, type);
				continue;
		}

		/* Note: If we ever use chained triggers, then this logic needs
		 * to be changed to ignore triggers that are not the last one in
		 * the chain. */
		if (tdata1 & hit_mask) {
			LOG_DEBUG("Trigger %d (unique_id=%d) has hit bit set.", i, r->trigger_unique_id[i]);
			if (riscv_set_register(target, GDB_REGNO_TDATA1, tdata1 & ~hit_mask) != ERROR_OK)
				return ERROR_FAIL;

			*unique_id = r->trigger_unique_id[i];
			break;
		}
	}

	if (riscv_set_register(target, GDB_REGNO_TSELECT, tselect) != ERROR_OK)
		return ERROR_FAIL;

	return ERROR_OK;
}



static int oldriscv_step(struct target *target, int current, uint32_t address,
		int handle_breakpoints)
{
	struct target_type *tt = get_target_type(target);
	return tt->step(target, current, address, handle_breakpoints);
}

static int old_or_new_riscv_step(struct target *target, int current,
		target_addr_t address, int handle_breakpoints)
{
	RISCV_INFO(r);
	LOG_DEBUG("handle_breakpoints=%d", handle_breakpoints);
	if (!r->is_halted)
		return oldriscv_step(target, current, address, handle_breakpoints);
	else
		return wch_riscv_openocd_step(target, current, address, handle_breakpoints);
}

static int wch_riscv_examine(struct target *target)
{
	LOG_DEBUG("[%s]", target_name(target));
	if (target_was_examined(target)) {
		LOG_DEBUG("Target was already examined.");
		return ERROR_OK;
	}

	/* Don't need to select dbus, since the first thing we do is read dtmcontrol. */

	RISCV_INFO(info);
	info->dtm_version = 1;
	LOG_DEBUG("  version=0x%x", info->dtm_version);

	struct target_type *tt = get_target_type(target);
	if (!tt)
		return ERROR_FAIL;

	int result = tt->init_target(info->cmd_ctx, target);
	if (result != ERROR_OK)
		return result;

	return tt->examine(target);
}

static int oldriscv_poll(struct target *target)
{
	struct target_type *tt = get_target_type(target);
	return tt->poll(target);
}

static int old_or_new_riscv_poll(struct target *target)
{
	RISCV_INFO(r);
	if (!r->is_halted)
		return oldriscv_poll(target);
	else
		return wch_riscv_openocd_poll(target);
}

static int wch_riscv_flush_registers(struct target *target)
{
	RISCV_INFO(r);

	if (!target->reg_cache)
		return ERROR_OK;

	LOG_DEBUG("[%s]", target_name(target));

	for (uint32_t number = 0; number < target->reg_cache->num_regs; number++) {
		struct reg *reg = &target->reg_cache->reg_list[number];
		if (reg->valid && reg->dirty) {
			uint64_t value = buf_get_u64(reg->value, 0, reg->size);
			LOG_DEBUG("[%s] %s is dirty; write back 0x%" PRIx64,
				  target_name(target), reg->name, value);
			int result = r->set_register(target, number, value);
			if (result != ERROR_OK)
				return ERROR_FAIL;
			reg->dirty = false;
		}
	}

	return ERROR_OK;
}


int wch_halt_prep(struct target *target)
{
	RISCV_INFO(r);

	LOG_DEBUG("[%s] prep hart, debug_reason=%d", target_name(target),
				target->debug_reason);
	if (riscv_is_halted(target)) {
		LOG_DEBUG("[%s] Hart is already halted (debug_reason=%d).",
				target_name(target), target->debug_reason);
		if (target->debug_reason == DBG_REASON_NOTHALTED) {
			enum riscv_halt_reason halt_reason = riscv_halt_reason(target);
			if (set_debug_reason(target, halt_reason) != ERROR_OK)
				return ERROR_FAIL;
		}
	} else {
		if (r->halt_prep(target) != ERROR_OK)
			return ERROR_FAIL;
		r->prepped = true;
	}

	return ERROR_OK;
}

int wch_riscv_halt_go_all_harts(struct target *target)
{
	RISCV_INFO(r);

	if (riscv_is_halted(target)) {
		LOG_DEBUG("[%s] Hart is already halted.", target_name(target));
	} else {
		if (r->halt_go(target) != ERROR_OK)
			return ERROR_FAIL;

		riscv_invalidate_register_cache(target);
	}

	return ERROR_OK;
}

int wch_halt_go(struct target *target)
{
	riscv_info_t *r = riscv_info(target);
	int result;
	if (!r->is_halted) {
		struct target_type *tt = get_target_type(target);
		result = tt->halt(target);
	} else {
		result = wch_riscv_halt_go_all_harts(target);
	}
	target->state = TARGET_HALTED;
	if (target->debug_reason == DBG_REASON_NOTHALTED)
		target->debug_reason = DBG_REASON_DBGRQ;

	return result;
}

static int halt_finish(struct target *target)
{
	return target_call_event_callbacks(target, TARGET_EVENT_HALTED);
}

int wch_riscv_halt(struct target *target)
{
	RISCV_INFO(r);

	if (!r->is_halted) {
		struct target_type *tt = get_target_type(target);
		return tt->halt(target);
	}

	LOG_TARGET_DEBUG(target, "halting all harts");

	int result = ERROR_OK;
	if (target->smp) {
		struct target_list *tlist;
		foreach_smp_target(tlist, target->smp_targets) {
			struct target *t = tlist->target;
			if (wch_halt_prep(t) != ERROR_OK)
				result = ERROR_FAIL;
		}

		foreach_smp_target(tlist, target->smp_targets) {
			struct target *t = tlist->target;
			riscv_info_t *i = riscv_info(t);
			if (i->prepped) {
				if (wch_halt_go(t) != ERROR_OK)
					result = ERROR_FAIL;
			}
		}

		foreach_smp_target(tlist, target->smp_targets) {
			struct target *t = tlist->target;
			if (halt_finish(t) != ERROR_OK)
				return ERROR_FAIL;
		}

	} else {
		if (wch_halt_prep(target) != ERROR_OK)
			result = ERROR_FAIL;
		if (wch_halt_go(target) != ERROR_OK)
			result = ERROR_FAIL;
		if (halt_finish(target) != ERROR_OK)
			return ERROR_FAIL;
	}

	return result;
}

static int riscv_assert_reset(struct target *target)
{
	LOG_DEBUG("[%d]", target->coreid);
	struct target_type *tt = get_target_type(target);
	riscv_invalidate_register_cache(target);
	return tt->assert_reset(target);
}

static int riscv_deassert_reset(struct target *target)
{
	LOG_DEBUG("[%d]", target->coreid);
	struct target_type *tt = get_target_type(target);
	return tt->deassert_reset(target);
}

/* state must be riscv_reg_t state[RISCV_MAX_HWBPS] = {0}; */
static int disable_triggers(struct target *target, riscv_reg_t *state)
{
	RISCV_INFO(r);

	LOG_DEBUG("deal with triggers");

	if (riscv_enumerate_triggers(target) != ERROR_OK)
		return ERROR_FAIL;

	if (r->manual_hwbp_set) {
		/* Look at every trigger that may have been set. */
		riscv_reg_t tselect;
		if (riscv_get_register(target, &tselect, GDB_REGNO_TSELECT) != ERROR_OK)
			return ERROR_FAIL;
		for (unsigned int t = 0; t < r->trigger_count; t++) {
			if (riscv_set_register(target, GDB_REGNO_TSELECT, t) != ERROR_OK)
				return ERROR_FAIL;
			riscv_reg_t tdata1;
			if (riscv_get_register(target, &tdata1, GDB_REGNO_TDATA1) != ERROR_OK)
				return ERROR_FAIL;
			if (tdata1 & CSR_TDATA1_DMODE(riscv_xlen(target))) {
				state[t] = tdata1;
				if (riscv_set_register(target, GDB_REGNO_TDATA1, 0) != ERROR_OK)
					return ERROR_FAIL;
			}
		}
		if (riscv_set_register(target, GDB_REGNO_TSELECT, tselect) != ERROR_OK)
			return ERROR_FAIL;

	} else {
		/* Just go through the triggers we manage. */
		struct watchpoint *watchpoint = target->watchpoints;
		int i = 0;
		while (watchpoint) {
			LOG_DEBUG("watchpoint %d: set=%d", i, watchpoint->is_set);
			state[i] = watchpoint->is_set;
			if (watchpoint->is_set) {
				if (wch_riscv_remove_watchpoint(target, watchpoint) != ERROR_OK)
					return ERROR_FAIL;
			}
			watchpoint = watchpoint->next;
			i++;
		}
	}

	return ERROR_OK;
}

static int enable_triggers(struct target *target, riscv_reg_t *state)
{
	RISCV_INFO(r);

	if (r->manual_hwbp_set) {
		/* Look at every trigger that may have been set. */
		riscv_reg_t tselect;
		if (riscv_get_register(target, &tselect, GDB_REGNO_TSELECT) != ERROR_OK)
			return ERROR_FAIL;
		for (unsigned int t = 0; t < r->trigger_count; t++) {
			if (state[t] != 0) {
				if (riscv_set_register(target, GDB_REGNO_TSELECT, t) != ERROR_OK)
					return ERROR_FAIL;
				if (riscv_set_register(target, GDB_REGNO_TDATA1, state[t]) != ERROR_OK)
					return ERROR_FAIL;
			}
		}
		if (riscv_set_register(target, GDB_REGNO_TSELECT, tselect) != ERROR_OK)
			return ERROR_FAIL;

	} else {
		struct watchpoint *watchpoint = target->watchpoints;
		int i = 0;
		while (watchpoint) {
			LOG_DEBUG("watchpoint %d: cleared=%" PRId64, i, state[i]);
			if (state[i]) {
				if (wch_riscv_add_watchpoint(target, watchpoint) != ERROR_OK)
					return ERROR_FAIL;
			}
			watchpoint = watchpoint->next;
			i++;
		}
	}

	return ERROR_OK;
}

/**
 * Get everything ready to resume.
 */
static int resume_prep(struct target *target, int current,
		target_addr_t address, int handle_breakpoints, int debug_execution)
{
	RISCV_INFO(r);
	LOG_TARGET_DEBUG(target, "target->state=%d", target->state);

	if (!current)
		riscv_set_register(target, GDB_REGNO_PC, address);

	if (target->debug_reason == DBG_REASON_WATCHPOINT) {
		/* To be able to run off a trigger, disable all the triggers, step, and
		 * then resume as usual. */
		riscv_reg_t trigger_state[RISCV_MAX_HWBPS] = {0};

		if (disable_triggers(target, trigger_state) != ERROR_OK)
			return ERROR_FAIL;

		if (old_or_new_riscv_step(target, true, 0, false) != ERROR_OK)
			return ERROR_FAIL;

		if (enable_triggers(target, trigger_state) != ERROR_OK)
			return ERROR_FAIL;
	}

	if (r->is_halted) {
		if (r->resume_prep(target) != ERROR_OK)
			return ERROR_FAIL;
	}

	LOG_DEBUG("[%d] mark as prepped", target->coreid);
	r->prepped = true;

	return ERROR_OK;
}

/**
 * Resume all the harts that have been prepped, as close to instantaneous as
 * possible.
 */
static int resume_go(struct target *target, int current,
		target_addr_t address, int handle_breakpoints, int debug_execution)
{
	riscv_info_t *r = riscv_info(target);
	int result;
	if (!r->is_halted) {
		struct target_type *tt = get_target_type(target);
		result = tt->resume(target, current, address, handle_breakpoints,
				debug_execution);
	} else {
		result = riscv_resume_go_all_harts(target);
	}

	return result;
}

static int resume_finish(struct target *target, int debug_execution)
{
	register_cache_invalidate(target->reg_cache);

	target->state = debug_execution ? TARGET_DEBUG_RUNNING : TARGET_RUNNING;
	target->debug_reason = DBG_REASON_NOTHALTED;
	return target_call_event_callbacks(target,
		debug_execution ? TARGET_EVENT_DEBUG_RESUMED : TARGET_EVENT_RESUMED);
}

/**
 * @par single_hart When true, only resume a single hart even if SMP is
 * configured.  This is used to run algorithms on just one hart.
 */
int wch_riscv_resume(
		struct target *target,
		int current,
		target_addr_t address,
		int handle_breakpoints,
		int debug_execution,
		bool single_hart)
{
	LOG_DEBUG("handle_breakpoints=%d", handle_breakpoints);
	int result = ERROR_OK;
	if (target->smp && !single_hart) {
		struct target_list *tlist;
		foreach_smp_target_direction(resume_order == RO_NORMAL,
									 tlist, target->smp_targets) {
			struct target *t = tlist->target;
			if (resume_prep(t, current, address, handle_breakpoints,
						debug_execution) != ERROR_OK)
				result = ERROR_FAIL;
		}

		foreach_smp_target_direction(resume_order == RO_NORMAL,
									 tlist, target->smp_targets) {
			struct target *t = tlist->target;
			riscv_info_t *i = riscv_info(t);
			if (i->prepped) {
				if (resume_go(t, current, address, handle_breakpoints,
							debug_execution) != ERROR_OK)
					result = ERROR_FAIL;
			}
		}

		foreach_smp_target_direction(resume_order == RO_NORMAL,
									 tlist, target->smp_targets) {
			struct target *t = tlist->target;
			if (resume_finish(t, debug_execution) != ERROR_OK)
				result = ERROR_FAIL;
		}

	} else {
		if (resume_prep(target, current, address, handle_breakpoints,
					debug_execution) != ERROR_OK)
			result = ERROR_FAIL;
		if (resume_go(target, current, address, handle_breakpoints,
					debug_execution) != ERROR_OK)
			result = ERROR_FAIL;
		if (resume_finish(target, debug_execution) != ERROR_OK)
			return ERROR_FAIL;
	}

	return result;
}

static int riscv_target_resume(struct target *target, int current, target_addr_t address,
		int handle_breakpoints, int debug_execution)
{
	return wch_riscv_resume(target, current, address, handle_breakpoints,
			debug_execution, false);
}

static int riscv_mmu(struct target *target, int *enabled)
{
	if (!riscv_enable_virt2phys) {
		*enabled = 0;
		return ERROR_OK;
	}

	/* Don't use MMU in explicit or effective M (machine) mode */
	riscv_reg_t priv;
	if (riscv_get_register(target, &priv, GDB_REGNO_PRIV) != ERROR_OK) {
		LOG_ERROR("Failed to read priv register.");
		return ERROR_FAIL;
	}

	riscv_reg_t mstatus;
	if (riscv_get_register(target, &mstatus, GDB_REGNO_MSTATUS) != ERROR_OK) {
		LOG_ERROR("Failed to read mstatus register.");
		return ERROR_FAIL;
	}

	if ((get_field(mstatus, MSTATUS_MPRV) ? get_field(mstatus, MSTATUS_MPP) : priv) == PRV_M) {
		LOG_DEBUG("SATP/MMU ignored in Machine mode (mstatus=0x%" PRIx64 ").", mstatus);
		*enabled = 0;
		return ERROR_OK;
	}

	riscv_reg_t satp;
	if (riscv_get_register(target, &satp, GDB_REGNO_SATP) != ERROR_OK) {
		LOG_DEBUG("Couldn't read SATP.");
		/* If we can't read SATP, then there must not be an MMU. */
		*enabled = 0;
		return ERROR_OK;
	}

	if (get_field(satp, RISCV_SATP_MODE(riscv_xlen(target))) == SATP_MODE_OFF) {
		LOG_DEBUG("MMU is disabled.");
		*enabled = 0;
	} else {
		LOG_DEBUG("MMU is enabled.");
		*enabled = 1;
	}

	return ERROR_OK;
}

static int riscv_address_translate(struct target *target,
		target_addr_t virtual, target_addr_t *physical)
{
	RISCV_INFO(r);
	riscv_reg_t satp_value;
	int mode;
	uint64_t ppn_value;
	target_addr_t table_address;
	const virt2phys_info_t *info;
	uint64_t pte = 0;
	int i;

	int result = riscv_get_register(target, &satp_value, GDB_REGNO_SATP);
	if (result != ERROR_OK)
		return result;

	unsigned xlen = riscv_xlen(target);
	mode = get_field(satp_value, RISCV_SATP_MODE(xlen));
	switch (mode) {
		case SATP_MODE_SV32:
			info = &sv32;
			break;
		case SATP_MODE_SV39:
			info = &sv39;
			break;
		case SATP_MODE_SV48:
			info = &sv48;
			break;
		case SATP_MODE_OFF:
			LOG_ERROR("No translation or protection." \
				      " (satp: 0x%" PRIx64 ")", satp_value);
			return ERROR_FAIL;
		default:
			LOG_ERROR("The translation mode is not supported." \
				      " (satp: 0x%" PRIx64 ")", satp_value);
			return ERROR_FAIL;
	}
	LOG_DEBUG("virtual=0x%" TARGET_PRIxADDR "; mode=%s", virtual, info->name);

	/* verify bits xlen-1:va_bits-1 are all equal */
	target_addr_t mask = ((target_addr_t)1 << (xlen - (info->va_bits - 1))) - 1;
	target_addr_t masked_msbs = (virtual >> (info->va_bits - 1)) & mask;
	if (masked_msbs != 0 && masked_msbs != mask) {
		LOG_ERROR("Virtual address 0x%" TARGET_PRIxADDR " is not sign-extended "
				"for %s mode.", virtual, info->name);
		return ERROR_FAIL;
	}

	ppn_value = get_field(satp_value, RISCV_SATP_PPN(xlen));
	table_address = ppn_value << RISCV_PGSHIFT;
	i = info->level - 1;
	while (i >= 0) {
		uint64_t vpn = virtual >> info->vpn_shift[i];
		vpn &= info->vpn_mask[i];
		target_addr_t pte_address = table_address +
									(vpn << info->pte_shift);
		uint8_t buffer[8];
		assert(info->pte_shift <= 3);
		int retval = r->read_memory(target, pte_address,
				4, (1 << info->pte_shift) / 4, buffer, 4);
		if (retval != ERROR_OK)
			return ERROR_FAIL;

		if (info->pte_shift == 2)
			pte = buf_get_u32(buffer, 0, 32);
		else
			pte = buf_get_u64(buffer, 0, 64);

		LOG_DEBUG("i=%d; PTE @0x%" TARGET_PRIxADDR " = 0x%" PRIx64, i,
				pte_address, pte);

		if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W)))
			return ERROR_FAIL;

		if ((pte & PTE_R) || (pte & PTE_X)) /* Found leaf PTE. */
			break;

		i--;
		if (i < 0)
			break;
		ppn_value = pte >> PTE_PPN_SHIFT;
		table_address = ppn_value << RISCV_PGSHIFT;
	}

	if (i < 0) {
		LOG_ERROR("Couldn't find the PTE.");
		return ERROR_FAIL;
	}

	/* Make sure to clear out the high bits that may be set. */
	*physical = virtual & (((target_addr_t)1 << info->va_bits) - 1);

	while (i < info->level) {
		ppn_value = pte >> info->pte_ppn_shift[i];
		ppn_value &= info->pte_ppn_mask[i];
		*physical &= ~(((target_addr_t)info->pa_ppn_mask[i]) <<
				info->pa_ppn_shift[i]);
		*physical |= (ppn_value << info->pa_ppn_shift[i]);
		i++;
	}
	LOG_DEBUG("0x%" TARGET_PRIxADDR " -> 0x%" TARGET_PRIxADDR, virtual,
			*physical);

	return ERROR_OK;
}

static int riscv_virt2phys(struct target *target, target_addr_t virtual, target_addr_t *physical)
{
	int enabled;
	if (riscv_mmu(target, &enabled) == ERROR_OK) {
		if (!enabled)
			return ERROR_FAIL;

		if (riscv_address_translate(target, virtual, physical) == ERROR_OK)
			return ERROR_OK;
	}

	return ERROR_FAIL;
}

static int riscv_read_phys_memory(struct target *target, target_addr_t phys_address,
			uint32_t size, uint32_t count, uint8_t *buffer)
{
	RISCV_INFO(r);
	return r->read_memory(target, phys_address, size, count, buffer, size);
}

static int riscv_read_memory(struct target *target, target_addr_t address,
		uint32_t size, uint32_t count, uint8_t *buffer)
{
	if (count == 0) {
		LOG_WARNING("0-length read from 0x%" TARGET_PRIxADDR, address);
		return ERROR_OK;
	}

	target_addr_t physical_addr;
	if (target->type->virt2phys(target, address, &physical_addr) == ERROR_OK)
		address = physical_addr;

	RISCV_INFO(r);
	return r->read_memory(target, address, size, count, buffer, size);
}

static int riscv_write_phys_memory(struct target *target, target_addr_t phys_address,
			uint32_t size, uint32_t count, const uint8_t *buffer)
{
	struct target_type *tt = get_target_type(target);
	return tt->write_memory(target, phys_address, size, count, buffer);
}

static int riscv_write_memory(struct target *target, target_addr_t address,
		uint32_t size, uint32_t count, const uint8_t *buffer)
{
	if (count == 0) {
		LOG_WARNING("0-length write to 0x%" TARGET_PRIxADDR, address);
		return ERROR_OK;
	}

	target_addr_t physical_addr;
	if (target->type->virt2phys(target, address, &physical_addr) == ERROR_OK)
		address = physical_addr;

	struct target_type *tt = get_target_type(target);
	return tt->write_memory(target, address, size, count, buffer);
}



static int riscv_get_gdb_reg_list_internal(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class, bool read)
{
	LOG_TARGET_DEBUG(target, "reg_class=%d, read=%d", reg_class, read);

	if (!target->reg_cache) {
		LOG_ERROR("Target not initialized. Return ERROR_FAIL.");
		return ERROR_FAIL;
	}

	switch (reg_class) {
		case REG_CLASS_GENERAL:
			*reg_list_size = 33;
			break;
		case REG_CLASS_ALL:
			*reg_list_size = target->reg_cache->num_regs;
			break;
		default:
			LOG_ERROR("Unsupported reg_class: %d", reg_class);
			return ERROR_FAIL;
	}

	*reg_list = calloc(*reg_list_size, sizeof(struct reg *));
	if (!*reg_list)
		return ERROR_FAIL;

	for (int i = 0; i < *reg_list_size; i++) {
		assert(!target->reg_cache->reg_list[i].valid ||
				target->reg_cache->reg_list[i].size > 0);
		(*reg_list)[i] = &target->reg_cache->reg_list[i];
		if (read &&
				target->reg_cache->reg_list[i].exist &&
				!target->reg_cache->reg_list[i].valid) {
			if (target->reg_cache->reg_list[i].type->get(
						&target->reg_cache->reg_list[i]) != ERROR_OK)
				return ERROR_FAIL;
		}
	}

	return ERROR_OK;
}

static int riscv_get_gdb_reg_list_noread(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class)
{
	return riscv_get_gdb_reg_list_internal(target, reg_list, reg_list_size,
			reg_class, false);
}

static int riscv_get_gdb_reg_list(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class)
{
	return riscv_get_gdb_reg_list_internal(target, reg_list, reg_list_size,
			reg_class, true);
}

static int riscv_arch_state(struct target *target)
{
	struct target_type *tt = get_target_type(target);
	return tt->arch_state(target);
}

/* Algorithm must end with a software breakpoint instruction. */
static int riscv_run_algorithm(struct target *target, int num_mem_params,
		struct mem_param *mem_params, int num_reg_params,
		struct reg_param *reg_params, target_addr_t entry_point,
		target_addr_t exit_point, int timeout_ms, void *arch_info)
{
	RISCV_INFO(info);

	if (num_mem_params > 0) {
		LOG_ERROR("Memory parameters are not supported for RISC-V algorithms.");
		return ERROR_FAIL;
	}

	if (target->state != TARGET_HALTED) {
		LOG_WARNING("target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* Save registers */
	struct reg *reg_pc = register_get_by_name(target->reg_cache, "pc", true);
	if (!reg_pc || reg_pc->type->get(reg_pc) != ERROR_OK)
		return ERROR_FAIL;
	uint64_t saved_pc = buf_get_u64(reg_pc->value, 0, reg_pc->size);
	LOG_DEBUG("saved_pc=0x%" PRIx64, saved_pc);

	uint64_t saved_regs[32];
	for (int i = 0; i < num_reg_params; i++) {
		LOG_DEBUG("save %s", reg_params[i].reg_name);
		struct reg *r = register_get_by_name(target->reg_cache, reg_params[i].reg_name, false);
		if (!r) {
			LOG_ERROR("Couldn't find register named '%s'", reg_params[i].reg_name);
			return ERROR_FAIL;
		}
		
		if (r->size != reg_params[i].size) {
			LOG_ERROR("Register %s is %d bits instead of %d bits.",
					reg_params[i].reg_name, r->size, reg_params[i].size);
			return ERROR_FAIL;
		}
			
		if (r->number > GDB_REGNO_XPR31) {
			LOG_ERROR("Only GPRs can be use as argument registers.");
			return ERROR_FAIL;
		}
			
		if (r->type->get(r) != ERROR_OK)
			return ERROR_FAIL;
		saved_regs[r->number] = buf_get_u64(r->value, 0, r->size);

		if (reg_params[i].direction == PARAM_OUT || reg_params[i].direction == PARAM_IN_OUT) {
			if (r->type->set(r, reg_params[i].value) != ERROR_OK)
				return ERROR_FAIL;
		}
	}

	/* Disable Interrupts before attempting to run the algorithm. */
	uint64_t current_mstatus;
	uint64_t irq_disabled_mask = MSTATUS_MIE | MSTATUS_HIE | MSTATUS_SIE | MSTATUS_UIE;
	if (riscv_interrupts_disable(target, irq_disabled_mask, &current_mstatus) != ERROR_OK)
		return ERROR_FAIL;

	/* Run algorithm */
	LOG_DEBUG("resume at 0x%" TARGET_PRIxADDR, entry_point);
	if (wch_riscv_resume(target, 0, entry_point, 0, 1, true) != ERROR_OK)
		return ERROR_FAIL;

	int64_t start = timeval_ms();
	while (target->state != TARGET_HALTED) {
		LOG_DEBUG("poll()");
		int64_t now = timeval_ms();
		if (now - start > timeout_ms) {
			LOG_ERROR("Algorithm timed out after %" PRId64 " ms.", now - start);
			wch_riscv_halt(target);
			old_or_new_riscv_poll(target);
			enum gdb_regno regnums[] = {
				GDB_REGNO_RA, GDB_REGNO_SP, GDB_REGNO_GP, GDB_REGNO_TP,
				GDB_REGNO_T0, GDB_REGNO_T1, GDB_REGNO_T2, GDB_REGNO_FP,
				GDB_REGNO_S1, GDB_REGNO_A0, GDB_REGNO_A1, GDB_REGNO_A2,
				GDB_REGNO_A3, GDB_REGNO_A4, GDB_REGNO_A5, GDB_REGNO_A6,
				GDB_REGNO_A7, GDB_REGNO_S2, GDB_REGNO_S3, GDB_REGNO_S4,
				GDB_REGNO_S5, GDB_REGNO_S6, GDB_REGNO_S7, GDB_REGNO_S8,
				GDB_REGNO_S9, GDB_REGNO_S10, GDB_REGNO_S11, GDB_REGNO_T3,
				GDB_REGNO_T4, GDB_REGNO_T5, GDB_REGNO_T6,
				GDB_REGNO_PC,
				GDB_REGNO_MSTATUS, GDB_REGNO_MEPC, GDB_REGNO_MCAUSE,
			};
			for (unsigned i = 0; i < ARRAY_SIZE(regnums); i++) {
				enum gdb_regno regno = regnums[i];
				riscv_reg_t reg_value;
				if (riscv_get_register(target, &reg_value, regno) != ERROR_OK)
					break;
				LOG_ERROR("%s = 0x%" PRIx64, gdb_regno_name(regno), reg_value);
			}
			return ERROR_TARGET_TIMEOUT;
		}

		int result = old_or_new_riscv_poll(target);
		if (result != ERROR_OK)
			return result;
	}

	/* TODO: The current hart id might have been changed in poll(). */
	/* if (riscv_select_current_hart(target) != ERROR_OK)
		return ERROR_FAIL; */
		
	if (reg_pc->type->get(reg_pc) != ERROR_OK)
		return ERROR_FAIL;
	uint64_t final_pc = buf_get_u64(reg_pc->value, 0, reg_pc->size);
	if (exit_point && final_pc != exit_point) {
		LOG_ERROR("PC ended up at 0x%" PRIx64 " instead of 0x%"
				TARGET_PRIxADDR, final_pc, exit_point);
		return ERROR_FAIL;
	}

	/* Restore Interrupts */
	if (riscv_interrupts_restore(target, current_mstatus) != ERROR_OK)
		return ERROR_FAIL;

	/* Restore registers */
	uint8_t buf[8] = { 0 };
	buf_set_u64(buf, 0, info->xlen, saved_pc);
	if (reg_pc->type->set(reg_pc, buf) != ERROR_OK)
		return ERROR_FAIL;

	for (int i = 0; i < num_reg_params; i++) {
		if (reg_params[i].direction == PARAM_IN ||
				reg_params[i].direction == PARAM_IN_OUT) {
			struct reg *r = register_get_by_name(target->reg_cache, reg_params[i].reg_name, false);
			if (r->type->get(r) != ERROR_OK) {
				LOG_ERROR("get(%s) failed", r->name);
				return ERROR_FAIL;
			}
			buf_cpy(r->value, reg_params[i].value, reg_params[i].size);
		}
		LOG_DEBUG("restore %s", reg_params[i].reg_name);
		struct reg *r = register_get_by_name(target->reg_cache, reg_params[i].reg_name, false);
		buf_set_u64(buf, 0, info->xlen, saved_regs[r->number]);
		if (r->type->set(r, buf) != ERROR_OK) {
			LOG_ERROR("set(%s) failed", r->name);
			return ERROR_FAIL;
		}
	}

	return ERROR_OK;
}

static int riscv_checksum_memory(struct target *target,
		target_addr_t address, uint32_t count,
		uint32_t *checksum)
{
	struct working_area *crc_algorithm;
	struct reg_param reg_params[2];
	int retval;

	LOG_DEBUG("address=0x%" TARGET_PRIxADDR "; count=0x%" PRIx32, address, count);

	static const uint8_t riscv32_crc_code[] = {
#include "../../contrib/loaders/checksum/riscv32_crc.inc"
	};
	static const uint8_t riscv64_crc_code[] = {
#include "../../contrib/loaders/checksum/riscv64_crc.inc"
	};

	static const uint8_t *crc_code;

	unsigned xlen = riscv_xlen(target);
	unsigned crc_code_size;
	if (xlen == 32) {
		crc_code = riscv32_crc_code;
		crc_code_size = sizeof(riscv32_crc_code);
	} else {
		crc_code = riscv64_crc_code;
		crc_code_size = sizeof(riscv64_crc_code);
	}

	if (count < crc_code_size * 4) {
		/* Don't use the algorithm for relatively small buffers. It's faster
		 * just to read the memory.  target_checksum_memory() will take care of
		 * that if we fail. */
		return ERROR_FAIL;
	}

	retval = target_alloc_working_area(target, crc_code_size, &crc_algorithm);
	if (retval != ERROR_OK)
		return retval;

	if (crc_algorithm->address + crc_algorithm->size > address &&
			crc_algorithm->address < address + count) {
		/* Region to checksum overlaps with the work area we've been assigned.
		 * Bail. (Would be better to manually checksum what we read there, and
		 * use the algorithm for the rest.) */
		target_free_working_area(target, crc_algorithm);
		return ERROR_FAIL;
	}

	retval = target_write_buffer(target, crc_algorithm->address, crc_code_size,
			crc_code);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to write code to " TARGET_ADDR_FMT ": %d",
				crc_algorithm->address, retval);
		target_free_working_area(target, crc_algorithm);
		return retval;
	}

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, address);
	buf_set_u64(reg_params[1].value, 0, xlen, count);

	/* 20 second timeout/megabyte */
	int timeout = 20000 * (1 + (count / (1024 * 1024)));

	retval = target_run_algorithm(target, 0, NULL, 2, reg_params,
			crc_algorithm->address,
			0,	/* Leave exit point unspecified because we don't know. */
			timeout, NULL);

	if (retval == ERROR_OK)
		*checksum = buf_get_u32(reg_params[0].value, 0, 32);
	else
		LOG_ERROR("error executing RISC-V CRC algorithm");

	destroy_reg_param(&reg_params[0]);
	destroy_reg_param(&reg_params[1]);

	target_free_working_area(target, crc_algorithm);

	LOG_DEBUG("checksum=0x%" PRIx32 ", result=%d", *checksum, retval);

	return retval;
}

/*** OpenOCD Helper Functions ***/

 static enum riscv_poll_hart {
	RPH_NO_CHANGE,
	RPH_DISCOVERED_HALTED,
	RPH_DISCOVERED_RUNNING,
	RPH_ERROR
};
static enum riscv_poll_hart riscv_poll_hart(struct target *target, int hartid)
{
	RISCV_INFO(r);

	LOG_TARGET_DEBUG(target, "polling, target->state=%d", target->state);

	/* If OpenOCD thinks we're running but this hart is halted then it's time
	 * to raise an event. */
	bool halted = riscv_is_halted(target);

	if (halted && timeval_ms() - r->last_activity > 100) {
		/* If we've been idle for a while, flush the register cache. Just in case
		 * OpenOCD is going to be disconnected without shutting down cleanly. */
		if (wch_riscv_flush_registers(target) != ERROR_OK)
			return ERROR_FAIL;
	}

	if (target->state != TARGET_HALTED && halted) {
		LOG_DEBUG("  triggered a halt");
		r->on_halt(target);
		return RPH_DISCOVERED_HALTED;
	} else if (target->state != TARGET_RUNNING && target->state != TARGET_DEBUG_RUNNING && !halted) {
		LOG_DEBUG("  triggered running");
		target->state = TARGET_RUNNING;
		target->debug_reason = DBG_REASON_NOTHALTED;
		return RPH_DISCOVERED_RUNNING;
	}

	return RPH_NO_CHANGE;
}

int wch_sample_memory(struct target *target)
{
	RISCV_INFO(r);

	if (!r->sample_buf.buf || !r->sample_config.enabled)
		return ERROR_OK;

	LOG_DEBUG("buf used/size: %d/%d", r->sample_buf.used, r->sample_buf.size);

	uint64_t start = timeval_ms();
	riscv_sample_buf_maybe_add_timestamp(target, true);
	int result = ERROR_OK;
	if (r->sample_memory) {
		result = r->sample_memory(target, &r->sample_buf, &r->sample_config,
									  start + TARGET_DEFAULT_POLLING_INTERVAL);
		if (result != ERROR_NOT_IMPLEMENTED)
			goto exit;
	}

	/* Default slow path. */
	while (timeval_ms() - start < TARGET_DEFAULT_POLLING_INTERVAL) {
		for (unsigned int i = 0; i < ARRAY_SIZE(r->sample_config.bucket); i++) {
			if (r->sample_config.bucket[i].enabled &&
					r->sample_buf.used + 1 + r->sample_config.bucket[i].size_bytes < r->sample_buf.size) {
				assert(i < RISCV_SAMPLE_BUF_TIMESTAMP_BEFORE);
				r->sample_buf.buf[r->sample_buf.used] = i;
				result = riscv_read_phys_memory(
					target, r->sample_config.bucket[i].address,
					r->sample_config.bucket[i].size_bytes, 1,
					r->sample_buf.buf + r->sample_buf.used + 1);
				if (result == ERROR_OK)
					r->sample_buf.used += 1 + r->sample_config.bucket[i].size_bytes;
				else
					goto exit;
			}
		}
	}

exit:
	riscv_sample_buf_maybe_add_timestamp(target, false);
	if (result != ERROR_OK) {
		LOG_INFO("Turning off memory sampling because it failed.");
		r->sample_config.enabled = false;
	}
	return result;
}

/*** OpenOCD Interface ***/
int wch_riscv_openocd_poll(struct target *target)
{
	LOG_DEBUG("polling all harts");
	enum target_state old_state = target->state;

	if (target->smp) {
		unsigned should_remain_halted = 0;
		unsigned should_resume = 0;
		struct target_list *list;
		foreach_smp_target(list, target->smp_targets) {
			struct target *t = list->target;
			if (!target_was_examined(t))
				continue;
			enum riscv_poll_hart out = riscv_poll_hart(t, t->coreid);
			switch (out) {
			case RPH_NO_CHANGE:
				break;
			case RPH_DISCOVERED_RUNNING:
				t->state = TARGET_RUNNING;
				t->debug_reason = DBG_REASON_NOTHALTED;
				break;
			case RPH_DISCOVERED_HALTED:
				t->state = TARGET_HALTED;
				enum riscv_halt_reason halt_reason =
					riscv_halt_reason(t);
				if (set_debug_reason(t, halt_reason) != ERROR_OK)
					return ERROR_FAIL;

				if (halt_reason == RISCV_HALT_BREAKPOINT) {
					int retval;
					switch (riscv_semihosting(t, &retval)) {
					case SEMI_NONE:
					case SEMI_WAITING:
						/* This hart should remain halted. */
						should_remain_halted++;
						break;
					case SEMI_HANDLED:
						/* This hart should be resumed, along with any other
							 * harts that halted due to haltgroups. */
						should_resume++;
						break;
					case SEMI_ERROR:
						return retval;
					}
				} else if (halt_reason != RISCV_HALT_GROUP) {
					should_remain_halted++;
				}
				break;

			case RPH_ERROR:
				return ERROR_FAIL;
			}
		}

		LOG_DEBUG("should_remain_halted=%d, should_resume=%d",
				  should_remain_halted, should_resume);
		if (should_remain_halted && should_resume) {
			LOG_WARNING("%d harts should remain halted, and %d should resume.",
						should_remain_halted, should_resume);
		}
		if (should_remain_halted) {
			LOG_DEBUG("halt all");
			wch_riscv_halt(target);
		} else if (should_resume) {
			LOG_DEBUG("resume all");
			wch_riscv_resume(target, true, 0, 0, 0, false);
		}

		/* Sample memory if any target is running. */
		foreach_smp_target(list, target->smp_targets) {
			struct target *t = list->target;
			if (t->state == TARGET_RUNNING) {
				wch_sample_memory(target);
				break;
			}
		}

		return ERROR_OK;

	} else {
		enum riscv_poll_hart out = riscv_poll_hart(target, target->coreid);
		if (out == RPH_NO_CHANGE || out == RPH_DISCOVERED_RUNNING) {
			if (target->state == TARGET_RUNNING)
				wch_sample_memory(target);
			return ERROR_OK;
		} else if (out == RPH_ERROR) {
			return ERROR_FAIL;
		}

		LOG_TARGET_DEBUG(target, "hart halted");

		target->state = TARGET_HALTED;
		enum riscv_halt_reason halt_reason = riscv_halt_reason(target);
		if (set_debug_reason(target, halt_reason) != ERROR_OK)
			return ERROR_FAIL;
		target->state = TARGET_HALTED;
	}

	if (target->debug_reason == DBG_REASON_BREAKPOINT) {
		int retval;
		switch (riscv_semihosting(target, &retval)) {
			case SEMI_NONE:
			case SEMI_WAITING:
				target_call_event_callbacks(target, TARGET_EVENT_HALTED);
				break;
			case SEMI_HANDLED:
				if (wch_riscv_resume(target, true, 0, 0, 0, false) != ERROR_OK)
					return ERROR_FAIL;
				break;
			case SEMI_ERROR:
				return retval;
		}
	} else {
		if (old_state == TARGET_DEBUG_RUNNING)
			target_call_event_callbacks(target, TARGET_EVENT_DEBUG_HALTED);
		else
			target_call_event_callbacks(target, TARGET_EVENT_HALTED);
	}

	return ERROR_OK;
}

int wch_riscv_openocd_step(struct target *target, int current,
	target_addr_t address, int handle_breakpoints)
{
	LOG_TARGET_DEBUG(target, "stepping hart");

	if (!current) {
		if (riscv_set_register(target, GDB_REGNO_PC, address) != ERROR_OK)
			return ERROR_FAIL;
	}

	struct breakpoint *breakpoint = NULL;
	/* the front-end may request us not to handle breakpoints */
	if (handle_breakpoints) {
		if (current) {
			if (riscv_get_register(target, &address, GDB_REGNO_PC) != ERROR_OK)
				return ERROR_FAIL;
		}
		breakpoint = breakpoint_find(target, address);
		if (breakpoint && (wch_riscv_remove_breakpoint(target, breakpoint) != ERROR_OK))
			return ERROR_FAIL;
	}

	riscv_reg_t trigger_state[RISCV_MAX_HWBPS] = {0};
	if (disable_triggers(target, trigger_state) != ERROR_OK)
		return ERROR_FAIL;

	bool success = true;
	uint64_t current_mstatus;
	RISCV_INFO(info);

	if (info->isrmask_mode == RISCV_ISRMASK_STEPONLY) {
		/* Disable Interrupts before stepping. */
		uint64_t irq_disabled_mask = MSTATUS_MIE | MSTATUS_HIE | MSTATUS_SIE | MSTATUS_UIE;
		if (riscv_interrupts_disable(target, irq_disabled_mask,
				&current_mstatus) != ERROR_OK) {
			success = false;
			LOG_ERROR("unable to disable interrupts");
			goto _exit;
		}
	}

	if (riscv_step_rtos_hart(target) != ERROR_OK) {
		success = false;
		LOG_ERROR("unable to step rtos hart");
	}

	register_cache_invalidate(target->reg_cache);

	if (info->isrmask_mode == RISCV_ISRMASK_STEPONLY)
		if (riscv_interrupts_restore(target, current_mstatus) != ERROR_OK) {
			success = false;
			LOG_ERROR("unable to restore interrupts");
		}

_exit:
	if (enable_triggers(target, trigger_state) != ERROR_OK) {
		success = false;
		LOG_ERROR("unable to enable triggers");
	}

	if (breakpoint && (wch_riscv_add_breakpoint(target, breakpoint) != ERROR_OK)) {
		success = false;
		LOG_TARGET_ERROR(target, "unable to restore the disabled breakpoint");
	}

	if (success) {
		target->state = TARGET_RUNNING;
		target_call_event_callbacks(target, TARGET_EVENT_RESUMED);
		target->state = TARGET_HALTED;
		target->debug_reason = DBG_REASON_SINGLESTEP;
		target_call_event_callbacks(target, TARGET_EVENT_HALTED);
	}
	return success ? ERROR_OK : ERROR_FAIL;
}

/* Command Handlers */
COMMAND_HANDLER(wch_riscv_set_command_timeout_sec)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	int timeout = atoi(CMD_ARGV[0]);
	if (timeout <= 0) {
		LOG_ERROR("%s is not a valid integer argument for command.", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	riscv_command_timeout_sec = timeout;

	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_reset_timeout_sec)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	int timeout = atoi(CMD_ARGV[0]);
	if (timeout <= 0) {
		LOG_ERROR("%s is not a valid integer argument for command.", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	riscv_reset_timeout_sec = timeout;
	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_prefer_sba)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);
	bool prefer_sba;
	LOG_WARNING("`riscv set_prefer_sba` is deprecated. Please use `riscv set_mem_access` instead.");
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], prefer_sba);
	if (prefer_sba) {
		/* Use system bus with highest priority */
		r->mem_access_methods[0] = RISCV_MEM_ACCESS_SYSBUS;
		r->mem_access_methods[1] = RISCV_MEM_ACCESS_PROGBUF;
		r->mem_access_methods[2] = RISCV_MEM_ACCESS_ABSTRACT;
	} else {
		/* Use progbuf with highest priority */
		r->mem_access_methods[0] = RISCV_MEM_ACCESS_PROGBUF;
		r->mem_access_methods[1] = RISCV_MEM_ACCESS_SYSBUS;
		r->mem_access_methods[2] = RISCV_MEM_ACCESS_ABSTRACT;
	}

	/* Reset warning flags */
	r->mem_access_progbuf_warn = true;
	r->mem_access_sysbus_warn = true;
	r->mem_access_abstract_warn = true;

	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_mem_access)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);
	int progbuf_cnt = 0;
	int sysbus_cnt = 0;
	int abstract_cnt = 0;

	if (CMD_ARGC < 1 || CMD_ARGC > RISCV_NUM_MEM_ACCESS_METHODS) {
		LOG_ERROR("Command takes 1 to %d parameters", RISCV_NUM_MEM_ACCESS_METHODS);
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	/* Check argument validity */
	for (unsigned int i = 0; i < CMD_ARGC; i++) {
		if (strcmp("progbuf", CMD_ARGV[i]) == 0) {
			progbuf_cnt++;
		} else if (strcmp("sysbus", CMD_ARGV[i]) == 0) {
			sysbus_cnt++;
		} else if (strcmp("abstract", CMD_ARGV[i]) == 0) {
			abstract_cnt++;
		} else {
			LOG_ERROR("Unknown argument '%s'. "
				"Must be one of: 'progbuf', 'sysbus' or 'abstract'.", CMD_ARGV[i]);
			return ERROR_COMMAND_SYNTAX_ERROR;
		}
	}
	if (progbuf_cnt > 1 || sysbus_cnt > 1 || abstract_cnt > 1) {
		LOG_ERROR("Syntax error - duplicate arguments to `riscv set_mem_access`.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	/* Args are valid, store them */
	for (unsigned int i = 0; i < RISCV_NUM_MEM_ACCESS_METHODS; i++)
		r->mem_access_methods[i] = RISCV_MEM_ACCESS_UNSPECIFIED;
	for (unsigned int i = 0; i < CMD_ARGC; i++) {
		if (strcmp("progbuf", CMD_ARGV[i]) == 0)
			r->mem_access_methods[i] = RISCV_MEM_ACCESS_PROGBUF;
		else if (strcmp("sysbus", CMD_ARGV[i]) == 0)
			r->mem_access_methods[i] = RISCV_MEM_ACCESS_SYSBUS;
		else if (strcmp("abstract", CMD_ARGV[i]) == 0)
			r->mem_access_methods[i] = RISCV_MEM_ACCESS_ABSTRACT;
	}

	/* Reset warning flags */
	r->mem_access_progbuf_warn = true;
	r->mem_access_sysbus_warn = true;
	r->mem_access_abstract_warn = true;

	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_enable_virtual)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], riscv_enable_virtual);
	return ERROR_OK;
}


COMMAND_HANDLER(wch_riscv_set_expose_csrs)
{
	if (CMD_ARGC == 0) {
		LOG_ERROR("Command expects parameters");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(info);
	int ret = ERROR_OK;

	for (unsigned int i = 0; i < CMD_ARGC; i++) {
		ret = parse_ranges(&info->expose_csr, CMD_ARGV[i], "csr", 0xfff);
		if (ret != ERROR_OK)
			break;
	}

	return ret;
}

COMMAND_HANDLER(wch_riscv_set_expose_custom)
{
	if (CMD_ARGC == 0) {
		LOG_ERROR("Command expects parameters");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(info);
	int ret = ERROR_OK;

	for (unsigned int i = 0; i < CMD_ARGC; i++) {
		ret = parse_ranges(&info->expose_custom, CMD_ARGV[i], "custom", 0x3fff);
		if (ret != ERROR_OK)
			break;
	}

	return ret;
}

COMMAND_HANDLER(wch_riscv_authdata_read)
{
	unsigned int index = 0;
	if (CMD_ARGC == 0) {
		/* nop */
	} else if (CMD_ARGC == 1) {
		COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], index);
	} else {
		LOG_ERROR("Command takes at most one parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	if (!target) {
		LOG_ERROR("target is NULL!");
		return ERROR_FAIL;
	}

	RISCV_INFO(r);
	if (!r) {
		LOG_ERROR("riscv_info is NULL!");
		return ERROR_FAIL;
	}

	if (r->authdata_read) {
		uint32_t value;
		if (r->authdata_read(target, &value, index) != ERROR_OK)
			return ERROR_FAIL;
		command_print_sameline(CMD, "0x%08" PRIx32, value);
		return ERROR_OK;
	} else {
		LOG_ERROR("authdata_read is not implemented for this target.");
		return ERROR_FAIL;
	}
}

COMMAND_HANDLER(wch_riscv_authdata_write)
{
	uint32_t value;
	unsigned int index = 0;

	if (CMD_ARGC == 0) {
		/* nop */
	} else if (CMD_ARGC == 1) {
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], value);
	} else if (CMD_ARGC == 2) {
		COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], index);
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], value);
	} else {
		LOG_ERROR("Command takes at most 2 arguments");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	if (r->authdata_write) {
		return r->authdata_write(target, value, index);
	} else {
		LOG_ERROR("authdata_write is not implemented for this target.");
		return ERROR_FAIL;
	}
}

COMMAND_HANDLER(wch_riscv_dmi_read)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	if (!target) {
		LOG_ERROR("target is NULL!");
		return ERROR_FAIL;
	}

	RISCV_INFO(r);
	if (!r) {
		LOG_ERROR("riscv_info is NULL!");
		return ERROR_FAIL;
	}

	if (r->dmi_read) {
		uint32_t address, value;
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], address);
		if (r->dmi_read(target, &value, address) != ERROR_OK)
			return ERROR_FAIL;
		command_print(CMD, "0x%" PRIx32, value);
		return ERROR_OK;
	} else {
		LOG_ERROR("dmi_read is not implemented for this target.");
		return ERROR_FAIL;
	}
}


COMMAND_HANDLER(wch_riscv_dmi_write)
{
	if (CMD_ARGC != 2) {
		LOG_ERROR("Command takes exactly 2 arguments");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	uint32_t address, value;
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], address);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], value);

	if (r->dmi_write) {
		/* Perform the DMI write */
		int retval = r->dmi_write(target, address, value);

		/* Invalidate our cached progbuf copy:
		   - if the user tinkered directly with a progbuf register
		   - if debug module was reset, in which case progbuf registers
		     may not retain their value.
		*/
		bool progbufTouched = (address >= DM_PROGBUF0 && address <= DM_PROGBUF15);
		bool dmDeactivated = (address == DM_DMCONTROL && (value & DM_DMCONTROL_DMACTIVE) == 0);
		if (progbufTouched || dmDeactivated) {
			if (r->invalidate_cached_debug_buffer)
				r->invalidate_cached_debug_buffer(target);
		}

		return retval;
	}

	LOG_ERROR("dmi_write is not implemented for this target.");
	return ERROR_FAIL;
}

COMMAND_HANDLER(wch_riscv_test_sba_config_reg)
{
	if (CMD_ARGC != 4) {
		LOG_ERROR("Command takes exactly 4 arguments");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	target_addr_t legal_address;
	uint32_t num_words;
	target_addr_t illegal_address;
	bool run_sbbusyerror_test;

	COMMAND_PARSE_NUMBER(target_addr, CMD_ARGV[0], legal_address);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], num_words);
	COMMAND_PARSE_NUMBER(target_addr, CMD_ARGV[2], illegal_address);
	COMMAND_PARSE_ON_OFF(CMD_ARGV[3], run_sbbusyerror_test);

	if (r->test_sba_config_reg) {
		return r->test_sba_config_reg(target, legal_address, num_words,
				illegal_address, run_sbbusyerror_test);
	} else {
		LOG_ERROR("test_sba_config_reg is not implemented for this target.");
		return ERROR_FAIL;
	}
}

COMMAND_HANDLER(wch_riscv_reset_delays)
{
	int wait = 0;

	if (CMD_ARGC > 1) {
		LOG_ERROR("Command takes at most one argument");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	if (CMD_ARGC == 1)
		COMMAND_PARSE_NUMBER(int, CMD_ARGV[0], wait);

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);
	r->reset_delays_wait = wait;
	return ERROR_OK;
}



COMMAND_HANDLER(wch_riscv_resume_order)
{
	if (CMD_ARGC > 1) {
		LOG_ERROR("Command takes at most one argument");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	if (!strcmp(CMD_ARGV[0], "normal")) {
		resume_order = RO_NORMAL;
	} else if (!strcmp(CMD_ARGV[0], "reversed")) {
		resume_order = RO_REVERSED;
	} else {
		LOG_ERROR("Unsupported resume order: %s", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}




COMMAND_HANDLER(wch_riscv_set_maskisr)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(info);

	static const struct jim_nvp nvp_maskisr_modes[] = {
		{ .name = "off", .value = RISCV_ISRMASK_OFF },
		{ .name = "steponly", .value = RISCV_ISRMASK_STEPONLY },
		{ .name = NULL, .value = -1 },
	};
	const struct jim_nvp *n;

	if (CMD_ARGC > 0) {
		n = jim_nvp_name2value_simple(nvp_maskisr_modes, CMD_ARGV[0]);
		if (!n->name)
			return ERROR_COMMAND_SYNTAX_ERROR;
		info->isrmask_mode = n->value;
	} else {
		n = jim_nvp_value2name_simple(nvp_maskisr_modes, info->isrmask_mode);
		command_print(CMD, "riscv interrupt mask %s", n->name);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_enable_virt2phys)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], riscv_enable_virt2phys);
	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_ebreakm)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], riscv_ebreakm);
	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_ebreaks)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], riscv_ebreaks);
	return ERROR_OK;
}

COMMAND_HANDLER(wch_riscv_set_ebreaku)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], riscv_ebreaku);
	return ERROR_OK;
}

COMMAND_HANDLER(wch_handle_repeat_read)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	if (CMD_ARGC < 2) {
		LOG_ERROR("Command requires at least count and address arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	if (CMD_ARGC > 3) {
		LOG_ERROR("Command takes at most 3 arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	uint32_t count;
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], count);
	target_addr_t address;
	COMMAND_PARSE_ADDRESS(CMD_ARGV[1], address);
	uint32_t size = 4;
	if (CMD_ARGC > 2)
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], size);

	if (count == 0)
		return ERROR_OK;

	uint8_t *buffer = malloc(size * count);
	if (!buffer) {
		LOG_ERROR("malloc failed");
		return ERROR_FAIL;
	}
	int result = r->read_memory(target, address, size, count, buffer, 0);
	if (result == ERROR_OK) {
		target_handle_md_output(cmd, target, address, size, count, buffer,
			false);
	}
	free(buffer);
	return result;
}

COMMAND_HANDLER(wch_handle_memory_sample_command)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	if (CMD_ARGC == 0) {
		command_print(CMD, "Memory sample configuration for %s:", target_name(target));
		for (unsigned i = 0; i < ARRAY_SIZE(r->sample_config.bucket); i++) {
			if (r->sample_config.bucket[i].enabled) {
				command_print(CMD, "bucket %d; address=0x%" TARGET_PRIxADDR "; size=%d", i,
							  r->sample_config.bucket[i].address,
							  r->sample_config.bucket[i].size_bytes);
			} else {
				command_print(CMD, "bucket %d; disabled", i);
			}
		}
		return ERROR_OK;
	}

	if (CMD_ARGC < 2) {
		LOG_ERROR("Command requires at least bucket and address arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	uint32_t bucket;
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], bucket);
	if (bucket > ARRAY_SIZE(r->sample_config.bucket)) {
		LOG_ERROR("Max bucket number is %d.", (unsigned) ARRAY_SIZE(r->sample_config.bucket));
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	if (!strcmp(CMD_ARGV[1], "clear")) {
		r->sample_config.bucket[bucket].enabled = false;
	} else {
		COMMAND_PARSE_ADDRESS(CMD_ARGV[1], r->sample_config.bucket[bucket].address);

		if (CMD_ARGC > 2) {
			COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], r->sample_config.bucket[bucket].size_bytes);
			if (r->sample_config.bucket[bucket].size_bytes != 4 &&
					r->sample_config.bucket[bucket].size_bytes != 8) {
				LOG_ERROR("Only 4-byte and 8-byte sizes are supported.");
				return ERROR_COMMAND_ARGUMENT_INVALID;
			}
		} else {
			r->sample_config.bucket[bucket].size_bytes = 4;
		}

		r->sample_config.bucket[bucket].enabled = true;
	}

	if (!r->sample_buf.buf) {
		r->sample_buf.size = 1024 * 1024;
		r->sample_buf.buf = malloc(r->sample_buf.size);
	}

	/* Clear the buffer when the configuration is changed. */
	r->sample_buf.used = 0;

	r->sample_config.enabled = true;

	return ERROR_OK;
}

COMMAND_HANDLER(wch_handle_dump_sample_buf_command)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	if (CMD_ARGC > 1) {
		LOG_ERROR("Command takes at most 1 arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	bool base64 = false;
	if (CMD_ARGC > 0) {
		if (!strcmp(CMD_ARGV[0], "base64")) {
			base64 = true;
		} else {
			LOG_ERROR("Unknown argument: %s", CMD_ARGV[0]);
			return ERROR_COMMAND_SYNTAX_ERROR;
		}
	}

	int result = ERROR_OK;
	if (base64) {
		unsigned char *encoded = base64_encode(r->sample_buf.buf,
									  r->sample_buf.used, NULL);
		if (!encoded) {
			LOG_ERROR("Failed base64 encode!");
			result = ERROR_FAIL;
			goto error;
		}
		command_print(CMD, "%s", encoded);
		free(encoded);
	} else {
		unsigned i = 0;
		while (i < r->sample_buf.used) {
			uint8_t command = r->sample_buf.buf[i++];
			if (command == RISCV_SAMPLE_BUF_TIMESTAMP_BEFORE) {
				uint32_t timestamp = buf_get_u32(r->sample_buf.buf + i, 0, 32);
				i += 4;
				command_print(CMD, "timestamp before: %u", timestamp);
			} else if (command == RISCV_SAMPLE_BUF_TIMESTAMP_AFTER) {
				uint32_t timestamp = buf_get_u32(r->sample_buf.buf + i, 0, 32);
				i += 4;
				command_print(CMD, "timestamp after: %u", timestamp);
			} else if (command < ARRAY_SIZE(r->sample_config.bucket)) {
				command_print_sameline(CMD, "0x%" TARGET_PRIxADDR ": ",
									   r->sample_config.bucket[command].address);
				if (r->sample_config.bucket[command].size_bytes == 4) {
					uint32_t value = buf_get_u32(r->sample_buf.buf + i, 0, 32);
					i += 4;
					command_print(CMD, "0x%08" PRIx32, value);
				} else if (r->sample_config.bucket[command].size_bytes == 8) {
					uint64_t value = buf_get_u64(r->sample_buf.buf + i, 0, 64);
					i += 8;
					command_print(CMD, "0x%016" PRIx64, value);
				} else {
					LOG_ERROR("Found invalid size in bucket %d: %d", command,
							  r->sample_config.bucket[command].size_bytes);
					result = ERROR_FAIL;
					goto error;
				}
			} else {
				LOG_ERROR("Found invalid command byte in sample buf: 0x%2x at offset 0x%x",
					command, i - 1);
				result = ERROR_FAIL;
				goto error;
			}
		}
	}

error:
	/* Clear the sample buffer even when there was an error. */
	r->sample_buf.used = 0;
	return result;
}



COMMAND_HANDLER(wch_handle_info)
{
	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);

	/* This output format can be fed directly into TCL's "array set". */

	riscv_print_info_line(CMD, "hart", "xlen", riscv_xlen(target));
	riscv_enumerate_triggers(target);
	riscv_print_info_line(CMD, "hart", "trigger_count",
						  r->trigger_count);

	if (r->print_info)
		return CALL_COMMAND_HANDLER(r->print_info, target);

	return 0;
}

static const struct command_registration riscv_exec_command_handlers[] = {
	{
		.name = "dump_sample_buf",
		.handler = wch_handle_dump_sample_buf_command,
		.mode = COMMAND_ANY,
		.usage = "[base64]",
		.help = "Print the contents of the sample buffer, and clear the buffer."
	},
	{
		.name = "info",
		.handler = wch_handle_info,
		.mode = COMMAND_ANY,
		.usage = "",
		.help = "Displays some information OpenOCD detected about the target."
	},
	{
		.name = "memory_sample",
		.handler = wch_handle_memory_sample_command,
		.mode = COMMAND_ANY,
		.usage = "bucket address|clear [size=4]",
		.help = "Causes OpenOCD to frequently read size bytes at the given address."
	},
	{
		.name = "repeat_read",
		.handler = wch_handle_repeat_read,
		.mode = COMMAND_ANY,
		.usage = "count address [size=4]",
		.help = "Repeatedly read the value at address."
	},
	{
		.name = "set_command_timeout_sec",
		.handler = wch_riscv_set_command_timeout_sec,
		.mode = COMMAND_ANY,
		.usage = "[sec]",
		.help = "Set the wall-clock timeout (in seconds) for individual commands"
	},
	{
		.name = "set_reset_timeout_sec",
		.handler = wch_riscv_set_reset_timeout_sec,
		.mode = COMMAND_ANY,
		.usage = "[sec]",
		.help = "Set the wall-clock timeout (in seconds) after reset is deasserted"
	},
	{
		.name = "set_prefer_sba",
		.handler = wch_riscv_set_prefer_sba,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "When on, prefer to use System Bus Access to access memory. "
			"When off (default), prefer to use the Program Buffer to access memory."
	},
	{
		.name = "set_mem_access",
		.handler = wch_riscv_set_mem_access,
		.mode = COMMAND_ANY,
		.usage = "method1 [method2] [method3]",
		.help = "Set which memory access methods shall be used and in which order "
			"of priority. Method can be one of: 'progbuf', 'sysbus' or 'abstract'."
	},
	{
		.name = "set_enable_virtual",
		.handler = wch_riscv_set_enable_virtual,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "When on, memory accesses are performed on physical or virtual "
				"memory depending on the current system configuration. "
				"When off (default), all memory accessses are performed on physical memory."
	},
	{
		.name = "expose_csrs",
		.handler = wch_riscv_set_expose_csrs,
		.mode = COMMAND_CONFIG,
		.usage = "n0[-m0|=name0][,n1[-m1|=name1]]...",
		.help = "Configure a list of inclusive ranges for CSRs to expose in "
				"addition to the standard ones. This must be executed before "
				"`init`."
	},
	{
		.name = "expose_custom",
		.handler = wch_riscv_set_expose_custom,
		.mode = COMMAND_CONFIG,
		.usage = "n0[-m0|=name0][,n1[-m1|=name1]]...",
		.help = "Configure a list of inclusive ranges for custom registers to "
			"expose. custom0 is accessed as abstract register number 0xc000, "
			"etc. This must be executed before `init`."
	},
	{
		.name = "authdata_read",
		.handler = wch_riscv_authdata_read,
		.usage = "[index]",
		.mode = COMMAND_ANY,
		.help = "Return the 32-bit value read from authdata or authdata0 "
				"(index=0), or authdata1 (index=1)."
	},
	{
		.name = "authdata_write",
		.handler = wch_riscv_authdata_write,
		.mode = COMMAND_ANY,
		.usage = "[index] value",
		.help = "Write the 32-bit value to authdata or authdata0 (index=0), "
				"or authdata1 (index=1)."
	},
	{
		.name = "dmi_read",
		.handler = wch_riscv_dmi_read,
		.mode = COMMAND_ANY,
		.usage = "address",
		.help = "Perform a 32-bit DMI read at address, returning the value."
	},
	{
		.name = "dmi_write",
		.handler = wch_riscv_dmi_write,
		.mode = COMMAND_ANY,
		.usage = "address value",
		.help = "Perform a 32-bit DMI write of value at address."
	},
	{
		.name = "test_sba_config_reg",
		.handler = wch_riscv_test_sba_config_reg,
		.mode = COMMAND_ANY,
		.usage = "legal_address num_words "
			"illegal_address run_sbbusyerror_test[on/off]",
		.help = "Perform a series of tests on the SBCS register. "
			"Inputs are a legal, 128-byte aligned address and a number of words to "
			"read/write starting at that address (i.e., address range [legal address, "
			"legal_address+word_size*num_words) must be legally readable/writable), "
			"an illegal, 128-byte aligned address for error flag/handling cases, "
			"and whether sbbusyerror test should be run."
	},
	{
		.name = "reset_delays",
		.handler = wch_riscv_reset_delays,
		.mode = COMMAND_ANY,
		.usage = "[wait]",
		.help = "OpenOCD learns how many Run-Test/Idle cycles are required "
			"between scans to avoid encountering the target being busy. This "
			"command resets those learned values after `wait` scans. It's only "
			"useful for testing OpenOCD itself."
	},
	{
		.name = "resume_order",
		.handler = wch_riscv_resume_order,
		.mode = COMMAND_ANY,
		.usage = "normal|reversed",
		.help = "Choose the order that harts are resumed in when `hasel` is not "
			"supported. Normal order is from lowest hart index to highest. "
			"Reversed order is from highest hart index to lowest."
	},
	
	{
		.name = "set_maskisr",
		.handler = wch_riscv_set_maskisr,
		.mode = COMMAND_EXEC,
		.help = "mask riscv interrupts",
		.usage = "['off'|'steponly']",
	},
	{
		.name = "set_enable_virt2phys",
		.handler = wch_riscv_set_enable_virt2phys,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "When on (default), enable translation from virtual address to "
			"physical address."
	},
	{
		.name = "set_ebreakm",
		.handler = wch_riscv_set_ebreakm,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "Control dcsr.ebreakm. When off, M-mode ebreak instructions "
			"don't trap to OpenOCD. Defaults to on."
	},
	{
		.name = "set_ebreaks",
		.handler = wch_riscv_set_ebreaks,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "Control dcsr.ebreaks. When off, S-mode ebreak instructions "
			"don't trap to OpenOCD. Defaults to on."
	},
	{
		.name = "set_ebreaku",
		.handler = wch_riscv_set_ebreaku,
		.mode = COMMAND_ANY,
		.usage = "on|off",
		.help = "Control dcsr.ebreaku. When off, U-mode ebreak instructions "
			"don't trap to OpenOCD. Defaults to on."
	},
	COMMAND_REGISTRATION_DONE
};

/*
 * To be noted that RISC-V targets use the same semihosting commands as
 * ARM targets.
 *
 * The main reason is compatibility with existing tools. For example the
 * Eclipse OpenOCD/SEGGER J-Link/QEMU plug-ins have several widgets to
 * configure semihosting, which generate commands like `arm semihosting
 * enable`.
 * A secondary reason is the fact that the protocol used is exactly the
 * one specified by ARM. If RISC-V will ever define its own semihosting
 * protocol, then a command like `riscv semihosting enable` will make
 * sense, but for now all semihosting commands are prefixed with `arm`.
 */
extern const struct command_registration semihosting_common_handlers[];

const struct command_registration wch_riscv_command_handlers[] = {
	{
		.name = "riscv",
		.mode = COMMAND_ANY,
		.help = "RISC-V Command Group",
		.usage = "",
		.chain = riscv_exec_command_handlers
	},
	{
		.name = "arm",
		.mode = COMMAND_ANY,
		.help = "ARM Command Group",
		.usage = "",
		.chain = semihosting_common_handlers
	},
	COMMAND_REGISTRATION_DONE
};

static unsigned riscv_xlen_nonconst(struct target *target)
{
	return riscv_xlen(target);
}

static unsigned int riscv_data_bits(struct target *target)
{
	RISCV_INFO(r);
	if (r->data_bits)
		return r->data_bits(target);
	return riscv_xlen(target);
}

struct target_type wch_riscv_target = {
	.name = "wch_riscv",

	.target_create = wch_riscv_create_target,
	.init_target = wch_riscv_init_target,
	.deinit_target = wch_riscv_deinit_target,
	.examine = wch_riscv_examine,

	/* poll current target status */
	.poll = old_or_new_riscv_poll,

	.halt = wch_riscv_halt,
	.resume = riscv_target_resume,
	.step = old_or_new_riscv_step,

	.assert_reset = riscv_assert_reset,
	.deassert_reset = riscv_deassert_reset,

	.read_memory = riscv_read_memory,
	.write_memory = riscv_write_memory,
	.read_phys_memory = riscv_read_phys_memory,
	.write_phys_memory = riscv_write_phys_memory,

	.checksum_memory = riscv_checksum_memory,

	.mmu = riscv_mmu,
	.virt2phys = riscv_virt2phys,

	.get_gdb_arch = riscv_get_gdb_arch,
	.get_gdb_reg_list = riscv_get_gdb_reg_list,
	.get_gdb_reg_list_noread = riscv_get_gdb_reg_list_noread,

	.add_breakpoint = wch_riscv_add_breakpoint,
	.remove_breakpoint = wch_riscv_remove_breakpoint,

	.add_watchpoint = wch_riscv_add_watchpoint,
	.remove_watchpoint = wch_riscv_remove_watchpoint,
	.hit_watchpoint = riscv_hit_watchpoint,

	.arch_state = riscv_arch_state,

	.run_algorithm = riscv_run_algorithm,

	.commands = wch_riscv_command_handlers,

	.address_bits = riscv_xlen_nonconst,
	.data_bits = riscv_data_bits
};



static int riscv_resume_go_all_harts(struct target *target)
{
	RISCV_INFO(r);

	LOG_TARGET_DEBUG(target, "resuming hart, state=%d", target->state);
	if (riscv_is_halted(target)) {
		if (r->resume_go(target) != ERROR_OK)
			return ERROR_FAIL;
	} else {
		LOG_DEBUG("[%s] hart requested resume, but was already resumed",
				target_name(target));
	}

	riscv_invalidate_register_cache(target);
	return ERROR_OK;
}





/**
 * If write is true:
 *   return true iff we are guaranteed that the register will contain exactly
 *       the value we just wrote when it's read.
 * If write is false:
 *   return true iff we are guaranteed that the register will read the same
 *       value in the future as the value we just read.
 */
static bool gdb_regno_cacheable(enum gdb_regno regno, bool write)
{
	/* GPRs, FPRs, vector registers are just normal data stores. */
	if (regno <= GDB_REGNO_XPR31 ||
			(regno >= GDB_REGNO_FPR0 && regno <= GDB_REGNO_FPR31) ||
			(regno >= GDB_REGNO_V0 && regno <= GDB_REGNO_V31))
		return true;

	/* Most CSRs won't change value on us, but we can't assume it about arbitrary
	 * CSRs. */
	switch (regno) {
		case GDB_REGNO_DPC:
			return true;

		case GDB_REGNO_VSTART:
		case GDB_REGNO_VXSAT:
		case GDB_REGNO_VXRM:
		case GDB_REGNO_VLENB:
		case GDB_REGNO_VL:
		case GDB_REGNO_VTYPE:
		case GDB_REGNO_MISA:
		case GDB_REGNO_DCSR:
		case GDB_REGNO_DSCRATCH0:
		case GDB_REGNO_MSTATUS:
		case GDB_REGNO_MEPC:
		case GDB_REGNO_MCAUSE:
		case GDB_REGNO_SATP:
			/*
			 * WARL registers might not contain the value we just wrote, but
			 * these ones won't spontaneously change their value either. *
			 */
			return !write;

		case GDB_REGNO_TSELECT:	/* I think this should be above, but then it doesn't work. */
		case GDB_REGNO_TDATA1:	/* Changes value when tselect is changed. */
		case GDB_REGNO_TDATA2:  /* Changse value when tselect is changed. */
		default:
			return false;
	}
}



static int register_get(struct reg *reg)
{
	riscv_reg_info_t *reg_info = reg->arch_info;
	struct target *target = reg_info->target;
	RISCV_INFO(r);

	if (reg->number >= GDB_REGNO_V0 && reg->number <= GDB_REGNO_V31) {
		if (!r->get_register_buf) {
			LOG_ERROR("Reading register %s not supported on this RISC-V target.",
					gdb_regno_name(reg->number));
			return ERROR_FAIL;
		}

		if (r->get_register_buf(target, reg->value, reg->number) != ERROR_OK)
			return ERROR_FAIL;
	} else {
		uint64_t value;
		int result = riscv_get_register(target, &value, reg->number);
		if (result != ERROR_OK)
			return result;
		buf_set_u64(reg->value, 0, reg->size, value);
	}
	reg->valid = gdb_regno_cacheable(reg->number, false);
	char *str = buf_to_hex_str(reg->value, reg->size);
	LOG_DEBUG("[%s] read 0x%s from %s (valid=%d)", target_name(target),
			str, reg->name, reg->valid);
	free(str);
	return ERROR_OK;
}

static int register_set(struct reg *reg, uint8_t *buf)
{
	riscv_reg_info_t *reg_info = reg->arch_info;
	struct target *target = reg_info->target;
	RISCV_INFO(r);

	char *str = buf_to_hex_str(buf, reg->size);
	LOG_DEBUG("[%s] write 0x%s to %s (valid=%d)", target_name(target),
			str, reg->name, reg->valid);
	free(str);

	/* Exit early for writing x0, which on the hardware would be ignored, and we
	 * don't want to update our cache. */
	if (reg->number == GDB_REGNO_ZERO)
		return ERROR_OK;

	memcpy(reg->value, buf, DIV_ROUND_UP(reg->size, 8));
	reg->valid = gdb_regno_cacheable(reg->number, true);

	if (reg->number == GDB_REGNO_TDATA1 ||
			reg->number == GDB_REGNO_TDATA2) {
		r->manual_hwbp_set = true;
		/* When enumerating triggers, we clear any triggers with DMODE set,
		 * assuming they were left over from a previous debug session. So make
		 * sure that is done before a user might be setting their own triggers.
		 */
		if (riscv_enumerate_triggers(target) != ERROR_OK)
			return ERROR_FAIL;
	}

	if (reg->number >= GDB_REGNO_V0 && reg->number <= GDB_REGNO_V31) {
		if (!r->set_register_buf) {
			LOG_ERROR("Writing register %s not supported on this RISC-V target.",
					gdb_regno_name(reg->number));
			return ERROR_FAIL;
		}

		if (r->set_register_buf(target, reg->number, reg->value) != ERROR_OK)
			return ERROR_FAIL;
	} else {
		uint64_t value = buf_get_u64(buf, 0, reg->size);
		if (riscv_set_register(target, reg->number, value) != ERROR_OK)
			return ERROR_FAIL;
	}

	return ERROR_OK;
}

static struct reg_arch_type riscv_reg_arch_type = {
	.get = register_get,
	.set = register_set
};

static struct csr_info {
	unsigned number;
	const char *name;
};

static int cmp_csr_info(const void *p1, const void *p2)
{
	return (int) (((struct csr_info *)p1)->number) - (int) (((struct csr_info *)p2)->number);
}

 



