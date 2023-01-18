// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2007 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "arm.h"
#include "etm_dummy.h"

COMMAND_HANDLER(handle_etm_dummy_config_command)
{
	struct target *target;
	struct arm *arm;

	target = get_target(CMD_ARGV[0]);

	if (!target) {
		LOG_ERROR("target '%s' not defined", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	arm = target_to_arm(target);
	if (!is_arm(arm)) {
		command_print(CMD, "target '%s' isn't an ARM", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	if (arm->etm)
		arm->etm->capture_driver_priv = NULL;
	else {
		LOG_ERROR("target has no ETM defined, ETM dummy left unconfigured");
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static const struct command_registration etm_dummy_config_command_handlers[] = {
	{
		.name = "config",
		.handler = handle_etm_dummy_config_command,
		.mode = COMMAND_CONFIG,
		.usage = "target",
	},
	COMMAND_REGISTRATION_DONE
};
static const struct command_registration etm_dummy_command_handlers[] = {
	{
		.name = "etm_dummy",
		.mode = COMMAND_ANY,
		.help = "Dummy ETM capture driver command group",
		.chain = etm_dummy_config_command_handlers,
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE
};

static int etm_dummy_init(struct etm_context *etm_ctx)
{
	return ERROR_OK;
}

static trace_status_t etm_dummy_status(struct etm_context *etm_ctx)
{
	return TRACE_IDLE;
}

static int etm_dummy_read_trace(struct etm_context *etm_ctx)
{
	return ERROR_OK;
}

static int etm_dummy_start_capture(struct etm_context *etm_ctx)
{
	return ERROR_ETM_PORTMODE_NOT_SUPPORTED;
}

static int etm_dummy_stop_capture(struct etm_context *etm_ctx)
{
	return ERROR_OK;
}

struct etm_capture_driver etm_dummy_capture_driver = {
	.name = "dummy",
	.commands = etm_dummy_command_handlers,
	.init = etm_dummy_init,
	.status = etm_dummy_status,
	.start_capture = etm_dummy_start_capture,
	.stop_capture = etm_dummy_stop_capture,
	.read_trace = etm_dummy_read_trace,
};
