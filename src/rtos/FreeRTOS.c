/***************************************************************************
 *   Copyright (C) 2011 by Broadcom Corporation                            *
 *   Evan Hunter - ehunter@broadcom.com                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <helper/time_support.h>
#include <jtag/jtag.h>
#include "target/target.h"
#include "target/target_type.h"
#include "rtos.h"
#include "helper/log.h"
#include "helper/types.h"
#include "gnulib/gl_linkedhash_map.h"
#include "rtos_standard_stackings.h"
#include "target/armv7m.h"
#include "target/cortex_m.h"

#define FREERTOS_MAX_PRIORITIES	63
#define FREERTOS_THREAD_NAME_STR_SIZE 200

struct FreeRTOS_params {
	const char *target_name;
	int (*stacking)(struct rtos *rtos, const struct rtos_register_stacking **stacking,
					target_addr_t stack_ptr);
	const struct command_registration *commands;
};

struct FreeRTOS_thread_entry {
	threadid_t threadid;
	target_addr_t tcb;
};

struct FreeRTOS {
	const struct FreeRTOS_params *param;
	threadid_t last_threadid;
	/* Map from threadid to FreeRTOS_thread_entry. This map owns the value and
	 * is responsible for free()ing it. */
	gl_map_t entry_by_threadid;
	/* Map from tcb to FreeRTOS_thread_entry. */
	gl_map_t entry_by_tcb;
	/* sizeof(UBaseType_t) */
	unsigned ubasetype_size;
	/* sizeof(void *) */
	unsigned pointer_size;
	unsigned list_width;
	unsigned list_item_width;
	unsigned list_elem_next_offset;
	unsigned list_elem_next_size;
	unsigned list_elem_content_offset;
	unsigned list_elem_content_size;
	unsigned list_uxNumberOfItems_offset;
	unsigned list_uxNumberOfItems_size;
	unsigned list_next_offset;
	unsigned list_next_size;
	unsigned thread_stack_offset;
	unsigned thread_stack_size;
	unsigned thread_name_offset;
};

static int cortex_m_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	/* Check for armv7m with *enabled* FPU, i.e. a Cortex-M4F */
	int cm4_fpu_enabled = 0;
	struct armv7m_common *armv7m_target = target_to_armv7m(rtos->target);
	if (is_armv7m(armv7m_target)) {
		if (armv7m_target->fp_feature == FPv4_SP) {
			/* Found ARM v7m target which includes a FPU */
			uint32_t cpacr;

			int retval = target_read_u32(rtos->target, FPU_CPACR, &cpacr);
			if (retval != ERROR_OK) {
				LOG_ERROR("Could not read CPACR register to check FPU state");
				return retval;
			}

			/* Check if CP10 and CP11 are set to full access. */
			if (cpacr & 0x00F00000) {
				/* Found target with enabled FPU */
				cm4_fpu_enabled = 1;
			}
		}
	}

	if (cm4_fpu_enabled == 1) {
		/* Read the LR to decide between stacking with or without FPU */
		uint32_t LR_svc = 0;
		int retval = target_read_u32(rtos->target,
				stack_ptr + 0x20,
				&LR_svc);
		if (retval != ERROR_OK) {
			LOG_OUTPUT("Error reading stack frame from FreeRTOS thread");
			return retval;
		}
		if ((LR_svc & 0x10) == 0)
			*stacking = &rtos_standard_Cortex_M4F_FPU_stacking;
		else
			*stacking = &rtos_standard_Cortex_M4F_stacking;
	} else {
		*stacking = &rtos_standard_Cortex_M3_stacking;
	}

	return ERROR_OK;
}

static int nds32_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	*stacking = &rtos_standard_NDS32_N1068_stacking;
	return ERROR_OK;
}

static enum {
	STACKING_MAINLINE,
	STACKING_METAL
} riscv_freertos_stacking;
COMMAND_HANDLER(handle_riscv_freertos_stacking)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	if (!strcmp(CMD_ARGV[0], "mainline")) {
		riscv_freertos_stacking = STACKING_MAINLINE;
	} else if (!strcmp(CMD_ARGV[0], "metal")) {
		riscv_freertos_stacking = STACKING_METAL;
	} else {
		LOG_ERROR("Only two arguments are supported: mainline and metal");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	return ERROR_OK;
}

