// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ila_cmd.h"
#include "ila_hw.h"
#include "ila_json.h"

static struct ila_device *first;
static struct ila_device *last;
static struct ila_device *current;

static char trig_cap_val_to_mask[1 << ILA_APB__TRIG_CAP_MASK__WIDTH] = {
	'#', /* IMPOSSIBLE */
	' ',
	'F', /* FALLING_EDGE */
	'0', /* CONST_0 */
	'R', /* RISING_EDGE */
	' ',
	'B', /* BOTH_EDGE */
	' ',
	' ',
	'N', /* NONE_EDGE */
	' ',
	' ',
	'1', /* CONST_1 */
	' ',
	' ',
	'X', /* DONT_CARE */
};

static char trig_cap_mask_to_val[256];

#define COMMAND_CHECK_CURRENT \
	do { \
		if (!current) { \
			command_print(CMD, "no ila devices registered"); \
			return ERROR_FAIL; \
		} \
	} while (0)

COMMAND_HANDLER(handle_ilas_command)
{
	int ret = ERROR_OK;

	struct ila_device *device;

	if (CMD_ARGC == 1) {
		struct jtag_tap *tap = jtag_tap_by_string(CMD_ARGV[0]);

		if (tap) {
			device = first;

			while (device) {
				if (device->tap == tap) {
					current = device;
					return ERROR_OK;
				}

				device = device->next;
			}
		}

		command_print(cmd, "TAP: %s is unknown, try one of:\n", CMD_ARGV[0]);
		ret = ERROR_FAIL;
	}

	command_print(CMD, "    TapName            IdCode     ProbeWidth SampleDepth");
	command_print(CMD, "--  ------------------ ---------- ---------- -----------");

	device = first;

	while (device) {
		command_print(CMD, "%2d%c %-18s 0x%08x %10u %11u",
			device->tap->abs_chain_position,
			((device == current) ? '*' : ' '),
			device->tap->dotted_name,
			device->tap->idcode,
			device->probe_width,
			device->sample_depth
		);

		device = device->next;
	}

	return ret;
}

COMMAND_HANDLER(handle_ila_new_command)
{
	int ret = ERROR_OK;

	struct ila_device *device = calloc(1, sizeof(struct ila_device));
	struct ila_device *tmp = first;

	if (CMD_ARGC == 3) {
		device->tap = jtag_tap_by_string(CMD_ARGV[0]);
		if (!device->tap) {
			ret = ERROR_COMMAND_ARGUMENT_INVALID;
			goto free;
		}

		ret = parse_u32(CMD_ARGV[1], &device->probe_width);
		if (ret != ERROR_OK)
			goto free;

		ret = parse_u32(CMD_ARGV[2], &device->sample_depth);
		if (ret != ERROR_OK)
			goto free;
	} else if (CMD_ARGC == 2) {
		device->tap = jtag_tap_by_string(CMD_ARGV[0]);
		if (!device->tap) {
			ret = ERROR_COMMAND_ARGUMENT_INVALID;
			goto free;
		}

		ret = CALL_COMMAND_HANDLER(ila_device_load_json, device, CMD_ARGV[1]);
		if (ret != ERROR_OK)
			goto free;
	} else {
		ret = ERROR_COMMAND_SYNTAX_ERROR;
		goto free;
	}

	while (tmp) {
		if (tmp->tap == device->tap) {
			command_print(CMD, "ila device already registered for tap %s", tmp->tap->dotted_name);
			ret = ERROR_COMMAND_ARGUMENT_INVALID;
			goto free;
		}

		tmp = tmp->next;
	}

	device->next = NULL;
	device->_masks_reg_count =
		(device->probe_width + ILA_APB__TRIG_CAP_MASK__COUNT - 1) / ILA_APB__TRIG_CAP_MASK__COUNT;
	device->trigger_mask = malloc(device->_masks_reg_count * sizeof(uint32_t));
	memset(device->trigger_mask, 0xFF, device->_masks_reg_count * sizeof(uint32_t));
	device->capture_mask = malloc(device->_masks_reg_count * sizeof(uint32_t));
	memset(device->capture_mask, 0xFF, device->_masks_reg_count * sizeof(uint32_t));

	if (last) {
		last->next = device;
		last = device;
	} else {
		first = device;
		last = device;
		current = device;
	}

	goto exit;

free:
	ila_device_free(device);

exit:
	return ret;
}

