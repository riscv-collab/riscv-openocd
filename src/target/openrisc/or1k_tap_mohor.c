// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2013 by Franck Jullien                                  *
 *   elec4fun@gmail.com                                                    *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "or1k_tap.h"
#include "or1k.h"

#include <jtag/jtag.h>

#define OR1K_TAP_INST_DEBUG	0x8

static int or1k_tap_mohor_init(struct or1k_jtag *jtag_info)
{
	LOG_DEBUG("Initialising OpenCores JTAG TAP");

	/* Put TAP into state where it can talk to the debug interface
	 * by shifting in correct value to IR.
	 */

	/* Ensure TAP is reset - maybe not necessary*/
	jtag_add_tlr();

	struct jtag_tap *tap = jtag_info->tap;
	struct scan_field field;
	uint8_t ir_value = OR1K_TAP_INST_DEBUG;

	field.num_bits = tap->ir_length;
	field.out_value = &ir_value;
	field.in_value = NULL;

	jtag_add_ir_scan(tap, &field, TAP_IDLE);

	return jtag_execute_queue();
}

static struct or1k_tap_ip mohor_tap = {
	.name = "mohor",
	.init = or1k_tap_mohor_init,
};

int or1k_tap_mohor_register(void)
{
	list_add_tail(&mohor_tap.list, &tap_list);
	return 0;
}