static const struct command_registration riscv_commands[] = {
	{
		.name = "riscv_freertos_stacking",
		.handler = handle_riscv_freertos_stacking,
		.mode = COMMAND_ANY,
		.usage = "mainline|metal",
		.help = "Select which FreeRTOS branch is being used. OpenOCD needs to "
		"know because different branches save thread registers on the stack "
		"in different orders. It is likely that this order on both branches will "
		"change in the future, so make sure to seek out the very latest OpenOCD if "
		"debugging is not working right."
	},
	COMMAND_REGISTRATION_DONE
};

static int riscv_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;
	LOG_DEBUG("riscv_freertos_stacking=%d", riscv_freertos_stacking);
	switch (riscv_freertos_stacking) {
		case STACKING_MAINLINE:
			if (freertos->pointer_size == 4)
				*stacking = &rtos_standard_RV32_stacking;
			else if (freertos->pointer_size == 8)
				*stacking = &rtos_standard_RV64_stacking;
			break;
		case STACKING_METAL:
			if (freertos->pointer_size == 4)
				*stacking = &rtos_metal_RV32_stacking;
			else if (freertos->pointer_size == 8)
				*stacking = &rtos_metal_RV64_stacking;
			break;
	}
	return ERROR_OK;
}

static const struct FreeRTOS_params FreeRTOS_params_list[] = {
	{
	.target_name = "cortex_m",
	.stacking = cortex_m_stacking
	},
	{
	.target_name = "hla_target",
	.stacking = cortex_m_stacking
	},
	{
	.target_name = "nds32_v3",
	.stacking = nds32_stacking,
	},
	{
	.target_name = "riscv",
	.stacking = riscv_stacking,
	.commands = riscv_commands,
	},
};

#define FREERTOS_NUM_PARAMS ((int)(sizeof(FreeRTOS_params_list)/sizeof(struct FreeRTOS_params)))

static bool FreeRTOS_detect_rtos(struct target *target);
static int FreeRTOS_create(struct target *target);
static int FreeRTOS_update_threads(struct rtos *rtos);
static int FreeRTOS_get_thread_reg_list(struct rtos *rtos, threadid_t thread_id,
		struct rtos_reg **reg_list, int *num_regs);
static int FreeRTOS_get_thread_reg(struct rtos *rtos, threadid_t thread_id,
		uint32_t reg_num, struct rtos_reg *reg);
static int FreeRTOS_set_reg(struct rtos *rtos, uint32_t reg_num, uint8_t *reg_value);
static int FreeRTOS_get_symbol_list_to_lookup(symbol_table_elem_t *symbol_list[]);

struct rtos_type FreeRTOS_rtos = {
	.name = "FreeRTOS",

	.detect_rtos = FreeRTOS_detect_rtos,
	.create = FreeRTOS_create,
	.update_threads = FreeRTOS_update_threads,
	.get_thread_reg_list = FreeRTOS_get_thread_reg_list,
	.get_thread_reg = FreeRTOS_get_thread_reg,
	.set_reg = FreeRTOS_set_reg,
	.get_symbol_list_to_lookup = FreeRTOS_get_symbol_list_to_lookup,
};

enum FreeRTOS_symbol_values {
	FreeRTOS_VAL_pxCurrentTCB = 0,
	FreeRTOS_VAL_pxReadyTasksLists = 1,
	FreeRTOS_VAL_xDelayedTaskList1 = 2,
	FreeRTOS_VAL_xDelayedTaskList2 = 3,
	FreeRTOS_VAL_pxDelayedTaskList = 4,
	FreeRTOS_VAL_pxOverflowDelayedTaskList = 5,
	FreeRTOS_VAL_xPendingReadyList = 6,
	FreeRTOS_VAL_xTasksWaitingTermination = 7,
	FreeRTOS_VAL_xSuspendedTaskList = 8,
	FreeRTOS_VAL_uxCurrentNumberOfTasks = 9,
	FreeRTOS_VAL_uxTopUsedPriority = 10,
};

struct symbols {
	const char *name;
	bool optional;
};