static COMMAND_HELPER(print_trig_cap_mask, uint32_t *trig_cap_mask, uint32_t probe_width)
{
	uint32_t i, j;

	for (i = (probe_width + 31) / 32; i-- > 0;) {
		command_print(CMD, "    %3d      %-3d %3d      %-3d %3d      %-3d %3d      %-3d",
			i * 32 + 31, i * 32 + 24, i * 32 + 23, i * 32 + 16, i * 32 + 15, i * 32 + 8, i * 32 + 7, i * 32);

		command_print_sameline(CMD, " ");

		for (j = 32; j-- > 0;) {
			if (j % 8 == 7)
				command_print_sameline(CMD, "     ");

			if (i * 32 + j < probe_width)
				command_print_sameline(CMD, "%c", trig_cap_val_to_mask[ila_device_get_trig_cap_mask_val(trig_cap_mask,
					probe_width, i * 32 + j)]);
			else
				command_print_sameline(CMD, " ");
		}

		command_print_sameline(CMD, "\n");
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_print_command)
{
	uint32_t i, j;

	COMMAND_CHECK_CURRENT;

	command_print(CMD, "ila device %s", current->tap->dotted_name);
	command_print(CMD, "  probes (%u):", current->probes_size);
	for (i = 0; i < current->probes_size; i++) {
		command_print(CMD, "    [%2u] ID: %u, SEL: %u, CLK: %u, PARTS (%u):", i, current->probes[i].id,
			current->probes[i].sel, current->probes[i].clk, current->probes[i].parts_size);
		for (j = 0; j < current->probes[i].parts_size; j++) {
			command_print(CMD, "      [%2u] NAME: \"%s\", MSB: %u, LSB: %u", j,
				current->probes[i].parts[j].name, current->probes[i].parts[j].msb, current->probes[i].parts[j].lsb);
		}
	}
	command_print(CMD, "  probe_width = %u", current->probe_width);
	command_print(CMD, "  sample_depth = %u", current->sample_depth);
	command_print(CMD, "  probe_id = %u", current->probe_id);
	command_print(CMD, "  probe_sel = %u", current->probe_sel);
	command_print(CMD, "  clk_id = %u", current->clk_id);
	command_print(CMD, "  trigger_pos = %u", current->trigger_pos);
	command_print(CMD, "  trigger_mask:");
	CALL_COMMAND_HANDLER(print_trig_cap_mask, current->trigger_mask, current->probe_width);
	command_print(CMD, "  capture_mask:");
	CALL_COMMAND_HANDLER(print_trig_cap_mask, current->capture_mask, current->probe_width);
	command_print(CMD, "  trigger_mode = %s",
		(current->trigger_mode == ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__AND) ? "and" : "or");
	command_print(CMD, "  capture_mode = %s",
		(current->capture_mode == ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__AND) ? "and" : "or");
	command_print(CMD, "  trigger_sync = %s", current->trigger_sync ? "on" : "off");

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_probe_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC == 3) {
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], current->probe_id);
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], current->probe_sel);
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], current->clk_id);
	} else if (CMD_ARGC == 1) {
		if (ila_device_set_probe_by_signal_name(current, CMD_ARGV[0]) != ERROR_OK) {
			command_print(CMD, "signal \"%s\" not found", CMD_ARGV[0]);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
	} else {
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_trigger_pos_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], current->trigger_pos);

	if (current->trigger_pos >= current->sample_depth) {
		command_print(CMD, "trigger_pos is outside of sample_depth");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	return ERROR_OK;
}

