/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__ELCT_CMD_H
#define TARGET__ELCT__ELCT_CMD_H

#include "helper/command.h"

int elct_register_commands(struct command_context *cmd_ctx);
void elct_cleanup(void);

#endif
