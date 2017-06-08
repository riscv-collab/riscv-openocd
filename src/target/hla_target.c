/***************************************************************************
 *   Copyright (C) 2011 by Mathias Kuester                                 *
 *   Mathias Kuester <kesmtp@freenet.de>                                   *
 *                                                                         *
 *   Copyright (C) 2011 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   revised:  4/25/13 by brent@mbari.org [DCC target request support]	   *
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

#include "jtag/jtag.h"
#include "jtag/hla/hla_transport.h"
#include "jtag/hla/hla_interface.h"
#include "jtag/hla/hla_layout.h"
#include "register.h"
#include "algorithm.h"
#include "target.h"
#include "breakpoints.h"
#include "target_type.h"
#include "armv7m.h"
#include "cortex_m.h"
#include "arm_semihosting.h"
#include "target_request.h"

#define savedDCRDR  dbgbase  /* FIXME: using target->dbgbase to preserve DCRDR */

#define ARMV7M_SCS_DCRSR	DCB_DCRSR
#define ARMV7M_SCS_DCRDR	DCB_DCRDR

static inline struct hl_interface_s *target_to_adapter(struct target *target)
{
	return target->tap->priv;
}

static int adapter_load_core_reg_u32(struct target *target,
		uint32_t num, uint32_t *value)
{
	int retval;
	struct hl_interface_s *adapter = target_to_adapter(target);

	LOG_DEBUG("%s", __func__);

	/* NOTE:  we "know" here that the register identifiers used
	 * in the v7m header match the Cortex-M3 Debug Core Register
	 * Selector values for R0..R15, xPSR, MSP, and PSP.
	 */
	switch (num) {
	case 0 ... 18:
		/* read a normal core register */
		retval = adapter->layout->api->read_reg(adapter->handle, num, value);

		if (retval != ERROR_OK) {
			LOG_ERROR("JTAG failure %i", retval);
			return ERROR_JTAG_DEVICE_ERROR;
		}
		LOG_DEBUG("load from core reg %i  value 0x%" PRIx32 "", (int)num, *value);
		break;

	case ARMV7M_FPSCR:
		/* Floating-point Status and Registers */
		retval = target_write_u32(target, ARMV7M_SCS_DCRSR, 33);
		if (retval != ERROR_OK)
			return retval;
		retval = target_read_u32(target, ARMV7M_SCS_DCRDR, value);
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("load from FPSCR  value 0x%" PRIx32, *value);
		break;

	case ARMV7M_S0 ... ARMV7M_S31:
		/* Floating-point Status and Registers */
		retval = target_write_u32(target, ARMV7M_SCS_DCRSR, num-ARMV7M_S0+64);
		if (retval != ERROR_OK)
			return retval;
		retval = target_read_u32(target, ARMV7M_SCS_DCRDR, value);
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("load from FPU reg S%d  value 0x%" PRIx32,
			  (int)(num - ARMV7M_S0), *value);
		break;

	case ARMV7M_PRIMASK:
	case ARMV7M_BASEPRI:
	case ARMV7M_FAULTMASK:
	case ARMV7M_CONTROL:
		/* Cortex-M3 packages these four registers as bitfields
		 * in one Debug Core register.  So say r0 and r2 docs;
		 * it was removed from r1 docs, but still works.
		 */
		retval = adapter->layout->api->read_reg(adapter->handle, 20, value);
		if (retval != ERROR_OK)
			return retval;

		switch (num) {
		case ARMV7M_PRIMASK:
			*value = buf_get_u32((uint8_t *) value, 0, 1);
			break;

		case ARMV7M_BASEPRI:
			*value = buf_get_u32((uint8_t *) value, 8, 8);
			break;

		case ARMV7M_FAULTMASK:
			*value = buf_get_u32((uint8_t *) value, 16, 1);
			break;

		case ARMV7M_CONTROL:
			*value = buf_get_u32((uint8_t *) value, 24, 2);
			break;
		}

		LOG_DEBUG("load from special reg %i value 0x%" PRIx32 "",
			  (int)num, *value);
		break;

	default:
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	return ERROR_OK;
}

static int adapter_store_core_reg_u32(struct target *target,
		uint32_t num, uint32_t value)
{
	int retval;
	uint32_t reg;
	struct armv7m_common *armv7m = target_to_armv7m(target);
	struct hl_interface_s *adapter = target_to_adapter(target);

	LOG_DEBUG("%s", __func__);

	/* NOTE:  we "know" here that the register identifiers used
	 * in the v7m header match the Cortex-M3 Debug Core Register
	 * Selector values for R0..R15, xPSR, MSP, and PSP.
	 */
	switch (num) {
	case 0 ... 18:
		retval = adapter->layout->api->write_reg(adapter->handle, num, value);

		if (retval != ERROR_OK) {
			struct reg *r;

			LOG_ERROR("JTAG failure");
			r = armv7m->arm.core_cache->reg_list + num;
			r->dirty = r->valid;
			return ERROR_JTAG_DEVICE_ERROR;
		}
		LOG_DEBUG("write core reg %i value 0x%" PRIx32 "", (int)num, value);
		break;

	case ARMV7M_FPSCR:
		/* Floating-point Status and Registers */
		retval = target_write_u32(target, ARMV7M_SCS_DCRDR, value);
		if (retval != ERROR_OK)
			return retval;
		retval = target_write_u32(target, ARMV7M_SCS_DCRSR, 33 | (1<<16));
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("write FPSCR value 0x%" PRIx32, value);
		break;

	case ARMV7M_S0 ... ARMV7M_S31:
		/* Floating-point Status and Registers */
		retval = target_write_u32(target, ARMV7M_SCS_DCRDR, value);
		if (retval != ERROR_OK)
			return retval;
		retval = target_write_u32(target, ARMV7M_SCS_DCRSR, (num-ARMV7M_S0+64) | (1<<16));
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("write FPU reg S%d  value 0x%" PRIx32,
			  (int)(num - ARMV7M_S0), value);
		break;

	case ARMV7M_PRIMASK:
	case ARMV7M_BASEPRI:
	case ARMV7M_FAULTMASK:
	case ARMV7M_CONTROL:
		/* Cortex-M3 packages these four registers as bitfields
		 * in one Debug Core register.  So say r0 and r2 docs;
		 * it was removed from r1 docs, but still works.
		 */

		adapter->layout->api->read_reg(adapter->handle, 20, &reg);

		switch (num) {
		case ARMV7M_PRIMASK:
			buf_set_u32((uint8_t *) &reg, 0, 1, value);
			break;

		case ARMV7M_BASEPRI:
			buf_set_u32((uint8_t *) &reg, 8, 8, value);
			break;

		case ARMV7M_FAULTMASK:
			buf_set_u32((uint8_t *) &reg, 16, 1, value);
			break;

		case ARMV7M_CONTROL:
			buf_set_u32((uint8_t *) &reg, 24, 2, value);
			break;
		}

		adapter->layout->api->write_reg(adapter->handle, 20, reg);

		LOG_DEBUG("write special reg %i value 0x%" PRIx32 " ", (int)num, value);
		break;

	default:
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	return ERROR_OK;
}

static int adapter_examine_debug_reason(struct target *target)
{
	if ((target->debug_reason != DBG_REASON_DBGRQ)
			&& (target->debug_reason != DBG_REASON_SINGLESTEP)) {
		target->debug_reason = DBG_REASON_BREAKPOINT;
	}

	return ERROR_OK;
}

static int hl_dcc_read(struct hl_interface_s *hl_if, uint8_t *value, uint8_t *ctrl)
{
	uint16_t dcrdr;
	int retval = hl_if->layout->api->read_mem(hl_if->handle,
			DCB_DCRDR, 1, sizeof(dcrdr), (uint8_t *)&dcrdr);
	if (retval == ERROR_OK) {
	    *ctrl = (uint8_t)dcrdr;
	    *value = (uint8_t)(dcrdr >> 8);

	    LOG_DEBUG("data 0x%x ctrl 0x%x", *value, *ctrl);

	    if (dcrdr & 1) {
			/* write ack back to software dcc register
			 * to signify we have read data */
			/* atomically clear just the byte containing the busy bit */
			static const uint8_t zero;
			retval = hl_if->layout->api->write_mem(hl_if->handle, DCB_DCRDR, 1, 1, &zero);
		}
	}
	return retval;
}

static int hl_target_request_data(struct target *target,
	uint32_t size, uint8_t *buffer)
{
	struct hl_interface_s *hl_if = target_to_adapter(target);
	uint8_t data;
	uint8_t ctrl;
	uint32_t i;

	for (i = 0; i < (size * 4); i++) {
		hl_dcc_read(hl_if, &data, &ctrl);
		buffer[i] = data;
	}

	return ERROR_OK;
}

static int hl_handle_target_request(void *priv)
{
	struct target *target = priv;
	if (!target_was_examined(target))
		return ERROR_OK;
	struct hl_interface_s *hl_if = target_to_adapter(target);

	if (!target->dbg_msg_enabled)
		return ERROR_OK;

	if (target->state == TARGET_RUNNING) {
		uint8_t data;
		uint8_t ctrl;

		hl_dcc_read(hl_if, &data, &ctrl);

		/* check if we have data */
		if (ctrl & (1 << 0)) {
			uint32_t request;

			/* we assume target is quick enough */
			request = data;
			hl_dcc_read(hl_if, &data, &ctrl);
			request |= (data << 8);
			hl_dcc_read(hl_if, &data, &ctrl);
			request |= (data << 16);
			hl_dcc_read(hl_if, &data, &ctrl);
			request |= (data << 24);
			target_request(target, request);
		}
	}

	return ERROR_OK;
}

static int adapter_init_arch_info(struct target *target,
				       struct cortex_m_common *cortex_m,
				       struct jtag_tap *tap)
{
	struct armv7m_common *armv7m;

	LOG_DEBUG("%s", __func__);

	armv7m = &cortex_m->armv7m;
	armv7m_init_arch_info(target, armv7m);

	armv7m->load_core_reg_u32 = adapter_load_core_reg_u32;
	armv7m->store_core_reg_u32 = adapter_store_core_reg_u32;

	armv7m->examine_debug_reason = adapter_examine_debug_reason;
	armv7m->stlink = true;

	target_register_timer_callback(hl_handle_target_request, 1, 1, target);

	return ERROR_OK;
}

static int adapter_init_target(struct command_context *cmd_ctx,
				    struct target *target)
{
	LOG_DEBUG("%s", __func__);

	armv7m_build_reg_cache(target);
	arm_semihosting_init(target);
	return ERROR_OK;
}

static int adapter_target_create(struct target *target,
		Jim_Interp *interp)
{
	LOG_DEBUG("%s", __func__);

	struct cortex_m_common *cortex_m = calloc(1, sizeof(struct cortex_m_common));

	if (!cortex_m)
		return ERROR_COMMAND_SYNTAX_ERROR;

	adapter_init_arch_info(target, cortex_m, target->tap);

	return ERROR_OK;
}

static int adapter_load_context(struct target *target)
{
	struct armv7m_common *armv7m = target_to_armv7m(target);
	int num_regs = armv7m->arm.core_cache->num_regs;

	for (int i = 0; i < num_regs; i++) {

		struct reg *r = &armv7m->arm.core_cache->reg_list[i];
		if (!r->valid)
			armv7m->arm.read_core_reg(target, r, i, ARM_MODE_ANY);
	}

	return ERROR_OK;
}

static int adapter_debug_entry(struct target *target)
{
	struct hl_interface_s *adapter = target_to_adapter(target);
	struct armv7m_common *armv7m = target_to_armv7m(target);
	struct arm *arm = &armv7m->arm;
	struct reg *r;
	uint32_t xPSR;
	int retval;

	/* preserve the DCRDR across halts */
	retval = target_read_u32(target, DCB_DCRDR, &target->savedDCRDR);
	if (retval != ERROR_OK)
		return retval;

	retval = armv7m->examine_debug_reason(target);
	if (retval != ERROR_OK)
		return retval;

	adapter_load_context(target);

	/* make sure we clear the vector catch bit */
	adapter->layout->api->write_debug_reg(adapter->handle, DCB_DEMCR, TRCENA);

	r = arm->cpsr;
	xPSR = buf_get_u32(r->value, 0, 32);

	/* Are we in an exception handler */
	if (xPSR & 0x1FF) {
		armv7m->exception_number = (xPSR & 0x1FF);

		arm->core_mode = ARM_MODE_HANDLER;
		arm->map = armv7m_msp_reg_map;
	} else {
		unsigned control = buf_get_u32(arm->core_cache
				->reg_list[ARMV7M_CONTROL].value, 0, 2);

		/* is this thread privileged? */
		arm->core_mode = control & 1
				? ARM_MODE_USER_THREAD
				: ARM_MODE_THREAD;

		/* which stack is it using? */
		if (control & 2)
			arm->map = armv7m_psp_reg_map;
		else
			arm->map = armv7m_msp_reg_map;

		armv7m->exception_number = 0;
	}

	LOG_DEBUG("entered debug state in core mode: %s at PC 0x%08" PRIx32 ", target->state: %s",
		arm_mode_name(arm->core_mode),
		buf_get_u32(arm->pc->value, 0, 32),
		target_state_name(target));

	return retval;
}

static int adapter_poll(struct target *target)
{
	enum target_state state;
	struct hl_interface_s *adapter = target_to_adapter(target);
	struct armv7m_common *armv7m = target_to_armv7m(target);
	enum target_state prev_target_state = target->state;

	state = adapter->layout->api->state(adapter->handle);

	if (state == TARGET_UNKNOWN) {
		LOG_ERROR("jtag status contains invalid mode value - communication failure");
		return ERROR_TARGET_FAILURE;
	}

	if (prev_target_state == state)
		return ERROR_OK;

	target->state = state;

	if (state == TARGET_HALTED) {

		int retval = adapter_debug_entry(target);
		if (retval != ERROR_OK)
			return retval;

		if (prev_target_state == TARGET_DEBUG_RUNNING) {
			target_call_event_callbacks(target, TARGET_EVENT_DEBUG_HALTED);
		} else {
			if (arm_semihosting(target, &retval) != 0)
				return retval;

			target_call_event_callbacks(target, TARGET_EVENT_HALTED);
		}

		LOG_DEBUG("halted: PC: 0x%08" PRIx32, buf_get_u32(armv7m->arm.pc->value, 0, 32));
	}

	return ERROR_OK;
}

static int adapter_assert_reset(struct target *target)
{
	int res = ERROR_OK;
	struct hl_interface_s *adapter = target_to_adapter(target);
	struct armv7m_common *armv7m = target_to_armv7m(target);
	bool use_srst_fallback = true;

	LOG_DEBUG("%s", __func__);

	enum reset_types jtag_reset_config = jtag_get_reset_config();

	bool srst_asserted = false;

	if ((jtag_reset_config & RESET_HAS_SRST) &&
	    (jtag_reset_config & RESET_SRST_NO_GATING)) {
		jtag_add_reset(0, 1);
		res = adapter->layout->api->assert_srst(adapter->handle, 0);
		srst_asserted = true;
	}

	adapter->layout->api->write_debug_reg(adapter->handle, DCB_DHCSR, DBGKEY|C_DEBUGEN);

	/* only set vector catch if halt is requested */
	if (target->reset_halt)
		adapter->layout->api->write_debug_reg(adapter->handle, DCB_DEMCR, TRCENA|VC_CORERESET);
	else
		adapter->layout->api->write_debug_reg(adapter->handle, DCB_DEMCR, TRCENA);

	if (jtag_reset_config & RESET_HAS_SRST) {
		if (!srst_asserted) {
			jtag_add_reset(0, 1);
			res = adapter->layout->api->assert_srst(adapter->handle, 0);
		}
		if (res == ERROR_COMMAND_NOTFOUND)
			LOG_ERROR("Hardware srst not supported, falling back to software reset");
		else if (res == ERROR_OK) {
			/* hardware srst supported */
			use_srst_fallback = false;
		}
	}

	if (use_srst_fallback) {
		/* stlink v1 api does not support hardware srst, so we use a software reset fallback */
		adapter->layout->api->write_debug_reg(adapter->handle, NVIC_AIRCR, AIRCR_VECTKEY | AIRCR_SYSRESETREQ);
	}

	res = adapter->layout->api->reset(adapter->handle);

	if (res != ERROR_OK)
		return res;

	/* registers are now invalid */
	register_cache_invalidate(armv7m->arm.core_cache);

	if (target->reset_halt) {
		target->state = TARGET_RESET;
		target->debug_reason = DBG_REASON_DBGRQ;
	} else {
		target->state = TARGET_HALTED;
	}

	return ERROR_OK;
}

static int adapter_deassert_reset(struct target *target)
{
	struct hl_interface_s *adapter = target_to_adapter(target);

	enum reset_types jtag_reset_config = jtag_get_reset_config();

	LOG_DEBUG("%s", __func__);

	if (jtag_reset_config & RESET_HAS_SRST)
		adapter->layout->api->assert_srst(adapter->handle, 1);

	/* virtual deassert reset, we need it for the internal
	 * jtag state machine
	 */
	jtag_add_reset(0, 0);

	target->savedDCRDR = 0;  /* clear both DCC busy bits on initial resume */

	return target->reset_halt ? ERROR_OK : target_resume(target, 1, 0, 0, 0);
}

static int adapter_halt(struct target *target)
{
	int res;
	struct hl_interface_s *adapter = target_to_adapter(target);

	LOG_DEBUG("%s", __func__);

	if (target->state == TARGET_HALTED) {
		LOG_DEBUG("target was already halted");
		return ERROR_OK;
	}

	if (target->state == TARGET_UNKNOWN)
		LOG_WARNING("target was in unknown state when halt was requested");

	res = adapter->layout->api->halt(adapter->handle);

	if (res != ERROR_OK)
		return res;

	target->debug_reason = DBG_REASON_DBGRQ;

	return ERROR_OK;
}

static int adapter_resume(struct target *target, int current,
		target_addr_t address, int handle_breakpoints,
		int debug_execution)
{
	int res;
	struct hl_interface_s *adapter = target_to_adapter(target);
	struct armv7m_common *armv7m = target_to_armv7m(target);
	uint32_t resume_pc;
	struct breakpoint *breakpoint = NULL;
	struct reg *pc;

	LOG_DEBUG("%s %d " TARGET_ADDR_FMT " %d %d", __func__, current,
			address, handle_breakpoints, debug_execution);

	if (target->state != TARGET_HALTED) {
		LOG_WARNING("target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (!debug_execution) {
		target_free_all_working_areas(target);
		cortex_m_enable_breakpoints(target);
		cortex_m_enable_watchpoints(target);
	}

	pc = armv7m->arm.pc;
	if (!current) {
		buf_set_u32(pc->value, 0, 32, address);
		pc->dirty = true;
		pc->valid = true;
	}

	if (!breakpoint_find(target, buf_get_u32(pc->value, 0, 32))
			&& !debug_execution) {
		armv7m_maybe_skip_bkpt_inst(target, NULL);
	}

	resume_pc = buf_get_u32(pc->value, 0, 32);

	/* write any user vector flags */
	res = target_write_u32(target, DCB_DEMCR, TRCENA | armv7m->demcr);
	if (res != ERROR_OK)
		return res;

	armv7m_restore_context(target);

	/* restore savedDCRDR */
	res = target_write_u32(target, DCB_DCRDR, target->savedDCRDR);
	if (res != ERROR_OK)
		return res;

	/* registers are now invalid */
	register_cache_invalidate(armv7m->arm.core_cache);

	/* the front-end may request us not to handle breakpoints */
	if (handle_breakpoints) {
		/* Single step past breakpoint at current address */
		breakpoint = breakpoint_find(target, resume_pc);
		if (breakpoint) {
			LOG_DEBUG("unset breakpoint at " TARGET_ADDR_FMT " (ID: %" PRIu32 ")",
					breakpoint->address,
					breakpoint->unique_id);
			cortex_m_unset_breakpoint(target, breakpoint);

			res = adapter->layout->api->step(adapter->handle);

			if (res != ERROR_OK)
				return res;

			cortex_m_set_breakpoint(target, breakpoint);
		}
	}

	res = adapter->layout->api->run(adapter->handle);

	if (res != ERROR_OK)
		return res;

	target->debug_reason = DBG_REASON_NOTHALTED;

	if (!debug_execution) {
		target->state = TARGET_RUNNING;
		target_call_event_callbacks(target, TARGET_EVENT_RESUMED);
	} else {
		target->state = TARGET_DEBUG_RUNNING;
		target_call_event_callbacks(target, TARGET_EVENT_DEBUG_RESUMED);
	}

	return ERROR_OK;
}

static int adapter_step(struct target *target, int current,
		target_addr_t address, int handle_breakpoints)
{
	int res;
	struct hl_interface_s *adapter = target_to_adapter(target);
	struct armv7m_common *armv7m = target_to_armv7m(target);
	struct breakpoint *breakpoint = NULL;
	struct reg *pc = armv7m->arm.pc;
	bool bkpt_inst_found = false;

	LOG_DEBUG("%s", __func__);

	if (target->state != TARGET_HALTED) {
		LOG_WARNING("target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (!current) {
		buf_set_u32(pc->value, 0, 32, address);
		pc->dirty = true;
		pc->valid = true;
	}

	uint32_t pc_value = buf_get_u32(pc->value, 0, 32);

	/* the front-end may request us not to handle breakpoints */
	if (handle_breakpoints) {
		breakpoint = breakpoint_find(target, pc_value);
		if (breakpoint)
			cortex_m_unset_breakpoint(target, breakpoint);
	}

	armv7m_maybe_skip_bkpt_inst(target, &bkpt_inst_found);

	target->debug_reason = DBG_REASON_SINGLESTEP;

	armv7m_restore_context(target);

	/* restore savedDCRDR */
	res = target_write_u32(target, DCB_DCRDR, target->savedDCRDR);
	if (res != ERROR_OK)
		return res;

	target_call_event_callbacks(target, TARGET_EVENT_RESUMED);

	res = adapter->layout->api->step(adapter->handle);

	if (res != ERROR_OK)
		return res;

	/* registers are now invalid */
	register_cache_invalidate(armv7m->arm.core_cache);

	if (breakpoint)
		cortex_m_set_breakpoint(target, breakpoint);

	adapter_debug_entry(target);
	target_call_event_callbacks(target, TARGET_EVENT_HALTED);

	LOG_INFO("halted: PC: 0x%08" PRIx32, buf_get_u32(armv7m->arm.pc->value, 0, 32));

	return ERROR_OK;
}

static int adapter_read_memory(struct target *target, target_addr_t address,
		uint32_t size, uint32_t count,
		uint8_t *buffer)
{
	struct hl_interface_s *adapter = target_to_adapter(target);

	if (!count || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

	LOG_DEBUG("%s " TARGET_ADDR_FMT " %" PRIu32 " %" PRIu32,
			  __func__, address, size, count);

	return adapter->layout->api->read_mem(adapter->handle, address, size, count, buffer);
}

static int adapter_write_memory(struct target *target, target_addr_t address,
		uint32_t size, uint32_t count,
		const uint8_t *buffer)
{
	struct hl_interface_s *adapter = target_to_adapter(target);

	if (!count || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

	LOG_DEBUG("%s " TARGET_ADDR_FMT " %" PRIu32 " %" PRIu32,
			  __func__, address, size, count);

	return adapter->layout->api->write_mem(adapter->handle, address, size, count, buffer);
}

static const struct command_registration adapter_command_handlers[] = {
	{
		.chain = arm_command_handlers,
	},
	{
		.chain = armv7m_trace_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

struct target_type hla_target = {
	.name = "hla_target",
	.deprecated_name = "stm32_stlink",

	.init_target = adapter_init_target,
	.deinit_target = cortex_m_deinit_target,
	.target_create = adapter_target_create,
	.examine = cortex_m_examine,
	.commands = adapter_command_handlers,

	.poll = adapter_poll,
	.arch_state = armv7m_arch_state,

	.target_request_data = hl_target_request_data,
	.assert_reset = adapter_assert_reset,
	.deassert_reset = adapter_deassert_reset,

	.halt = adapter_halt,
	.resume = adapter_resume,
	.step = adapter_step,

	.get_gdb_reg_list = armv7m_get_gdb_reg_list,

	.read_memory = adapter_read_memory,
	.write_memory = adapter_write_memory,
	.checksum_memory = armv7m_checksum_memory,
	.blank_check_memory = armv7m_blank_check_memory,

	.run_algorithm = armv7m_run_algorithm,
	.start_algorithm = armv7m_start_algorithm,
	.wait_algorithm = armv7m_wait_algorithm,

	.add_breakpoint = cortex_m_add_breakpoint,
	.remove_breakpoint = cortex_m_remove_breakpoint,
	.add_watchpoint = cortex_m_add_watchpoint,
	.remove_watchpoint = cortex_m_remove_watchpoint,
};
