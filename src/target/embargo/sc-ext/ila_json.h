/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__ILA_JSON_H
#define TARGET__ELCT__ILA_JSON_H

#include "helper/command.h"
#include "ila_hw.h"

COMMAND_HELPER(ila_device_load_json, struct ila_device *device, const char *path);

#endif
