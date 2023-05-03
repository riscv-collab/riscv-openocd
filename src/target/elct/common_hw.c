// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common_hw.h"

struct jtag_tap *jtag_tap_by_idcode(uint32_t idcode)
{
	struct jtag_tap *tap = jtag_all_taps();

	while (tap) {
		if (tap->idcode == idcode)
			return tap;

		tap = tap->next_tap;
	}

	return NULL;
}

int tap_reg_read(struct jtag_tap *tap, uint8_t *addr, int width, uint8_t *data)
{
	struct scan_field field;

	field.num_bits = tap->ir_length;
	field.out_value = addr;
	field.in_value = NULL;
	field.check_value = NULL;
	field.check_mask = NULL;

	jtag_add_ir_scan(tap, &field, TAP_IDLE);

	field.num_bits = width;
	field.in_value = data;
	field.out_value = NULL;

	jtag_add_dr_scan(tap, 1, &field, TAP_IDLE);

	return jtag_execute_queue();
}

int tap_reg_write(struct jtag_tap *tap, uint8_t *addr, int width, uint8_t *data)
{
	struct scan_field field;

	field.num_bits = tap->ir_length;
	field.out_value = addr;
	field.in_value = NULL;
	field.check_value = NULL;
	field.check_mask = NULL;

	jtag_add_ir_scan(tap, &field, TAP_IDLE);

	field.num_bits = width;
	field.out_value = data;

	jtag_add_dr_scan(tap, 1, &field, TAP_IDLE);

	return jtag_execute_queue();
}