static const struct symbols FreeRTOS_symbol_list[] = {
	{ "pxCurrentTCB", false },
	{ "pxReadyTasksLists", false },
	{ "xDelayedTaskList1", false },
	{ "xDelayedTaskList2", false },
	{ "pxDelayedTaskList", false },
	{ "pxOverflowDelayedTaskList", false },
	{ "xPendingReadyList", false },
	{ "xTasksWaitingTermination", true }, /* Only if INCLUDE_vTaskDelete */
	{ "xSuspendedTaskList", true }, /* Only if INCLUDE_vTaskSuspend */
	{ "uxCurrentNumberOfTasks", false },
	{ "uxTopUsedPriority", true }, /* Unavailable since v7.5.3 */
	{ NULL, false }
};

/* TODO: */
/* this is not safe for little endian yet */
/* may be problems reading if sizes are not 32 bit long integers. */
/* test mallocs for failure */

static int FreeRTOS_read_struct_value(
	struct target *target, target_addr_t base_address, unsigned offset,
	unsigned size_bytes, uint64_t *value)
{
	uint8_t buf[size_bytes];
	int retval = target_read_buffer(target, base_address + offset, size_bytes, buf);
	*value = buf_get_u64(buf, 0, size_bytes * 8);
	return retval;
}

typedef struct {
	enum {
		TYPE_POINTER,
		TYPE_UBASE,
		TYPE_TICKTYPE,
		TYPE_LIST_ITEM,
		TYPE_CHAR_ARRAY
	} type;
	unsigned offset;
	unsigned size;
} type_offset_size_t;

static unsigned populate_offset_size(struct FreeRTOS *freertos,
									 type_offset_size_t *info, unsigned count)
{
	unsigned offset = 0;
	unsigned largest = 0;
	for (unsigned i = 0; i < count; i++) {
		unsigned align = 0;
		switch (info[i].type) {
			case TYPE_UBASE:
				info[i].size = freertos->ubasetype_size;
				align = freertos->ubasetype_size;
				break;
			case TYPE_POINTER:
				info[i].size = freertos->pointer_size;
				align = freertos->pointer_size;
				break;
			case TYPE_TICKTYPE:
				/* Could be either 16 or 32 bits, depending on configUSE_16_BIT_TICKS. */
				info[i].size = 4;
				align = 4;
				break;
			case TYPE_LIST_ITEM:
				info[i].size = freertos->list_item_width;
				align = MAX(freertos->ubasetype_size, freertos->pointer_size);
				break;
			case TYPE_CHAR_ARRAY:
				/* size is set by the caller. */
				align = 1;
				break;
		}

		assert(info[i].size > 0);
		assert(align > 0);

		largest = MAX(largest, align);

		if (offset & (align - 1)) {
			offset = offset & ~(align - 1);
			offset += align;
		}

		info[i].offset = offset;
		offset += info[i].size;
	}

	/* Now align offset to the largest type used, and return that as the width
	 * of the structure. */

	if (offset & (largest - 1)) {
		offset = offset & ~(largest - 1);
		offset += largest;
	}
	return offset;
}

