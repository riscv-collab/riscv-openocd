// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <time.h>

#include "ila_vcd.h"

#define VCD_ALPHABET_START '!'
#define VCD_ALPHABET_END '~'
#define VCD_ALPHABET_LEN (VCD_ALPHABET_END - VCD_ALPHABET_START + 1)

#define VCD_ID_MAX_LEN 3
typedef char vcd_id_t[VCD_ID_MAX_LEN + 1];

static void bit_to_vcd_id(uint32_t bit, vcd_id_t id)
{
	int i = 0;

	bit++;

	while (bit > 0) {
		id[i++] = VCD_ALPHABET_START + (bit % VCD_ALPHABET_LEN);
		bit /= VCD_ALPHABET_LEN;
	}

	id[i] = '\0';
}

static void ila_device_vcd_print_unknown_variable(uint32_t probe_width, uint32_t msb, uint32_t lsb,
	FILE *fp)
{
	vcd_id_t id;

	bit_to_vcd_id(probe_width - msb - 1, id);
	fprintf(fp, "$var reg %u %s probe [%u", msb - lsb + 1, id, msb);
	if (msb != lsb)
		fprintf(fp, ":%u", lsb);
	fprintf(fp, "] $end\n");
}

static void ila_device_vcd_print_variables(struct ila_device *device, struct ila_probe *probe, FILE *fp)
{
	uint32_t bit = device->probe_width - 1;
	uint32_t i;
	vcd_id_t id;

	if (probe) {
		for (i = 0; i < probe->parts_size; i++) {
			if (bit > probe->parts[i].msb) {
				ila_device_vcd_print_unknown_variable(device->probe_width, bit, probe->parts[i].msb + 1, fp);
				bit = probe->parts[i].msb;
			}

			bit_to_vcd_id(device->probe_width - bit - 1, id);
			fprintf(fp, "$var reg %u %s %s $end\n", bit - probe->parts[i].lsb + 1, id, probe->parts[i].name);

			if (bit)
				bit = probe->parts[i].lsb - 1;
			else
				bit = (uint32_t)-1;
		}
	}

	if (bit != (uint32_t)-1)
		ila_device_vcd_print_unknown_variable(device->probe_width, bit, 0, fp);

	bit_to_vcd_id(device->probe_width, id);
	fprintf(fp, "$var reg 1 %s _TRIGGER $end\n", id);
	bit_to_vcd_id(device->probe_width + 1, id);
	fprintf(fp, "$var reg 1 %s _WINDOW $end\n", id);
	bit_to_vcd_id(device->probe_width + 2, id);
	fprintf(fp, "$var reg 1 %s _GAP $end\n", id);
}

static int vcd_data_get_bit(uint8_t *vcd_data, uint32_t bit)
{
	return (vcd_data[bit / 8] >> (bit % 8)) & 1;
}

static bool vcd_data_is_changed(uint8_t *vcd_data, uint32_t msb, uint32_t lsb, uint32_t sample_byte_size,
	uint32_t sample)
{
	uint32_t i;

	if (sample == 0)
		return true;

	for (i = lsb; i <= msb; i++) {
		if (vcd_data_get_bit(vcd_data, sample_byte_size * (sample - 1) * 8 + i)
				!= vcd_data_get_bit(vcd_data, sample_byte_size * sample * 8 + i))
			return true;
	}

	return false;
}

static void vcd_data_print_val(uint8_t *vcd_data, uint32_t probe_width, uint32_t msb, uint32_t lsb,
	uint32_t sample_byte_size, uint32_t sample, FILE *fp)
{
	uint32_t i;
	vcd_id_t id;

	if (vcd_data_is_changed(vcd_data, msb, lsb, sample_byte_size, sample)) {
		if (msb != lsb)
			fprintf(fp, "b");

		for (i = msb + 1; i-- > lsb;)
			fprintf(fp, "%d", vcd_data_get_bit(vcd_data, sample_byte_size * sample * 8 + i));

		if (msb != lsb)
			fprintf(fp, " ");

		bit_to_vcd_id(probe_width - msb - 1, id);
		fprintf(fp, "%s\n", id);
	}
}