static COMMAND_HELPER(parse_trigger_capture_mask_command_args, uint32_t *msb, uint32_t *lsb)
{
	int ret;

	uint32_t i, j;
	char *colon;
	uint32_t mask_len;

	*msb = (uint32_t)-1;
	*lsb = (uint32_t)-1;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	for (i = 0; i < current->probes_size; i++) {
		if (current->probes[i].id == current->probe_id && current->probes[i].sel == current->probe_sel) {
			for (j = 0; j < current->probes[i].parts_size; j++) {
				if (!strcmp(CMD_ARGV[0], current->probes[i].parts[j].name)) {
					*msb = current->probes[i].parts[j].msb;
					*lsb = current->probes[i].parts[j].lsb;
					break;
				}
			}
			break;
		}
	}

	if (*msb == (uint32_t)-1 || *lsb == (uint32_t)-1) {
		colon = strchr(CMD_ARGV[0], ':');

		if (colon) {
			*colon = '\0';
			ret = parse_u32(CMD_ARGV[0], msb) | parse_u32(colon + 1, lsb);
			*colon = ':';

			if (ret != ERROR_OK) {
				command_print(CMD, "signal not found and failed to parse '%s' as range", CMD_ARGV[0]);
				return ERROR_COMMAND_ARGUMENT_INVALID;
			}

			if (*lsb > *msb) {
				i = *lsb;
				*lsb = *msb;
				*msb = i;
			}
		} else {
			ret = parse_u32(CMD_ARGV[0], msb);
			if (ret != ERROR_OK) {
				command_print(CMD, "signal not found and failed to parse '%s' as number", CMD_ARGV[0]);
				return ret;
			}

			*lsb = *msb;
		}
	}

	if (*msb >= current->probe_width) {
		command_print(CMD, "bit range [%u:%u] is outside of probe_width (%u)", *msb, *lsb, current->probe_width);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	mask_len = strlen(CMD_ARGV[1]);

	if (mask_len != *msb - *lsb + 1) {
		command_print(CMD, "wrong bit mask length %u != [%u:%u]", mask_len, *msb, *lsb);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_trigger_mask_command)
{
	int ret;

	uint32_t i;
	uint32_t msb = (uint32_t)-1;
	uint32_t lsb = (uint32_t)-1;
	int mask;

	ret = CALL_COMMAND_HANDLER(parse_trigger_capture_mask_command_args, &msb, &lsb);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < msb - lsb + 1; i++) {
		mask = CMD_ARGV[1][i];
		ila_device_set_trigger_mask_val(current, msb - i, trig_cap_mask_to_val[mask]);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_capture_mask_command)
{
	int ret;

	uint32_t i;
	uint32_t msb = (uint32_t)-1;
	uint32_t lsb = (uint32_t)-1;
	int mask;

	ret = CALL_COMMAND_HANDLER(parse_trigger_capture_mask_command_args, &msb, &lsb);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < msb - lsb + 1; i++) {
		mask = CMD_ARGV[1][i];
		ila_device_set_capture_mask_val(current, lsb + i, trig_cap_mask_to_val[mask]);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_trigger_mode_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (!strcmp(CMD_ARGV[0], "and")) {
		current->trigger_mode = ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__AND;
	} else if (!strcmp(CMD_ARGV[0], "or")) {
		current->trigger_mode = ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__OR;
	} else {
		command_print(CMD, "unknown trigger mask mode: '%s'", CMD_ARGV[0]);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_capture_mode_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (!strcmp(CMD_ARGV[0], "and")) {
		current->capture_mode = ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__AND;
	} else if (!strcmp(CMD_ARGV[0], "or")) {
		current->capture_mode = ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__OR;
	} else {
		command_print(CMD, "unknown trigger mask mode: '%s'", CMD_ARGV[0]);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_trigger_sync_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_BOOL(CMD_ARGV[0], current->trigger_sync, "on", "off");

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_file_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	strncpy(current->filename, CMD_ARGV[0], PATH_MAX - 1);

	return ERROR_OK;
}

COMMAND_HANDLER(handle_ila_load_command)
{
	int ret;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	ret = ila_device_load_settings(current);
	if (ret != ERROR_OK) {
		command_print(CMD, "error while reading device settings: %d", ret);
		return ret;
	}

	return ERROR_OK;
}

static COMMAND_HELPER(ila_arm_helper, struct ila_device *device)
{
	int ret = CALL_COMMAND_HANDLER(ila_device_arm_trigger, device);
	if (ret != ERROR_OK && ret != ERROR_WAIT)
		command_print(CMD, "error while writing device settings: %d", ret);
	return ret;
}

COMMAND_HANDLER(handle_ila_arm_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	return CALL_COMMAND_HANDLER(ila_arm_helper, current);
}

COMMAND_HANDLER(handle_ila_arm_all_command)
{
	int ret;

	struct ila_device *tmp = first;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	while (tmp) {
		if (tmp->trigger_sync && tmp != current) {
			ret = CALL_COMMAND_HANDLER(ila_arm_helper, tmp);
			if (ret != ERROR_OK)
				return ret;
		}

		tmp = tmp->next;
	}

	return CALL_COMMAND_HANDLER(ila_arm_helper, current);
}

static COMMAND_HELPER(ila_start_helper, struct ila_device *device)
{
	int ret = CALL_COMMAND_HANDLER(ila_device_start_capture, device);
	if (ret != ERROR_OK && ret != ERROR_WAIT)
		command_print(CMD, "error while writing device settings: %d", ret);
	return ret;
}

COMMAND_HANDLER(handle_ila_start_command)
{
	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	return CALL_COMMAND_HANDLER(ila_start_helper, current);
}

COMMAND_HANDLER(handle_ila_start_all_command)
{
	int ret;

	struct ila_device *tmp = first;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	while (tmp) {
		if (tmp->trigger_sync && tmp != current) {
			ret = CALL_COMMAND_HANDLER(ila_start_helper, tmp);
			if (ret != ERROR_OK)
				return ret;
		}

		tmp = tmp->next;
	}

	return CALL_COMMAND_HANDLER(ila_start_helper, current);
}

static COMMAND_HELPER(ila_wait_parser, double *seconds)
{
	char *end = NULL;

	*seconds = 0.1;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC > 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (CMD_ARGC == 1) {
		*seconds = strtod(CMD_ARGV[0], &end);
		if (end && *end) {
			command_print(CMD, "seconds option value ('%s') is not valid", CMD_ARGV[0]);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
	}

	return ERROR_OK;
}

static COMMAND_HELPER(ila_wait_helper, struct ila_device *device, double seconds)
{
	int ret = CALL_COMMAND_HANDLER(ila_device_wait_for_data, device, seconds);
	if (ret != ERROR_OK && ret != ERROR_WAIT)
		command_print(CMD, "error while waiting for data: %d", ret);
	return ret;
}

COMMAND_HANDLER(handle_ila_wait_command)
{
	int ret;

	double seconds = 0;

	ret = CALL_COMMAND_HANDLER(ila_wait_parser, &seconds);
	if (ret != ERROR_OK)
		return ret;

	return CALL_COMMAND_HANDLER(ila_wait_helper, current, seconds);
}

COMMAND_HANDLER(handle_ila_wait_all_command)
{
	int ret;

	double seconds = 0;
	struct ila_device *tmp = first;

	ret = CALL_COMMAND_HANDLER(ila_wait_parser, &seconds);
	if (ret != ERROR_OK)
		return ret;

	while (tmp) {
		if (tmp->trigger_sync && tmp != current) {
			ret = CALL_COMMAND_HANDLER(ila_wait_helper, tmp, seconds);
			if (ret != ERROR_OK)
				return ret;
		}

		tmp = tmp->next;
	}

	return CALL_COMMAND_HANDLER(ila_wait_helper, current, seconds);
}

COMMAND_HANDLER(handle_ila_rv_command)
{
	int ret;

	uint32_t value = 0;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	ret = ila_tap_vcd_reg_read(current->tap, (uint8_t *)&value, 1);

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);
	else
		command_print(CMD, "ILA VCD data: %08x", value);

	return ret;
}

COMMAND_HANDLER(handle_ila_rl_command)
{
	int ret;

	uint8_t addr = 0;
	uint32_t value = 0;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], addr);

	ret = ila_lscu_reg_read(current->tap, addr, &value);

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);
	else
		command_print(CMD, "0x%02x: %08x", addr, value);

	return ret;
}

COMMAND_HANDLER(handle_ila_wl_command)
{
	int ret;

	uint8_t addr = 0;
	uint32_t value = 0;

	COMMAND_CHECK_CURRENT;

	if (CMD_ARGC != 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], addr);

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], value);

	ret = ila_lscu_reg_write(current->tap, addr, value);

	if (ret != ERROR_OK)
		command_print(CMD, "Error: %d", ret);

	return ret;
}

