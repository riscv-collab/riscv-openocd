// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jansson.h>

#include "ila_hw.h"
#include "ila_json.h"

#define PARAMETERS_OBJ_NAME "PARAMETERS"
#define INSTANCE_ID_PARAM_NAME "ILA_INST_ID"
#define PROBE_WIDTH_PARAM_NAME "LA_PROBE_WIDTH"
#define SAMPLE_DEPTH_PARAM_NAME "SAMPLE_DEPTH"
#define NPROBES_PARAM_NAME "LA_NPROBE"
#define NCLK_PARAM_NAME "LA_NCLK"

#define PROBES_ARR_NAME "PROBES"
#define PROBE_ID_PARAM_NAME "PROBE_ID"
#define PROBE_SEL_PARAM_NAME "PROBE_SEL"
#define PROBE_CLK_PARAM_NAME "CLK_ID"

#define PARTS_ARR_NAME "PARTS"
#define PART_NAME_PARAM_NAME "NAME"
#define PART_MSB_PARAM_NAME "MSB"
#define PART_LSB_PARAM_NAME "LSB"

static COMMAND_HELPER(ila_device_load_json_parameters, struct ila_device *device, json_t *root)
{
	json_t *parameters, *param;
	unsigned long idcode;

	/* PARAMETERS */
	parameters = json_object_get(root, PARAMETERS_OBJ_NAME);
	if (!parameters) {
		command_print(CMD, "json error: " PARAMETERS_OBJ_NAME " object is missing");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	if (!json_is_object(parameters)) {
		command_print(CMD, "json error: " PARAMETERS_OBJ_NAME " is not an object");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	/* ILA_INST_ID */
	param = json_object_get(parameters, INSTANCE_ID_PARAM_NAME);
	if (!json_is_string(param)) {
		command_print(CMD, "json error: " INSTANCE_ID_PARAM_NAME " is not a string");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	idcode = strtoul(json_string_value(param), NULL, 16);
	if (device->tap->idcode != idcode) {
		command_print(CMD, "json error: TAP %s IDCODE mismatch: 0x%08x != 0x%08lx", device->tap->dotted_name,
			device->tap->idcode, idcode);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	/* LA_PROBE_WIDTH */
	param = json_object_get(parameters, PROBE_WIDTH_PARAM_NAME);
	if (!json_is_integer(param)) {
		command_print(CMD, "json error: " PROBE_WIDTH_PARAM_NAME " is not an integer");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->probe_width = json_integer_value(param);

	/* SAMPLE_DEPTH */
	param = json_object_get(parameters, SAMPLE_DEPTH_PARAM_NAME);
	if (!json_is_integer(param)) {
		command_print(CMD, "json error: " SAMPLE_DEPTH_PARAM_NAME " is not an integer");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->sample_depth = json_integer_value(param);

	/* LA_NPROBE */
	param = json_object_get(parameters, NPROBES_PARAM_NAME);
	if (!json_is_integer(param)) {
		command_print(CMD, "json error: " NPROBES_PARAM_NAME " is not an integer");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->nprobes = json_integer_value(param);

	/* LA_NCLK */
	param = json_object_get(parameters, NCLK_PARAM_NAME);
	if (!json_is_integer(param)) {
		command_print(CMD, "json error: " NCLK_PARAM_NAME " is not an integer");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->nclk = json_integer_value(param);

	return ERROR_OK;
}

static COMMAND_HELPER(ila_device_load_json_probe_parts, struct ila_device *device, json_t *prb, size_t i)
{
	json_t *parts, *prt, *param;
	size_t j;

	/* PARTS */
	parts = json_object_get(prb, PARTS_ARR_NAME);
	if (!json_is_array(parts)) {
		command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME " is not an array", i);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->probes[i].parts_size = json_array_size(parts);

	device->probes[i].parts = calloc(device->probes[i].parts_size, sizeof(struct ila_part));

	for (j = 0; j < device->probes[i].parts_size; j++) {
		/* PARTS[j] */
		prt = json_array_get(parts, j);
		if (!json_is_object(prt)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu] is not an object",
				i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}

		/* NAME */
		param = json_object_get(prt, PART_NAME_PARAM_NAME);
		if (!json_is_string(param)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu]." PART_NAME_PARAM_NAME
				" is not a string", i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
		strncpy(device->probes[i].parts[j].name, json_string_value(param),
			sizeof(device->probes[i].parts[j].name) - 1);

		/* MSB */
		param = json_object_get(prt, PART_MSB_PARAM_NAME);
		if (!json_is_integer(param)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu]." PART_MSB_PARAM_NAME
				" is not an integer", i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
		device->probes[i].parts[j].msb = json_integer_value(param);

		/* LSB */
		param = json_object_get(prt, PART_LSB_PARAM_NAME);
		if (!json_is_integer(param)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu]." PART_LSB_PARAM_NAME
				" is not an integer", i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
		device->probes[i].parts[j].lsb = json_integer_value(param);

		if (device->probes[i].parts[j].lsb > device->probes[i].parts[j].msb) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu] " PART_LSB_PARAM_NAME
				" > " PART_MSB_PARAM_NAME, i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}

		if (device->probes[i].parts[j].msb >= device->probe_width) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PARTS_ARR_NAME "[%zu] " PART_MSB_PARAM_NAME
				" is outside of " PROBE_WIDTH_PARAM_NAME, i, j);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
	}

	return ERROR_OK;
}

static COMMAND_HELPER(ila_device_load_json_probes, struct ila_device *device, json_t *root)
{
	json_t *probes, *prb, *param;
	size_t i;
	int ret;

	/* PROBES */
	probes = json_object_get(root, PROBES_ARR_NAME);
	if (!json_is_array(probes)) {
		command_print(CMD, "json error: " PROBES_ARR_NAME " is not an array");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}
	device->probes_size = json_array_size(probes);

	device->probes = calloc(device->probes_size, sizeof(struct ila_probe));

	for (i = 0; i < device->probes_size; i++) {
		/* PROBES[i] */
		prb = json_array_get(probes, i);
		if (!json_is_object(prb)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu] is not an object", i);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}

		/* PROBE_ID */
		param = json_object_get(prb, PROBE_ID_PARAM_NAME);
		if (!json_is_integer(param)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PROBE_ID_PARAM_NAME " is not an integer", i);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
		device->probes[i].id = json_integer_value(param);
		if (device->probes[i].id >= device->nprobes) {
			command_print(CMD,
				"json error: " PROBES_ARR_NAME "[%zu]." PROBE_ID_PARAM_NAME " is outside of " NPROBES_PARAM_NAME, i);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}

		/* PROBE_SEL */
		param = json_object_get(prb, PROBE_SEL_PARAM_NAME);
		if (param) {
			if (!json_is_integer(param)) {
				command_print(CMD,
					"json error: " PROBES_ARR_NAME "[%zu]." PROBE_SEL_PARAM_NAME " is not an integer", i);
				return ERROR_COMMAND_ARGUMENT_INVALID;
			}
			device->probes[i].sel = json_integer_value(param);
		}

		/* CLK_ID */
		param = json_object_get(prb, PROBE_CLK_PARAM_NAME);
		if (!json_is_integer(param)) {
			command_print(CMD, "json error: " PROBES_ARR_NAME "[%zu]." PROBE_CLK_PARAM_NAME " is not an integer", i);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}
		device->probes[i].clk = json_integer_value(param);
		if (device->probes[i].clk >= device->nclk) {
			command_print(CMD,
				"json error: " PROBES_ARR_NAME "[%zu]." PROBE_CLK_PARAM_NAME " is outside of " NCLK_PARAM_NAME, i);
			return ERROR_COMMAND_ARGUMENT_INVALID;
		}

		/* PARTS */
		ret = CALL_COMMAND_HANDLER(ila_device_load_json_probe_parts, device, prb, i);
		if (ret != ERROR_OK)
			return ret;
	}

	return ERROR_OK;
}

static COMMAND_HELPER(ila_device_sort_parts, struct ila_device *device)
{
	uint32_t i, j, k;
	struct ila_part tmp;

	for (i = 0; i < device->probes_size; i++) {
		for (j = 0; j < device->probes[i].parts_size - 1; j++) {
			for (k = j + 1; k < device->probes[i].parts_size; k++) {
				if (device->probes[i].parts[j].msb < device->probes[i].parts[k].msb) {
					tmp = device->probes[i].parts[j];
					device->probes[i].parts[j] = device->probes[i].parts[k];
					device->probes[i].parts[k] = tmp;
				}
			}
		}

		for (j = 0; j < device->probes[i].parts_size - 1; j++) {
			if (device->probes[i].parts[j].lsb <= device->probes[i].parts[j + 1].msb) {
				command_print(CMD, "json error: " PROBES_ARR_NAME "[%u] part %s overlaps with part %s", i,
					device->probes[i].parts[j].name, device->probes[i].parts[j + 1].name);
				return ERROR_COMMAND_ARGUMENT_INVALID;
			}
		}
	}

	return ERROR_OK;
}

COMMAND_HELPER(ila_device_load_json, struct ila_device *device, const char *path)
{
	int ret = ERROR_COMMAND_ARGUMENT_INVALID;

	json_error_t error;
	json_t *root;

	root = json_load_file(path, 0, &error);
	if (!root) {
		command_print(CMD, "json error: on line %d - %s", error.line, error.text);
		return ret;
	}
	if (!json_is_object(root)) {
		command_print(CMD, "json error: root is not an object");
		goto decref;
	}

	if (CALL_COMMAND_HANDLER(ila_device_load_json_parameters, device, root) != ERROR_OK)
		goto decref;

	if (CALL_COMMAND_HANDLER(ila_device_load_json_probes, device, root) != ERROR_OK)
		goto decref;

	if (CALL_COMMAND_HANDLER(ila_device_sort_parts, device) != ERROR_OK)
		goto decref;

	ret = ERROR_OK;

decref:
	json_decref(root);

	return ret;
}
