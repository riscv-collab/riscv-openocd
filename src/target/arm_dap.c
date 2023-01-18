// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2016 by Matthias Welwarsky                              *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include "target/arm_adi_v5.h"
#include "target/arm.h"
#include "helper/list.h"
#include "helper/command.h"
#include "transport/transport.h"
#include "jtag/interface.h"

static LIST_HEAD(all_dap);

extern const struct dap_ops swd_dap_ops;
extern const struct dap_ops jtag_dp_ops;
extern struct adapter_driver *adapter_driver;

/* DAP command support */
struct arm_dap_object {
	struct list_head lh;
	struct adiv5_dap dap;
	char *name;
	const struct swd_driver *swd;
};

static void dap_instance_init(struct adiv5_dap *dap)
{
	int i;
	/* Set up with safe defaults */
	for (i = 0; i <= DP_APSEL_MAX; i++) {
		dap->ap[i].dap = dap;
		dap->ap[i].ap_num = i;
		/* memaccess_tck max is 255 */
		dap->ap[i].memaccess_tck = 255;
		/* Number of bits for tar autoincrement, impl. dep. at least 10 */
		dap->ap[i].tar_autoincr_block = (1<<10);
		/* default CSW value */
		dap->ap[i].csw_default = CSW_AHB_DEFAULT;
		dap->ap[i].cfg_reg = MEM_AP_REG_CFG_INVALID; /* mem_ap configuration reg (large physical addr, etc.) */
	}
	INIT_LIST_HEAD(&dap->cmd_journal);
	INIT_LIST_HEAD(&dap->cmd_pool);
}

const char *adiv5_dap_name(struct adiv5_dap *self)
{
	struct arm_dap_object *obj = container_of(self, struct arm_dap_object, dap);
	return obj->name;
}

const struct swd_driver *adiv5_dap_swd_driver(struct adiv5_dap *self)
{
	struct arm_dap_object *obj = container_of(self, struct arm_dap_object, dap);
	return obj->swd;
}

struct adiv5_dap *adiv5_get_dap(struct arm_dap_object *obj)
{
	return &obj->dap;
}
struct adiv5_dap *dap_instance_by_jim_obj(Jim_Interp *interp, Jim_Obj *o)
{
	struct arm_dap_object *obj = NULL;
	const char *name;
	bool found = false;

	name = Jim_GetString(o, NULL);

	list_for_each_entry(obj, &all_dap, lh) {
		if (!strcmp(name, obj->name)) {
			found = true;
			break;
		}
	}

	if (found)
		return &obj->dap;
	return NULL;
}

static int dap_init_all(void)
{
	struct arm_dap_object *obj;
	int retval;

	LOG_DEBUG("Initializing all DAPs ...");

	list_for_each_entry(obj, &all_dap, lh) {
		struct adiv5_dap *dap = &obj->dap;

		/* with hla, dap is just a dummy */
		if (transport_is_hla())
			continue;

		/* skip taps that are disabled */
		if (!dap->tap->enabled)
			continue;

		if (transport_is_swd()) {
			dap->ops = &swd_dap_ops;
			obj->swd = adapter_driver->swd_ops;
		} else if (transport_is_dapdirect_swd()) {
			dap->ops = adapter_driver->dap_swd_ops;
		} else if (transport_is_dapdirect_jtag()) {
			dap->ops = adapter_driver->dap_jtag_ops;
		} else
			dap->ops = &jtag_dp_ops;

		retval = dap->ops->connect(dap);
		if (retval != ERROR_OK)
			return retval;
	}

	return ERROR_OK;
}

int dap_cleanup_all(void)
{
	struct arm_dap_object *obj, *tmp;
	struct adiv5_dap *dap;

	list_for_each_entry_safe(obj, tmp, &all_dap, lh) {
		dap = &obj->dap;
		if (dap->ops && dap->ops->quit)
			dap->ops->quit(dap);

		free(obj->name);
		free(obj);
	}

	return ERROR_OK;
}

enum dap_cfg_param {
	CFG_CHAIN_POSITION,
	CFG_IGNORE_SYSPWRUPACK,
	CFG_DP_ID,
	CFG_INSTANCE_ID,
};

static const struct jim_nvp nvp_config_opts[] = {
	{ .name = "-chain-position",     .value = CFG_CHAIN_POSITION },
	{ .name = "-ignore-syspwrupack", .value = CFG_IGNORE_SYSPWRUPACK },
	{ .name = "-dp-id",              .value = CFG_DP_ID },
	{ .name = "-instance-id",        .value = CFG_INSTANCE_ID },
	{ .name = NULL, .value = -1 }
};