COMMAND_HANDLER(handle_ila_version_command)
{
	int version = 0;

	if (CMD_ARGC > 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (CMD_ARGC == 0) {
		command_print(CMD, "%d", ila_get_version());
	} else {
		COMMAND_PARSE_NUMBER(int, CMD_ARGV[0], version);

		if (version != 1 && version != 2) {
			command_print(CMD, "Error: Wrong ILA version - %d", version);
			return ERROR_FAIL;
		}

		ila_set_version(version);
	}

	return ERROR_OK;
}

#define TRIG_CAP_MASK_DOC_STR \
	"bit_mask must be the same length as bit range or signal size. Bit order in bit_mask is always MSB->LSB. " \
	"signal_name must be present in currently selected probe. Bit masks: R - rising edge; F - falling edge; " \
	"B - either edge; 0 - const 0; 1 - const 1; N - no transition; X - don't care, # - impossible."

static const struct command_registration ila_subcommand_handlers[] = {
	{
		.name = "new",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_new_command,
		.help = "Create ila device.",
		.usage = "tapname json_or_probe_width [sample_depth]",
	},
	{
		.name = "print",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_print_command,
		.help = "Print current ila device settings.",
		.usage = "",
	},
	{
		.name = "probe",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_probe_command,
		.help = "Set probe id, probe sel and clock id for current ila device.",
		.usage = "probe_id_or_signal_name [probe_sel clk_id]",
	},
	{
		.name = "trigger_pos",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_trigger_pos_command,
		.help = "Set trigger position for current ila device.",
		.usage = "pos",
	},
	{
		.name = "trigger_mask",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_trigger_mask_command,
		.help = "Set trigger mask for current ila device. " TRIG_CAP_MASK_DOC_STR,
		.usage = "bit_range_or_signal_name bit_mask",
	},
	{
		.name = "capture_mask",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_capture_mask_command,
		.help = "Set capture mask for current ila device. " TRIG_CAP_MASK_DOC_STR,
		.usage = "bit_range_or_signal_name bit_mask",
	},
	{
		.name = "trigger_mode",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_trigger_mode_command,
		.help = "Set trigger mask mode (AND or OR) for current ila device.",
		.usage = "and|or",
	},
	{
		.name = "capture_mode",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_capture_mode_command,
		.help = "Set capture mask mode (AND or OR) for current ila device.",
		.usage = "and|or",
	},
	{
		.name = "trigger_sync",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_trigger_sync_command,
		.help = "Enable / disable trigger input from other ila devices.",
		.usage = "on|off",
	},
	{
		.name = "file",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_file_command,
		.help = "Set output VCD file name. Default: <ila_device>_probe[<probe_id>][<probe_sel>]_<curr_date>.vcd.",
		.usage = "filename",
	},
	{
		.name = "load",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_load_command,
		.help = "Load current ila device settings from hardware.",
		.usage = "",
	},
	{
		.name = "arm",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_arm_command,
		.help = "Commit current ila device settings to hardware and arm trigger.",
		.usage = "",
	},
	{
		.name = "arm_all",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_arm_all_command,
		.help = "Commit current ila device and all ila devices with trigger_sync=on settings to hardware and arm "
			"triggers.",
		.usage = "",
	},
	{
		.name = "start",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_start_command,
		.help = "Start capture on current ila device. Settings must be committed beforehand via `ila arm` or "
			"`ila arm_all` command.",
		.usage = "",
	},
	{
		.name = "start_all",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_start_all_command,
		.help = "Start capture on current ila device and all ila devices with trigger_sync=on. Settings must be "
			"committed beforehand via `ila arm` or `ila arm_all` command.",
		.usage = "",
	},
	{
		.name = "wait",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_wait_command,
		.help = "Wait for capture to finish on current ila device, read data and save it to file. seconds - timeout in "
			"seconds for waiting (can be a fractional number), default: 0.1.",
		.usage = "[seconds]",
	},
	{
		.name = "wait_all",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_wait_all_command,
		.help = "Wait for capture to finish on current ila device and all ila devices with trigger_sync=on, read data "
			"and save it to file. seconds - timeout in seconds for waiting (can be a fractional number), default: 0.1.",
		.usage = "[seconds]",
	},
	{
		.name = "rv",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_rv_command,
		.help = "Read JTAG2ILA TAP VCD data register.",
		.usage = "",
	},
	{
		.name = "rl",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_rl_command,
		.help = "Read JTAG2ILA LSCU register.",
		.usage = "address",
	},
	{
		.name = "wl",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_wl_command,
		.help = "Write JTAG2ILA LSCU register.",
		.usage = "address data",
	},
	{
		.name = "version",
		.mode = COMMAND_EXEC,
		.handler = handle_ila_version_command,
		.help = "Display or set global ILA version.",
		.usage = "[version]"
	},
	COMMAND_REGISTRATION_DONE,
};

static const struct command_registration ila_command_handlers[] = {
	{
		.name = "ilas",
		.handler = handle_ilas_command,
		.mode = COMMAND_EXEC,
		.help = "Change current ila device (one parameter) or print table of all ila device (no parameters)",
		.usage = "[tap]",
	},
	{
		.name = "ila",
		.mode = COMMAND_EXEC,
		.help = "Use JTAG2ILA block to capture ILAs.",
		.chain = ila_subcommand_handlers,
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE,
};

int ila_register_commands(struct command_context *cmd_ctx)
{
	trig_cap_mask_to_val['#'] = ILA_APB__TRIG_CAP_MASK__VALUE__IMPOSSIBLE;
	trig_cap_mask_to_val['F'] = ILA_APB__TRIG_CAP_MASK__VALUE__FALLING_EDGE;
	trig_cap_mask_to_val['0'] = ILA_APB__TRIG_CAP_MASK__VALUE__CONST_0;
	trig_cap_mask_to_val['R'] = ILA_APB__TRIG_CAP_MASK__VALUE__RISING_EDGE;
	trig_cap_mask_to_val['B'] = ILA_APB__TRIG_CAP_MASK__VALUE__BOTH_EDGE;
	trig_cap_mask_to_val['N'] = ILA_APB__TRIG_CAP_MASK__VALUE__NONE_EDGE;
	trig_cap_mask_to_val['1'] = ILA_APB__TRIG_CAP_MASK__VALUE__CONST_1;
	trig_cap_mask_to_val['X'] = ILA_APB__TRIG_CAP_MASK__VALUE__DONT_CARE;

	return register_commands(cmd_ctx, NULL, ila_command_handlers);
}

void ila_cleanup_all(void)
{
	struct ila_device *tmp;

	while (first) {
		tmp = first->next;
		ila_device_free(first);
		first = tmp;
	}
}
