/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__ILA_VCD_H
#define TARGET__ELCT__ILA_VCD_H

#include "ila_hw.h"

COMMAND_HELPER(ila_device_save_vcd, struct ila_device *device, uint8_t *vcd_data);

#endif