static int dap_configure(struct jim_getopt_info *goi, struct arm_dap_object *dap)
{
	struct jim_nvp *n;
	int e;

	/* parse config ... */
	while (goi->argc > 0) {
		Jim_SetEmptyResult(goi->interp);

		e = jim_getopt_nvp(goi, nvp_config_opts, &n);
		if (e != JIM_OK) {
			jim_getopt_nvp_unknown(goi, nvp_config_opts, 0);
			return e;
		}
		switch (n->value) {
		case CFG_CHAIN_POSITION: {
			Jim_Obj *o_t;
			e = jim_getopt_obj(goi, &o_t);
			if (e != JIM_OK)
				return e;

			struct jtag_tap *tap;
			tap = jtag_tap_by_jim_obj(goi->interp, o_t);
			if (!tap) {
				Jim_SetResultString(goi->interp, "-chain-position is invalid", -1);
				return JIM_ERR;
			}
			dap->dap.tap = tap;
			/* loop for more */
			break;
		}
		case CFG_IGNORE_SYSPWRUPACK:
			dap->dap.ignore_syspwrupack = true;
			break;
		case CFG_DP_ID: {
			jim_wide w;
			e = jim_getopt_wide(goi, &w);
			if (e != JIM_OK) {
				Jim_SetResultFormatted(goi->interp,
						"create %s: bad parameter %s",
						dap->name, n->name);
				return JIM_ERR;
			}
			if (w < 0 || w > DP_TARGETSEL_DPID_MASK) {
				Jim_SetResultFormatted(goi->interp,
						"create %s: %s out of range",
						dap->name, n->name);
				return JIM_ERR;
			}
			dap->dap.multidrop_targetsel =
				(dap->dap.multidrop_targetsel & DP_TARGETSEL_INSTANCEID_MASK)
				| (w & DP_TARGETSEL_DPID_MASK);
			dap->dap.multidrop_dp_id_valid = true;
			break;
		}
		case CFG_INSTANCE_ID: {
			jim_wide w;
			e = jim_getopt_wide(goi, &w);
			if (e != JIM_OK) {
				Jim_SetResultFormatted(goi->interp,
						"create %s: bad parameter %s",
						dap->name, n->name);
				return JIM_ERR;
			}
			if (w < 0 || w > 15) {
				Jim_SetResultFormatted(goi->interp,
						"create %s: %s out of range",
						dap->name, n->name);
				return JIM_ERR;
			}
			dap->dap.multidrop_targetsel =
				(dap->dap.multidrop_targetsel & DP_TARGETSEL_DPID_MASK)
				| ((w << DP_TARGETSEL_INSTANCEID_SHIFT) & DP_TARGETSEL_INSTANCEID_MASK);
			dap->dap.multidrop_instance_id_valid = true;
			break;
		}
		default:
			break;
		}
	}

	return JIM_OK;
}

