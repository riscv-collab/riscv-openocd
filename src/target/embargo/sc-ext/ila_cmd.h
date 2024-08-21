/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__ILA_CMD_H
#define TARGET__ELCT__ILA_CMD_H

#include "helper/command.h"

int ila_register_commands(struct command_context *cmd_ctx);
void ila_cleanup_all(void);

#endif