static int FreeRTOS_update_threads(struct rtos *rtos)
{
	int retval;
	unsigned int tasks_found = 0;

	if (rtos->rtos_specific_params == NULL)
		return ERROR_FAIL;

	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;

	if (rtos->symbols == NULL) {
		LOG_ERROR("No symbols for FreeRTOS");
		return ERROR_FAIL;
	}

	if (rtos->symbols[FreeRTOS_VAL_uxCurrentNumberOfTasks].address == 0) {
		LOG_ERROR("Don't have the number of threads in FreeRTOS");
		return ERROR_FAIL;
	}

	uint64_t thread_list_size;
	retval = FreeRTOS_read_struct_value(rtos->target,
										rtos->symbols[FreeRTOS_VAL_uxCurrentNumberOfTasks].address,
										0,
										freertos->ubasetype_size,
										&thread_list_size);
	LOG_DEBUG("FreeRTOS: Read uxCurrentNumberOfTasks at 0x%" PRIx64 ", value %" PRIu64,
										rtos->symbols[FreeRTOS_VAL_uxCurrentNumberOfTasks].address,
										thread_list_size);

	if (retval != ERROR_OK) {
		LOG_ERROR("Could not read FreeRTOS thread count from target");
		return retval;
	}

	/* wipe out previous thread details if any */
	rtos_free_threadlist(rtos);

	/* read the current thread */
	target_addr_t pxCurrentTCB;
	retval = FreeRTOS_read_struct_value(rtos->target,
										rtos->symbols[FreeRTOS_VAL_pxCurrentTCB].address,
										0,
										freertos->pointer_size,
										&pxCurrentTCB);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading current thread in FreeRTOS thread list");
		return retval;
	}
	LOG_DEBUG("FreeRTOS: Read pxCurrentTCB at 0x%" PRIx64 ", value 0x%" PRIx64,
										rtos->symbols[FreeRTOS_VAL_pxCurrentTCB].address,
										pxCurrentTCB);

	if ((thread_list_size == 0) || (pxCurrentTCB == 0)) {
		/* Either : No RTOS threads - there is always at least the current execution though */
		/* OR     : No current thread - all threads suspended - show the current execution
		 * of idling */
		char tmp_str[] = "Current Execution";
		thread_list_size++;
		tasks_found++;
		rtos->thread_details = malloc(
				sizeof(struct thread_detail) * thread_list_size);
		if (!rtos->thread_details) {
			LOG_ERROR("Error allocating memory for %" PRIu64 " threads", thread_list_size);
			return ERROR_FAIL;
		}
		rtos->thread_details->threadid = 1;
		rtos->thread_details->exists = true;
		rtos->thread_details->extra_info_str = NULL;
		rtos->thread_details->thread_name_str = malloc(sizeof(tmp_str));
		strcpy(rtos->thread_details->thread_name_str, tmp_str);

		if (thread_list_size == 1) {
			rtos->thread_count = 1;
			return ERROR_OK;
		}
	} else {
		/* create space for new thread details */
		rtos->thread_details = malloc(
				sizeof(struct thread_detail) * thread_list_size);
		if (!rtos->thread_details) {
			LOG_ERROR("Error allocating memory for %" PRId64 " threads", thread_list_size);
			return ERROR_FAIL;
		}
	}

	/* Find out how many lists are needed to be read from pxReadyTasksLists, */
	uint64_t top_used_priority = 0;
	if (rtos->symbols[FreeRTOS_VAL_uxTopUsedPriority].address == 0) {
		LOG_WARNING("FreeRTOS: uxTopUsedPriority is not defined, consult the OpenOCD manual for a work-around");
		/* This is a hack specific to the binary I'm debugging.
		 * Ideally we get https://github.com/FreeRTOS/FreeRTOS-Kernel/issues/33
		 * into our FreeRTOS source. */
		top_used_priority = 6;
	} else {
		retval = FreeRTOS_read_struct_value(rtos->target,
											rtos->symbols[FreeRTOS_VAL_uxTopUsedPriority].address,
											0,
											freertos->ubasetype_size,
											&top_used_priority);
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("FreeRTOS: Read uxTopUsedPriority at 0x%" PRIx64 ", value %" PRIu64,
				  rtos->symbols[FreeRTOS_VAL_uxTopUsedPriority].address,
				  top_used_priority);
	}
	if (top_used_priority > FREERTOS_MAX_PRIORITIES) {
		LOG_ERROR("FreeRTOS top used priority is unreasonably big, not proceeding: %" PRIu64,
			top_used_priority);
		return ERROR_FAIL;
	}

	/* uxTopUsedPriority was defined as configMAX_PRIORITIES - 1
	 * in old FreeRTOS versions (before V7.5.3)
	 * Use contrib/rtos-helpers/FreeRTOS-openocd.c to get compatible symbol
	 * in newer FreeRTOS versions.
	 * Here we restore the original configMAX_PRIORITIES value */
	unsigned int config_max_priorities = top_used_priority + 1;

	symbol_address_t *list_of_lists =
		malloc(sizeof(symbol_address_t) * (config_max_priorities + 5));
	if (!list_of_lists) {
		LOG_ERROR("Error allocating memory for %u priorities", config_max_priorities);
		return ERROR_FAIL;
	}

	unsigned int num_lists;
	for (num_lists = 0; num_lists < config_max_priorities; num_lists++)
		list_of_lists[num_lists] = rtos->symbols[FreeRTOS_VAL_pxReadyTasksLists].address +
			num_lists * freertos->list_width;

	list_of_lists[num_lists++] = rtos->symbols[FreeRTOS_VAL_xDelayedTaskList1].address;
	list_of_lists[num_lists++] = rtos->symbols[FreeRTOS_VAL_xDelayedTaskList2].address;
	list_of_lists[num_lists++] = rtos->symbols[FreeRTOS_VAL_xPendingReadyList].address;
	list_of_lists[num_lists++] = rtos->symbols[FreeRTOS_VAL_xSuspendedTaskList].address;
	list_of_lists[num_lists++] = rtos->symbols[FreeRTOS_VAL_xTasksWaitingTermination].address;

	rtos->current_thread = 0;
	for (unsigned int i = 0; i < num_lists; i++) {
		if (list_of_lists[i] == 0)
			continue;

		/* Read the number of threads in this list */
		uint64_t list_thread_count = 0;
		retval = FreeRTOS_read_struct_value(rtos->target,
											list_of_lists[i],
											freertos->list_uxNumberOfItems_offset,
											freertos->list_uxNumberOfItems_size,
											&list_thread_count);
		if (retval != ERROR_OK) {
			LOG_ERROR("Error reading number of threads in FreeRTOS thread list");
			free(list_of_lists);
			return retval;
		}
		LOG_DEBUG("FreeRTOS: Read thread count for list %u at 0x%" PRIx64 ", value %" PRIu64,
										i, list_of_lists[i], list_thread_count);

		if (list_thread_count == 0)
			continue;

		/* Read the location of first list item */
		target_addr_t prev_list_elem_ptr = -1;
		target_addr_t list_elem_ptr = 0;
		retval = FreeRTOS_read_struct_value(rtos->target,
											list_of_lists[i],
											freertos->list_next_offset,
											freertos->list_next_size,
											&list_elem_ptr);
		if (retval != ERROR_OK) {
			LOG_ERROR("Error reading first thread item location in FreeRTOS thread list");
			free(list_of_lists);
			return retval;
		}
		LOG_DEBUG("FreeRTOS: Read first item for list %u at 0x%" PRIx64 ", value 0x%" PRIx64,
				  i, list_of_lists[i] + freertos->list_next_offset, list_elem_ptr);

		while ((list_thread_count > 0) && (list_elem_ptr != 0) &&
				(list_elem_ptr != prev_list_elem_ptr) &&
				(tasks_found < thread_list_size)) {
			/* Get the location of the thread structure. */
			rtos->thread_details[tasks_found].threadid = 0;
			target_addr_t tcb;
			retval = FreeRTOS_read_struct_value(rtos->target,
												list_elem_ptr,
												freertos->list_elem_content_offset,
												freertos->list_elem_content_size,
												&tcb);
			if (retval != ERROR_OK) {
				LOG_ERROR("Error reading thread list item object in FreeRTOS thread list");
				free(list_of_lists);
				return retval;
			}

			const struct FreeRTOS_thread_entry *value =
					gl_map_get(freertos->entry_by_tcb, &tcb);

			if (value == NULL) {
				struct FreeRTOS_thread_entry *new_value = calloc(1, sizeof(struct FreeRTOS_thread_entry));
				new_value->tcb = tcb;
				/* threadid can't be 0. */
				new_value->threadid = ++freertos->last_threadid;

				if (gl_map_nx_put(freertos->entry_by_tcb, &new_value->tcb, new_value) == -1) {
					LOG_ERROR("gl_map_nx_put failed");
					return ERROR_FAIL;
				}
				if (gl_map_nx_put(freertos->entry_by_threadid, &new_value->threadid, new_value) == -1) {
					LOG_ERROR("gl_map_nx_put failed");
					return ERROR_FAIL;
				}
				value = new_value;
			}

			rtos->thread_details[tasks_found].threadid = value->threadid;

			LOG_DEBUG("FreeRTOS: Thread %" PRId64 " has TCB 0x%" TARGET_PRIxADDR
					  "; read from 0x%" PRIx64,
					  value->threadid, value->tcb,
					  list_elem_ptr + freertos->list_elem_content_offset);

			/* get thread name */

			char tmp_str[FREERTOS_THREAD_NAME_STR_SIZE];

			/* Read the thread name */
			retval = target_read_buffer(rtos->target,
					value->tcb + freertos->thread_name_offset,
					FREERTOS_THREAD_NAME_STR_SIZE,
					(uint8_t *)&tmp_str);
			if (retval != ERROR_OK) {
				LOG_ERROR("Error reading first thread item location in FreeRTOS thread list");
				free(list_of_lists);
				return retval;
			}
			tmp_str[FREERTOS_THREAD_NAME_STR_SIZE-1] = '\x00';
			LOG_DEBUG("FreeRTOS: Read Thread Name at 0x%" PRIx64 ", value '%s'",
										value->tcb + freertos->thread_name_offset,
										tmp_str);

			if (tmp_str[0] == '\x00')
				strcpy(tmp_str, "No Name");

			rtos->thread_details[tasks_found].thread_name_str =
				malloc(strlen(tmp_str)+1);
			strcpy(rtos->thread_details[tasks_found].thread_name_str, tmp_str);
			rtos->thread_details[tasks_found].exists = true;

			if (value->tcb == pxCurrentTCB) {
				char running_str[] = "State: Running";
				rtos->current_thread = value->threadid;
				rtos->thread_details[tasks_found].extra_info_str = malloc(
						sizeof(running_str));
				strcpy(rtos->thread_details[tasks_found].extra_info_str,
					running_str);
			} else
				rtos->thread_details[tasks_found].extra_info_str = NULL;

			tasks_found++;
			list_thread_count--;

			prev_list_elem_ptr = list_elem_ptr;
			list_elem_ptr = 0;
			retval = FreeRTOS_read_struct_value(rtos->target,
												prev_list_elem_ptr,
												freertos->list_elem_next_offset,
												freertos->list_elem_next_size,
												&list_elem_ptr);
			if (retval != ERROR_OK) {
				LOG_ERROR("Error reading next thread item location in FreeRTOS thread list");
				free(list_of_lists);
				return retval;
			}
			LOG_DEBUG("FreeRTOS: Read next thread location at " TARGET_ADDR_FMT
					  ", value " TARGET_ADDR_FMT,
					  prev_list_elem_ptr + freertos->list_elem_next_offset,
					  list_elem_ptr);
		}
	}

	free(list_of_lists);
	rtos->thread_count = tasks_found;
	return 0;
}