static void ila_device_vcd_print_values(struct ila_device *device, struct ila_probe *probe, uint8_t *vcd_data, FILE *fp)
{
	uint32_t bit, i, j;
	vcd_id_t id;
	uint32_t probe_vcd_reg_count;
	uint32_t vcd_reg_byte_size;
	uint32_t sample_byte_size;

	probe_vcd_reg_count = (device->probe_width + ILA_TAP__TAP_VCD_REG__WIDTH - 1) / ILA_TAP__TAP_VCD_REG__WIDTH;
	vcd_reg_byte_size = (ILA_TAP__TAP_VCD_REG__WIDTH + 7) / 8;
	sample_byte_size = vcd_reg_byte_size * probe_vcd_reg_count;

	for (i = 0; i < device->sample_depth; i++) {
		if (i == device->trigger_pos || vcd_data_is_changed(vcd_data, device->probe_width - 1, 0, sample_byte_size, i))
			fprintf(fp, "#%u\n", i);

		if (i == 0)
			fprintf(fp, "$dumpvars\n");

		bit = device->probe_width - 1;

		if (probe) {
			for (j = 0; j < probe->parts_size; j++) {
				if (bit > probe->parts[j].msb) {
					vcd_data_print_val(vcd_data, device->probe_width, bit, probe->parts[j].msb + 1,
						sample_byte_size, i, fp);
					bit = probe->parts[j].msb;
				}

				vcd_data_print_val(vcd_data, device->probe_width, bit, probe->parts[j].lsb, sample_byte_size, i, fp);

				if (probe->parts[j].lsb)
					bit = probe->parts[j].lsb - 1;
				else
					bit = (uint32_t)-1;
			}
		}

		if (bit != (uint32_t)-1)
			vcd_data_print_val(vcd_data, device->probe_width, bit, 0, sample_byte_size, i, fp);

		if (i == device->trigger_pos) {
			bit_to_vcd_id(device->probe_width, id);
			fprintf(fp, "1%s\n", id);
		} else if (i == 0) {
			bit_to_vcd_id(device->probe_width, id);
			fprintf(fp, "0%s\n", id);
		}

		if (i == 0) {
			bit_to_vcd_id(device->probe_width + 1, id);
			fprintf(fp, "1%s\n", id);
			bit_to_vcd_id(device->probe_width + 2, id);
			fprintf(fp, "0%s\n", id);
			fprintf(fp, "$end\n");
		}
	}
}

COMMAND_HELPER(ila_device_save_vcd, struct ila_device *device, uint8_t *vcd_data)
{
	time_t timer = 0;
	struct tm *tp;
	char str_time[256];
	char tmp_filename[PATH_MAX];
	char *filename;
	FILE *fp;
	struct ila_probe *probe = NULL;
	uint32_t i;

	time(&timer);
	tp = localtime(&timer);

	if (device->filename[0] == '\0') {
		strftime(str_time, sizeof(str_time), "%Y%m%d_%H%M%S", tp);
		snprintf(tmp_filename, PATH_MAX, "%s_probe[%u][%u]_%s.vcd", device->tap->dotted_name,
			device->probe_id, device->probe_sel, str_time);
		filename = tmp_filename;
	} else {
		filename = device->filename;
	}

	fp = fopen(filename, "w");
	if (!fp) {
		int e = errno;
		command_print(CMD, "failed to open file '%s': %s", filename, strerror(e));
		return ERROR_WAIT;
	}

	strftime(str_time, sizeof(str_time), "%Y-%b-%d %H:%M:%S", tp);

	for (i = 0; i < device->probes_size; i++) {
		if (device->probes[i].id == device->probe_id && device->probes[i].sel == device->probe_sel) {
			probe = &device->probes[i];
			break;
		}
	}

	fprintf(fp, "$date\n        %s\n$end\n", str_time);
	fprintf(fp, "$version\n        OpenOCD " VERSION RELSTR "\n$end\n");
	fprintf(fp, "$timescale\n        1ps\n$end\n");
	fprintf(fp, "$scope module dut $end\n");
	ila_device_vcd_print_variables(device, probe, fp);
	fprintf(fp, "$upscope $end\n");
	fprintf(fp, "$enddefinitions $end\n");
	ila_device_vcd_print_values(device, probe, vcd_data, fp);
	// Add extra timestamp for gtkwave view
	fprintf(fp, "#%u\n", device->sample_depth);

	fclose(fp);

	command_print(CMD, "ila data saved to %s", filename);

	return ERROR_OK;
}
