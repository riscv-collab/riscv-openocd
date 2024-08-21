// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "axi_cmd.h"
#include "axi_hw.h"

static struct jtag_tap *axi_tap;

COMMAND_HANDLER(check_axi_tap)
{
	if (axi_tap)
		return ERROR_OK;

	axi_tap = jtag_tap_by_idcode(AXI_INST_ID);

	if (!axi_tap) {
		command_print(CMD, "axi tap not found");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_axi_rl_command)
{
	int ret;

	uint8_t addr = 0;
	uint32_t value = 0;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], addr);

	ret = CALL_COMMAND_HANDLER(check_axi_tap);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_lscu_reg_read(axi_tap, addr, &value);

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);
	else
		command_print(CMD, "0x%02x: %08x", addr, value);

	return ret;
}

COMMAND_HANDLER(handle_axi_wl_command)
{
	int ret;

	uint8_t addr = 0;
	uint32_t value = 0;

	if (CMD_ARGC != 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], addr);

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], value);

	ret = CALL_COMMAND_HANDLER(check_axi_tap);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_lscu_reg_write(axi_tap, addr, value);

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);

	return ret;
}

COMMAND_HANDLER(handle_axi_ra_command)
{
	int ret;

	uint64_t addr = 0;
	uint32_t *data;
	unsigned int count = 1;

	if (CMD_ARGC < 1 || CMD_ARGC > 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u64, CMD_ARGV[0], addr);

	if (CMD_ARGC > 1)
		COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], count);

	data = malloc(sizeof(uint32_t) * count);

	ret = CALL_COMMAND_HANDLER(check_axi_tap);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_single_read_transaction(axi_tap, addr, data, count);

	if (ret == ERROR_OK) {
		unsigned int i, j;

		i = 0;
		while (i < count) {
			command_print_sameline(CMD, "0x%010" PRIx64 ":", addr + i * 4);
			for (j = 0; j < 8 && i < count; j++, i++)
				command_print_sameline(CMD, " %08x", data[i]);
			command_print_sameline(CMD, "\n");
		}
	} else {
		command_print(CMD, "Error: %d", ret);
	}

	free(data);

	return ret;
}

COMMAND_HELPER(parse_data_array, uint32_t *data)
{
	unsigned int i;

	for (i = 0; i < CMD_ARGC - 1; i++)
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[i + 1], data[i]);

	return ERROR_OK;
}

COMMAND_HANDLER(handle_axi_wa_command)
{
	int ret;

	uint64_t addr = 0;
	uint32_t *data;

	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u64, CMD_ARGV[0], addr);

	data = malloc(sizeof(uint32_t) * (CMD_ARGC - 1));

	ret = CALL_COMMAND_HANDLER(parse_data_array, data);
	if (ret != ERROR_OK)
		return ret;

	ret = CALL_COMMAND_HANDLER(check_axi_tap);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_single_write_transaction(axi_tap, addr, data, (CMD_ARGC - 1));

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);

	free(data);

	return ret;
}

static const struct command_registration axi_subcommand_handlers[] = {
	{
		.name = "rl",
		.mode = COMMAND_EXEC,
		.handler = handle_axi_rl_command,
		.help = "read JTAG2AXIM LSCU register",
		.usage = "address",
	},
	{
		.name = "wl",
		.mode = COMMAND_EXEC,
		.handler = handle_axi_wl_command,
		.help = "write JTAG2AXIM LSCU register",
		.usage = "address data",
	},
	{
		.name = "ra",
		.mode = COMMAND_EXEC,
		.handler = handle_axi_ra_command,
		.help = "send single AXI read transaction (32-bit words)",
		.usage = "address [count]",
	},
	{
		.name = "wa",
		.mode = COMMAND_EXEC,
		.handler = handle_axi_wa_command,
		.help = "send single AXI write transaction (32 bit words)",
		.usage = "address data [data ...]",
	},
	COMMAND_REGISTRATION_DONE,
};

static const struct command_registration axi_command_handlers[] = {
	{
		.name = "axi",
		.mode = COMMAND_ANY,
		.help = "use JTAG2AXIM block to send AXI transactions",
		.chain = axi_subcommand_handlers,
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE,
};

int axi_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, axi_command_handlers);
}