static int FreeRTOS_get_stacking_info(struct rtos *rtos, threadid_t thread_id,
									  const struct rtos_register_stacking **stacking_info,
									  target_addr_t *stack_ptr)
{
	if (rtos->rtos_specific_params == NULL) {
		LOG_ERROR("rtos_specific_params is NULL!");
		return ERROR_FAIL;
	}

	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;
	const struct FreeRTOS_params *param = freertos->param;

	const struct FreeRTOS_thread_entry *entry =
			gl_map_get(freertos->entry_by_threadid, &thread_id);
	if (entry == NULL) {
		LOG_ERROR("Unknown thread id: %" PRId64, thread_id);
		return ERROR_FAIL;
	}

	/* Read the stack pointer */
	int retval = FreeRTOS_read_struct_value(rtos->target,
											entry->tcb,
											freertos->thread_stack_offset,
											freertos->thread_stack_size,
											stack_ptr);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading stack frame from FreeRTOS thread %" PRIx64, thread_id);
		return retval;
	}
	LOG_DEBUG("[%" PRId64 "] FreeRTOS: Read stack pointer at 0x%" PRIx64 ", value 0x%" PRIx64,
			  thread_id, entry->tcb + freertos->thread_stack_offset, *stack_ptr);

	if (param->stacking(rtos, stacking_info, *stack_ptr) != ERROR_OK) {
		LOG_ERROR("No stacking info found for %s!", param->target_name);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int FreeRTOS_get_thread_reg_list(struct rtos *rtos, threadid_t thread_id,
		struct rtos_reg **reg_list, int *num_regs)
{
	/* Let the caller read registers directly for the current thread. */
	if (thread_id == 0)
		return ERROR_FAIL;

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (FreeRTOS_get_stacking_info(rtos, thread_id, &stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	return rtos_generic_stack_read(rtos->target, stacking_info, stack_ptr, reg_list, num_regs);
}

static int FreeRTOS_get_thread_reg(struct rtos *rtos, threadid_t thread_id,
		uint32_t reg_num, struct rtos_reg *reg)
{
	LOG_DEBUG("reg_num=%d", reg_num);
	/* Let the caller read registers directly for the current thread. */
	if (thread_id == 0)
		return ERROR_FAIL;

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (FreeRTOS_get_stacking_info(rtos, thread_id, &stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	return rtos_generic_stack_read_reg(rtos->target, stacking_info, stack_ptr, reg_num, reg);
}

static int FreeRTOS_set_reg(struct rtos *rtos, uint32_t reg_num, uint8_t *reg_value)
{
	LOG_DEBUG("[%" PRId64 "] reg_num=%" PRId32, rtos->current_threadid, reg_num);

	/* Let the caller write registers directly for the current thread. */
	if (rtos->current_threadid == rtos->current_thread)
		return ERROR_FAIL;

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (FreeRTOS_get_stacking_info(rtos, rtos->current_threadid,
								   &stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	return rtos_generic_stack_write_reg(rtos->target, stacking_info, stack_ptr,
										reg_num, reg_value);
}

static int FreeRTOS_get_symbol_list_to_lookup(symbol_table_elem_t *symbol_list[])
{
	unsigned int i;
	*symbol_list = calloc(
			ARRAY_SIZE(FreeRTOS_symbol_list), sizeof(symbol_table_elem_t));

	for (i = 0; i < ARRAY_SIZE(FreeRTOS_symbol_list); i++) {
		(*symbol_list)[i].symbol_name = FreeRTOS_symbol_list[i].name;
		(*symbol_list)[i].optional = FreeRTOS_symbol_list[i].optional;
	}

	return 0;
}

#if 0

static int FreeRTOS_set_current_thread(struct rtos *rtos, threadid_t thread_id)
{
	return 0;
}

static int FreeRTOS_get_thread_ascii_info(struct rtos *rtos, threadid_t thread_id, char **info)
{
	int retval;
	const struct FreeRTOS_params *param;

	if (rtos == NULL)
		return -1;

	if (thread_id == 0)
		return -2;

	if (rtos->rtos_specific_params == NULL)
		return -3;

	param = (const struct FreeRTOS_params *) rtos->rtos_specific_params;

#define FREERTOS_THREAD_NAME_STR_SIZE (200)
	char tmp_str[FREERTOS_THREAD_NAME_STR_SIZE];

	/* Read the thread name */
	retval = target_read_buffer(rtos->target,
			thread_id + param->thread_name_offset,
			FREERTOS_THREAD_NAME_STR_SIZE,
			(uint8_t *)&tmp_str);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading first thread item location in FreeRTOS thread list");
		return retval;
	}
	tmp_str[FREERTOS_THREAD_NAME_STR_SIZE-1] = '\x00';

	if (tmp_str[0] == '\x00')
		strcpy(tmp_str, "No Name");

	*info = malloc(strlen(tmp_str)+1);
	strcpy(*info, tmp_str);
	return 0;
}

#endif

static bool FreeRTOS_detect_rtos(struct target *target)
{
	if ((target->rtos->symbols != NULL) &&
			(target->rtos->symbols[FreeRTOS_VAL_pxReadyTasksLists].address != 0)) {
		/* looks like FreeRTOS */
		return true;
	}
	return false;
}

static bool target_addr_equals(const void *x1, const void *x2)
{
	const target_addr_t *a1 = x1;
	const target_addr_t *a2 = x2;
	return *a1 == *a2;
}

static size_t target_addr_hash(const void *x)
{
	const target_addr_t *a = x;
	return *a;
}

static void free_const(const void *x)
{
	/* Cast away const. */
	free((void *) x);
}

static int FreeRTOS_create(struct target *target)
{
	int i = 0;
	while ((i < FREERTOS_NUM_PARAMS) &&
			(0 != strcmp(FreeRTOS_params_list[i].target_name, target->type->name))) {
		i++;
	}
	if (i >= FREERTOS_NUM_PARAMS) {
		LOG_ERROR("Could not find target in FreeRTOS compatibility list");
		return ERROR_FAIL;
	}

	target->rtos->rtos_specific_params = calloc(1, sizeof(struct FreeRTOS));
	if (target->rtos->rtos_specific_params == NULL) {
		LOG_ERROR("calloc failed");
		return ERROR_FAIL;
	}

	struct FreeRTOS *freertos = (struct FreeRTOS *) target->rtos->rtos_specific_params;
	freertos->entry_by_threadid = gl_map_nx_create_empty(
		GL_LINKEDHASH_MAP, target_addr_equals, target_addr_hash, NULL, free_const);
	if (freertos->entry_by_threadid == NULL) {
		LOG_ERROR("gl_map_nx_create_empty failed");
		return ERROR_FAIL;
	}
	freertos->entry_by_tcb = gl_map_nx_create_empty(
		GL_LINKEDHASH_MAP, target_addr_equals, target_addr_hash, NULL, NULL);
	if (freertos->entry_by_tcb == NULL) {
		LOG_ERROR("gl_map_nx_create_empty failed");
		return ERROR_FAIL;
	}
	freertos->param = &FreeRTOS_params_list[i];

	if (freertos->param->commands) {
		if (register_commands(target->rtos->cmd_ctx, NULL,
							  freertos->param->commands) != ERROR_OK)
			return ERROR_FAIL;
	}

	freertos->pointer_size = DIV_ROUND_UP(target_address_bits(target), 8);
	freertos->ubasetype_size = DIV_ROUND_UP(target_data_bits(target), 8);

	/*
	 * FreeRTOS can be compiled with configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES
	 * in which case extra data is inserted and OpenOCD won't work right.
	 */

	/* struct xLIST */
	type_offset_size_t struct_list_info[] = {
		{TYPE_UBASE, 0, 0},		/* uxNumberOfItems */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxIndex */
		{TYPE_TICKTYPE, 0, 0},	/* xItemValue */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxNext */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxPrevious */
	};

	/* struct xLIST_ITEM */
	type_offset_size_t struct_list_item_info[] = {
		{TYPE_TICKTYPE, 0, 0},	/* xItemValue */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxNext */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxPrevious */
		{TYPE_POINTER, 0, 0},	/* void *pvOwner */
		{TYPE_POINTER, 0, 0},	/* List_t *pvContainer */
	};

	/* struct tskTaskControlBlock */
	type_offset_size_t task_control_block_info[] = {
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxTopOfStack */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xStateListItem */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xEventListItem */
		{TYPE_UBASE, 0, 0},			/* uxPriority */
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxStack */
		/* configMAX_TASK_NAME_LEN varies a lot between targets, but luckily the
		 * name is NULL_terminated and we don't need to read anything else in
		 * the TCB. */
		{TYPE_CHAR_ARRAY, 0, FREERTOS_THREAD_NAME_STR_SIZE},	/* char pcTaskName[configMAX_TASK_NAME_LEN] */
		/* Lots of more optional stuff, but is is irrelevant to us. */
	};

	freertos->list_width = populate_offset_size(
		freertos, struct_list_info, ARRAY_SIZE(struct_list_info));
	freertos->list_uxNumberOfItems_offset = struct_list_info[0].offset;
	freertos->list_uxNumberOfItems_size = struct_list_info[0].size;
	freertos->list_next_offset = struct_list_info[3].offset;
	freertos->list_next_size = struct_list_info[3].size;

	freertos->list_item_width = populate_offset_size(
		freertos, struct_list_item_info, ARRAY_SIZE(struct_list_item_info));
	freertos->list_elem_next_offset = struct_list_item_info[1].offset;
	freertos->list_elem_next_size = struct_list_item_info[1].size;
	freertos->list_elem_content_offset = struct_list_item_info[3].offset;
	freertos->list_elem_content_size = struct_list_item_info[3].size;

	populate_offset_size(
		freertos, task_control_block_info, ARRAY_SIZE(task_control_block_info));
	freertos->thread_stack_offset = task_control_block_info[0].offset;
	freertos->thread_stack_size = task_control_block_info[0].size;
	freertos->thread_name_offset = task_control_block_info[5].offset;

	return 0;
}
