// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elct_cmd.h"

#include <helper/log.h>

#include "axi_cmd.h"
#include "bps_cmd.h"
#include "ila_cmd.h"

int elct_register_commands(struct command_context *cmd_ctx)
{
	int ret;

	ret = axi_register_commands(cmd_ctx);
	if (ret != ERROR_OK)
		return ret;

	ret = bps_register_commands(cmd_ctx);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_register_commands(cmd_ctx);

	return ret;
}

void elct_cleanup(void)
{
	ila_cleanup_all();
}
