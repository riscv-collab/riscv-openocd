// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bps_cmd.h"
#include "common_hw.h"

#define BPS_INST_ID 0x01234567
#define BPS_TAP__IR_LEN 5
#define BPS_TAP__BYPASS_CTL__ADDR 0x02
#define BPS_TAP__BYPASS_CTL__WIDTH 32

#define ELCT_MAX_IR_SCAN_WIDTH 252
#define ELCT_MAX_TAP_COUNT 49

static uint8_t ir_scan_buffer[(ELCT_MAX_IR_SCAN_WIDTH + 7) / 8];
static uint8_t dr_scan_buffer[(ELCT_MAX_TAP_COUNT - 1 + BPS_TAP__BYPASS_CTL__WIDTH + 7) / 8];

static void buffer_set_bit(uint8_t *buffer, unsigned int bit_num, int bit_val)
{
	unsigned int byte_num = bit_num / 8;
	uint8_t byte_mask = 1 << (bit_num % 8);

	if (bit_val)
		buffer[byte_num] |= byte_mask;
	else
		buffer[byte_num] &= ~byte_mask;
}

static void buffer_set_val(uint8_t *buffer, unsigned int start_bit, uint32_t val, unsigned int val_len)
{
	unsigned int i;

	for (i = 0; i < val_len; i++)
		buffer_set_bit(buffer, start_bit + i, (val >> i) & 1);
}

COMMAND_HANDLER(handle_bps_command)
{
	uint32_t ctl = 0;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], ctl);

	memset(ir_scan_buffer, 0xff, sizeof(ir_scan_buffer));
	buffer_set_val(ir_scan_buffer, ELCT_MAX_IR_SCAN_WIDTH - 5, BPS_TAP__BYPASS_CTL__ADDR, BPS_TAP__IR_LEN);

	memset(dr_scan_buffer, 0, sizeof(dr_scan_buffer));
	buffer_set_val(dr_scan_buffer, ELCT_MAX_TAP_COUNT - 1, ctl, BPS_TAP__BYPASS_CTL__WIDTH);

	jtag_add_plain_ir_scan(ELCT_MAX_IR_SCAN_WIDTH, ir_scan_buffer, NULL, TAP_IDLE);
	jtag_add_plain_dr_scan(ELCT_MAX_TAP_COUNT - 1 + BPS_TAP__BYPASS_CTL__WIDTH, dr_scan_buffer, NULL, TAP_IDLE);
	jtag_add_tlr();
	jtag_execute_queue();

	return command_run_line(CMD_CTX, "shutdown");
}

static const struct command_registration bps_command_handlers[] = {
	{
		.name = "bps",
		.mode = COMMAND_EXEC,
		.handler = handle_bps_command,
		.help = "set bypass control value and shutdown openocd",
		.usage = "ctl",
	},
	COMMAND_REGISTRATION_DONE,
};

int bps_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, bps_command_handlers);
}