static int dap_check_config(struct adiv5_dap *dap)
{
	if (transport_is_jtag() || transport_is_dapdirect_jtag() || transport_is_hla())
		return ERROR_OK;

	struct arm_dap_object *obj;
	bool new_multidrop = dap_is_multidrop(dap);
	bool had_multidrop = new_multidrop;
	uint32_t targetsel = dap->multidrop_targetsel;
	unsigned int non_multidrop_count = had_multidrop ? 0 : 1;

	list_for_each_entry(obj, &all_dap, lh) {
		struct adiv5_dap *dap_it = &obj->dap;

		if (transport_is_swd()) {
			if (dap_is_multidrop(dap_it)) {
				had_multidrop = true;
				if (new_multidrop && dap_it->multidrop_targetsel == targetsel) {
					uint32_t dp_id = targetsel & DP_TARGETSEL_DPID_MASK;
					uint32_t instance_id = targetsel >> DP_TARGETSEL_INSTANCEID_SHIFT;
					LOG_ERROR("%s and %s have the same multidrop selectors -dp-id 0x%08"
							  PRIx32 " and -instance-id 0x%" PRIx32,
							  obj->name, adiv5_dap_name(dap),
							  dp_id, instance_id);
					return ERROR_FAIL;
				}
			} else {
				non_multidrop_count++;
			}
		} else if (transport_is_dapdirect_swd()) {
			non_multidrop_count++;
		}
	}

	if (non_multidrop_count > 1) {
		LOG_ERROR("Two or more SWD non multidrop DAPs are not supported");
		return ERROR_FAIL;
	}
	if (had_multidrop && non_multidrop_count) {
		LOG_ERROR("Mixing of SWD multidrop DAPs and non multidrop DAPs is not supported");
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int dap_create(struct jim_getopt_info *goi)
{
	struct command_context *cmd_ctx;
	static struct arm_dap_object *dap;
	Jim_Obj *new_cmd;
	Jim_Cmd *cmd;
	const char *cp;
	int e;

	cmd_ctx = current_command_context(goi->interp);
	assert(cmd_ctx);

	if (goi->argc < 3) {
		Jim_WrongNumArgs(goi->interp, 1, goi->argv, "?name? ..options...");
		return JIM_ERR;
	}
	/* COMMAND */
	jim_getopt_obj(goi, &new_cmd);
	/* does this command exist? */
	cmd = Jim_GetCommand(goi->interp, new_cmd, JIM_NONE);
	if (cmd) {
		cp = Jim_GetString(new_cmd, NULL);
		Jim_SetResultFormatted(goi->interp, "Command: %s Exists", cp);
		return JIM_ERR;
	}

	/* Create it */
	dap = calloc(1, sizeof(struct arm_dap_object));
	if (!dap)
		return JIM_ERR;

	dap_instance_init(&dap->dap);

	cp = Jim_GetString(new_cmd, NULL);
	dap->name = strdup(cp);

	e = dap_configure(goi, dap);
	if (e != JIM_OK)
		goto err;

	if (!dap->dap.tap) {
		Jim_SetResultString(goi->interp, "-chain-position required when creating DAP", -1);
		e = JIM_ERR;
		goto err;
	}

	e = dap_check_config(&dap->dap);
	if (e != ERROR_OK) {
		e = JIM_ERR;
		goto err;
	}

	struct command_registration dap_create_commands[] = {
		{
			.name = cp,
			.mode = COMMAND_ANY,
			.help = "dap instance command group",
			.usage = "",
			.chain = dap_instance_commands,
		},
		COMMAND_REGISTRATION_DONE
	};

	/* don't expose the instance commands when using hla */
	if (transport_is_hla())
		dap_create_commands[0].chain = NULL;

	e = register_commands_with_data(cmd_ctx, NULL, dap_create_commands, dap);
	if (e != ERROR_OK) {
		e = JIM_ERR;
		goto err;
	}

	list_add_tail(&dap->lh, &all_dap);

	return JIM_OK;

err:
	free(dap->name);
	free(dap);
	return e;
}

static int jim_dap_create(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	struct jim_getopt_info goi;
	jim_getopt_setup(&goi, interp, argc - 1, argv + 1);
	if (goi.argc < 2) {
		Jim_WrongNumArgs(goi.interp, goi.argc, goi.argv,
			"<name> [<dap_options> ...]");
		return JIM_ERR;
	}
	return dap_create(&goi);
}

static int jim_dap_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	struct arm_dap_object *obj;

	if (argc != 1) {
		Jim_WrongNumArgs(interp, 1, argv, "Too many parameters");
		return JIM_ERR;
	}
	Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));
	list_for_each_entry(obj, &all_dap, lh) {
		Jim_ListAppendElement(interp, Jim_GetResult(interp),
			Jim_NewStringObj(interp, obj->name, -1));
	}
	return JIM_OK;
}

COMMAND_HANDLER(handle_dap_init)
{
	return dap_init_all();
}

COMMAND_HANDLER(handle_dap_info_command)
{
	struct target *target = get_current_target(CMD_CTX);
	struct arm *arm = target_to_arm(target);
	struct adiv5_dap *dap = arm->dap;
	uint32_t apsel;

	if (!dap) {
		LOG_ERROR("DAP instance not available. Probably a HLA target...");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	switch (CMD_ARGC) {
		case 0:
			apsel = dap->apsel;
			break;
		case 1:
			COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], apsel);
			if (apsel > DP_APSEL_MAX)
				return ERROR_COMMAND_SYNTAX_ERROR;
			break;
		default:
			return ERROR_COMMAND_SYNTAX_ERROR;
	}

	return dap_info_command(CMD, &dap->ap[apsel]);
}

static const struct command_registration dap_subcommand_handlers[] = {
	{
		.name = "create",
		.mode = COMMAND_ANY,
		.jim_handler = jim_dap_create,
		.usage = "name '-chain-position' name",
		.help = "Creates a new DAP instance",
	},
	{
		.name = "names",
		.mode = COMMAND_ANY,
		.jim_handler = jim_dap_names,
		.usage = "",
		.help = "Lists all registered DAP instances by name",
	},
	{
		.name = "init",
		.mode = COMMAND_ANY,
		.handler = handle_dap_init,
		.usage = "",
		.help = "Initialize all registered DAP instances"
	},
	{
		.name = "info",
		.handler = handle_dap_info_command,
		.mode = COMMAND_EXEC,
		.help = "display ROM table for MEM-AP of current target "
		"(default currently selected AP)",
		.usage = "[ap_num]",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration dap_commands[] = {
	{
		.name = "dap",
		.mode = COMMAND_CONFIG,
		.help = "DAP commands",
		.chain = dap_subcommand_handlers,
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE
};

int dap_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, dap_commands);
}
