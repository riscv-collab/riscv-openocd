// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <endian.h>
#include <time.h>
#include <unistd.h>

#include "ila_hw.h"
#include "ila_vcd.h"

#define APB_DELAY_US 10
#define APB_TIMEOUT 1000

#define ILA_DELAY_US 10
#define ILA_TIMEOUT 1000

/* #define DEBUG_ILA_HW */
/* #define VCD_DATA_SOURCE_LSCU */

static int ila_tap_apb_access_read(struct jtag_tap *tap, uint8_t *data)
{
	uint8_t tap_addr = ILA_TAP__APB_ACCESS__ADDR;

	return tap_reg_read(tap, &tap_addr, ILA_TAP__APB_ACCESS__WIDTH, data);
}

static int ila_tap_apb_access_write(struct jtag_tap *tap, uint8_t *data)
{
	uint8_t tap_addr = ILA_TAP__APB_ACCESS__ADDR;

	return tap_reg_write(tap, &tap_addr, ILA_TAP__APB_ACCESS__WIDTH, data);
}

int ila_tap_vcd_reg_read(struct jtag_tap *tap, uint8_t *data, uint32_t count)
{
	uint8_t tap_addr = ILA_TAP__TAP_VCD_REG__ADDR;
	uint32_t vcd_reg_byte_size = (ILA_TAP__TAP_VCD_REG__WIDTH + 7) / 8;
	uint32_t i;
	struct scan_field field;

	field.num_bits = tap->ir_length;
	field.out_value = &tap_addr;
	field.in_value = NULL;
	field.check_value = NULL;
	field.check_mask = NULL;

	jtag_add_ir_scan(tap, &field, TAP_IDLE);

	field.num_bits = ILA_TAP__TAP_VCD_REG__WIDTH;
	field.out_value = NULL;

	for (i = 0; i < count; i++) {
		field.in_value = data + i * vcd_reg_byte_size;

		jtag_add_dr_scan(tap, 1, &field, TAP_IDLE);
	}

	return jtag_execute_queue();
}

int ila_lscu_reg_read(struct jtag_tap *tap, uint8_t addr, uint32_t *value)
{
	int ret;

	uint64_t reg = 0;
	uint64_t reg_le;
	uint8_t *data;
	int count = 0;

	REG64_SET_FIELD(&reg, ILA_TAP__APB_ACCESS, OPERATION, ILA_TAP__APB_ACCESS__OPERATION__VALUE__READ);
	REG64_SET_FIELD(&reg, ILA_TAP__APB_ACCESS, ADDRESS, addr);

	reg_le = htole64(reg);
	data = (uint8_t *)&reg_le;

	ret = ila_tap_apb_access_write(tap, data);
	if (ret != ERROR_OK)
		return ret;

	do {
		uint8_t status;
		reg_le = 0;

		ret = ila_tap_apb_access_read(tap, data);
		if (ret != ERROR_OK)
			return ret;

		reg = le64toh(reg_le);

		*value = (uint32_t)REG64_GET_FIELD(reg, ILA_TAP__APB_ACCESS, DATA);
		status = (uint8_t)REG64_GET_FIELD(reg, ILA_TAP__APB_ACCESS, STATUS);

		if (status == ILA_TAP__APB_ACCESS__STATUS__VALUE__OKAY)
			break;

		if (status == ILA_TAP__APB_ACCESS__STATUS__VALUE__ERR)
			return ERROR_FAIL;

		if (count >= APB_TIMEOUT)
			return ERROR_TIMEOUT_REACHED;

		usleep(APB_DELAY_US);
		count++;
	} while (1);

#ifdef DEBUG_ILA_HW
	printf("ILA LSCU read  0x%02X: 0x%08X\n", addr, *value);
#endif

	return ERROR_OK;
}

int ila_lscu_reg_write(struct jtag_tap *tap, uint8_t addr, uint32_t value)
{
	int ret;

	uint64_t reg = 0;
	uint64_t reg_le;
	uint8_t *data;
	int count = 0;

	REG64_SET_FIELD(&reg, ILA_TAP__APB_ACCESS, OPERATION, ILA_TAP__APB_ACCESS__OPERATION__VALUE__WRITE);
	REG64_SET_FIELD(&reg, ILA_TAP__APB_ACCESS, ADDRESS, addr);
	REG64_SET_FIELD(&reg, ILA_TAP__APB_ACCESS, DATA, value);

	reg_le = htole64(reg);
	data = (uint8_t *)&reg_le;

	ret = ila_tap_apb_access_write(tap, data);
	if (ret != ERROR_OK)
		return ret;

	do {
		uint8_t status;
		reg_le = 0;

		ret = ila_tap_apb_access_read(tap, data);
		if (ret != ERROR_OK)
			return ret;

		reg = le64toh(reg_le);

		status = (uint8_t)REG64_GET_FIELD(reg, ILA_TAP__APB_ACCESS, STATUS);

		if (status == ILA_TAP__APB_ACCESS__STATUS__VALUE__OKAY)
			break;

		if (status == ILA_TAP__APB_ACCESS__STATUS__VALUE__ERR)
			return ERROR_FAIL;

		if (count >= APB_TIMEOUT)
			return ERROR_TIMEOUT_REACHED;

		usleep(APB_DELAY_US);
		count++;
	} while (1);

#ifdef DEBUG_ILA_HW
	printf("ILA LSCU write 0x%02X: 0x%08X\n", addr, value);
#endif

	return ERROR_OK;
}

void ila_device_free(struct ila_device *device)
{
	uint32_t i;

	if (device) {
		if (device->probes) {
			for (i = 0; i < device->probes_size; i++) {
				if (device->probes[i].parts)
					free(device->probes[i].parts);
			}

			free(device->probes);
		}

		if (device->trigger_mask)
			free(device->trigger_mask);

		if (device->capture_mask)
			free(device->capture_mask);

		free(device);
	}
}

int ila_device_set_probe_by_signal_name(struct ila_device *device, const char *name)
{
	uint32_t i, j;

	for (i = 0; i < device->probes_size; i++) {
		for (j = 0; j < device->probes[i].parts_size; j++) {
			if (!strcmp(device->probes[i].parts[j].name, name)) {
				device->probe_id = device->probes[i].id;
				device->probe_sel = device->probes[i].sel;
				device->clk_id = device->probes[i].clk;
				return ERROR_OK;
			}
		}
	}

	return ERROR_FAIL;
}

uint32_t ila_device_get_trig_cap_mask_val(uint32_t *trig_cap_mask, uint32_t probe_width, uint32_t bit_num)
{
	uint32_t reg;
	uint32_t pos;

	if (bit_num >= probe_width)
		return 0;

	reg = bit_num / ILA_APB__TRIG_CAP_MASK__COUNT;
	pos = bit_num % ILA_APB__TRIG_CAP_MASK__COUNT;

	return (trig_cap_mask[reg] & ILA_APB__TRIG_CAP_MASK__X__MASK(pos)) >> ILA_APB__TRIG_CAP_MASK__X__SHIFT(pos);
}

uint32_t ila_device_get_trigger_mask_val(struct ila_device *device, uint32_t bit_num)
{
	return ila_device_get_trig_cap_mask_val(device->trigger_mask, device->probe_width, bit_num);
}

uint32_t ila_device_get_capture_mask_val(struct ila_device *device, uint32_t bit_num)
{
	return ila_device_get_trig_cap_mask_val(device->capture_mask, device->probe_width, bit_num);
}

void ila_device_set_trig_cap_mask_val(uint32_t *trig_cap_mask, uint32_t probe_width, uint32_t bit_num, uint32_t val)
{
	uint32_t reg;
	uint32_t pos;

	if (bit_num >= probe_width)
		return;

	reg = bit_num / ILA_APB__TRIG_CAP_MASK__COUNT;
	pos = bit_num % ILA_APB__TRIG_CAP_MASK__COUNT;

	trig_cap_mask[reg] &= ~ILA_APB__TRIG_CAP_MASK__X__MASK(pos);
	trig_cap_mask[reg] |= val << ILA_APB__TRIG_CAP_MASK__X__SHIFT(pos);
}

void ila_device_set_trigger_mask_val(struct ila_device *device, uint32_t bit_num, uint32_t val)
{
	ila_device_set_trig_cap_mask_val(device->trigger_mask, device->probe_width, bit_num, val);
}

void ila_device_set_capture_mask_val(struct ila_device *device, uint32_t bit_num, uint32_t val)
{
	ila_device_set_trig_cap_mask_val(device->capture_mask, device->probe_width, bit_num, val);
}

int ila_device_load_settings(struct ila_device *device)
{
	int ret;

	uint32_t cfg_flag = 0;
	uint32_t mask = 0;
	uint32_t i;

	ret = ila_lscu_reg_read(device->tap, ILA_APB__CFG_FLAG__ADDR, &cfg_flag);
	if (ret != ERROR_OK)
		return ret;

	device->trigger_mode = REG32_GET_FIELD(cfg_flag, ILA_APB__CFG_FLAG, TRIG_MATCH_INV);
	device->capture_mode = REG32_GET_FIELD(cfg_flag, ILA_APB__CFG_FLAG, CAP_MATCH_INV);
	device->trigger_sync = REG32_GET_FIELD(cfg_flag, ILA_APB__CFG_FLAG, TRIG_EXT_EN);

	ret = ila_lscu_reg_read(device->tap, ILA_APB__PROBE_SEL__ADDR, &device->probe_id);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_lscu_reg_read(device->tap, ILA_APB__CLK_SEL__ADDR, &device->clk_id);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_PTR__ADDR, &device->trigger_pos);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < device->_masks_reg_count; i++) {
		ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_MASK_R__ADDR, &mask);
		if (ret != ERROR_OK)
			return ret;
		if (device->trigger_mode == ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__OR)
			mask = ~mask;
		device->trigger_mask[i] = mask;
	}

	for (i = 0; i < device->_masks_reg_count; i++) {
		ret = ila_lscu_reg_read(device->tap, ILA_APB__CAP_MASK_R__ADDR, &mask);
		if (ret != ERROR_OK)
			return ret;
		if (device->capture_mode == ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__OR)
			mask = ~mask;
		device->capture_mask[i] = mask;
	}

	return ERROR_OK;
}

COMMAND_HELPER(ila_device_arm_trigger, struct ila_device *device)
{
	int ret;

	uint32_t cfg_flag;
	uint32_t mask;
	uint32_t trig_sts = 0;
	uint32_t i, count;

	cfg_flag = 0;
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_MATCH_INV, device->trigger_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_MATCH_INV, device->capture_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_EXT_EN, device->trigger_sync);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, OUT_PIN, 1);

	ret = ila_lscu_reg_write(device->tap, ILA_APB__CFG_FLAG__ADDR, cfg_flag);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_lscu_reg_write(device->tap, ILA_APB__CLK_SEL__ADDR, device->clk_id);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_lscu_reg_write(device->tap, ILA_APB__PROBE_SEL__ADDR, device->probe_id);
	if (ret != ERROR_OK)
		return ret;

	ret = ila_lscu_reg_write(device->tap, ILA_APB__TRIG_PTR__ADDR, device->trigger_pos);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < device->_masks_reg_count; i++) {
		mask = device->trigger_mask[i];
		if (device->trigger_mode == ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__OR)
			mask = ~mask;
		ret = ila_lscu_reg_write(device->tap, ILA_APB__TRIG_MASK_W__ADDR, mask);
		if (ret != ERROR_OK)
			return ret;
	}

	for (i = 0; i < device->_masks_reg_count; i++) {
		mask = device->capture_mask[i];
		if (device->capture_mode == ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__OR)
			mask = ~mask;
		ret = ila_lscu_reg_write(device->tap, ILA_APB__CAP_MASK_W__ADDR, mask);
		if (ret != ERROR_OK)
			return ret;
	}

	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_RST_N, 1);

	ret = ila_lscu_reg_write(device->tap, ILA_APB__CFG_FLAG__ADDR, cfg_flag);
	if (ret != ERROR_OK)
		return ret;

#ifdef ELCT_DDR_ILA_WORKAROUND
	if (device->clk_id == 0 && device->tap->idcode == 0x01020305 && strcasestr(device->tap->dotted_name, "ddr")) {
		ret = ila_lscu_reg_write(device->tap, ILA_APB__CLK_SEL__ADDR, 1);
		if (ret != ERROR_OK)
			return ret;
		ret = ila_lscu_reg_write(device->tap, ILA_APB__CLK_SEL__ADDR, 0);
		if (ret != ERROR_OK)
			return ret;
	}
#endif

	count = 0;
	do {
		ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_STS__ADDR, &trig_sts);
		if (ret != ERROR_OK)
			return ret;

		if (trig_sts == ILA_APB__TRIG_STS__TRIG_STS__VALUE__ARMED)
			break;

		if (count >= ILA_TIMEOUT) {
			command_print(CMD, "timeout waiting for trigger to become armed");
			return ERROR_WAIT;
		}

		usleep(ILA_DELAY_US);
		count++;
	} while (1);

	return ERROR_OK;
}

COMMAND_HELPER(ila_device_start_capture, struct ila_device *device)
{
	int ret;

	uint32_t cfg_flag;
	uint32_t trig_sts = 0;
	uint32_t count;

	ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_STS__ADDR, &trig_sts);
	if (ret != ERROR_OK)
		return ret;

	if (trig_sts != ILA_APB__TRIG_STS__TRIG_STS__VALUE__ARMED) {
		command_print(CMD, "trigger is not armed before starting capture");
		return ERROR_WAIT;
	}

	cfg_flag = 0;
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, START_CAP, 1);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_RST_N, 1);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_MATCH_INV, device->trigger_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_MATCH_INV, device->capture_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_EXT_EN, device->trigger_sync);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, OUT_PIN, 1);

	ret = ila_lscu_reg_write(device->tap, ILA_APB__CFG_FLAG__ADDR, cfg_flag);
	if (ret != ERROR_OK)
		return ret;

	count = 0;
	do {
		ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_STS__ADDR, &trig_sts);
		if (ret != ERROR_OK)
			return ret;

		if (trig_sts == ILA_APB__TRIG_STS__TRIG_STS__VALUE__START
				|| trig_sts == ILA_APB__TRIG_STS__TRIG_STS__VALUE__TRIG_DONE
				|| trig_sts == ILA_APB__TRIG_STS__TRIG_STS__VALUE__CAP_DONE)
			break;

		if (count >= ILA_TIMEOUT) {
			command_print(CMD, "timeout waiting for capture to start");
			return ERROR_WAIT;
		}

		usleep(ILA_DELAY_US);
		count++;
	} while (1);

	return ERROR_OK;
}

static COMMAND_HELPER(ila_device_read_vcd_data, struct ila_device *device)
{
	int ret = ERROR_OK;

	uint32_t probe_vcd_reg_count;
	uint32_t vcd_reg_byte_size;
	uint8_t *vcd_data;

	probe_vcd_reg_count = (device->probe_width + ILA_TAP__TAP_VCD_REG__WIDTH - 1) / ILA_TAP__TAP_VCD_REG__WIDTH;
	vcd_reg_byte_size = (ILA_TAP__TAP_VCD_REG__WIDTH + 7) / 8;

	vcd_data = malloc(vcd_reg_byte_size * probe_vcd_reg_count * device->sample_depth);

#ifndef VCD_DATA_SOURCE_LSCU
	ret = ila_tap_vcd_reg_read(device->tap, vcd_data, probe_vcd_reg_count * device->sample_depth);
	if (ret != ERROR_OK)
		goto exit;
#endif

#if defined(DEBUG_ILA_HW) || defined(VCD_DATA_SOURCE_LSCU)
	uint32_t i, j;

	for (i = 0; i < device->sample_depth; i++) {
		for (j = 0; j < probe_vcd_reg_count; j++) {
			size_t offset = vcd_reg_byte_size * (i * probe_vcd_reg_count + j);

#ifdef VCD_DATA_SOURCE_LSCU
			ret = ila_lscu_reg_read(device->tap, ILA_APB__VCD_DATA__ADDR, (uint32_t *)(vcd_data + offset));
			if (ret != ERROR_OK)
				goto exit;
#endif

#if defined(DEBUG_ILA_HW) && !defined(VCD_DATA_SOURCE_LSCU)
			printf("ILA VCD data: 0x%08X\n", *(uint32_t *)(vcd_data + offset));
#endif
		}
	}
#endif

	ret = CALL_COMMAND_HANDLER(ila_device_save_vcd, device, vcd_data);

exit:
	free(vcd_data);

	return ret;
}

COMMAND_HELPER(ila_device_wait_for_data, struct ila_device *device, double seconds)
{
	int ret;

	uint32_t cfg_flag;
	uint32_t trig_sts = 0;
	struct timespec ts;
	uint64_t nanoseconds;
	uint64_t start_time;
	uint64_t curr_time;
	uint32_t count;

	ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_STS__ADDR, &trig_sts);
	if (ret != ERROR_OK)
		return ret;

	if (trig_sts != ILA_APB__TRIG_STS__TRIG_STS__VALUE__START
			&& trig_sts != ILA_APB__TRIG_STS__TRIG_STS__VALUE__TRIG_DONE
			&& trig_sts != ILA_APB__TRIG_STS__TRIG_STS__VALUE__CAP_DONE) {
		command_print(CMD, "capture is not started before waiting for data");
		return ERROR_WAIT;
	}

	nanoseconds = seconds * 1000000000;

	clock_gettime(CLOCK_REALTIME, &ts);
	start_time = ts.tv_sec * 1000000000 + ts.tv_nsec;
	curr_time = start_time;

	do {
		if (trig_sts == ILA_APB__TRIG_STS__TRIG_STS__VALUE__CAP_DONE)
			break;

		usleep(ILA_DELAY_US);

		ret = ila_lscu_reg_read(device->tap, ILA_APB__TRIG_STS__ADDR, &trig_sts);
		if (ret != ERROR_OK)
			return ret;

		clock_gettime(CLOCK_REALTIME, &ts);
		curr_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

		if (curr_time - start_time > nanoseconds) {
			command_print(CMD, "timeout waiting for data");
			return ERROR_WAIT;
		}
	} while (1);

	cfg_flag = 0;
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_RST_N, 1);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_MATCH_INV, device->trigger_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_MATCH_INV, device->capture_mode);
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, TRIG_EXT_EN, device->trigger_sync);
#ifndef VCD_DATA_SOURCE_LSCU
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, CAP_CTRL_SEL_CLK, ILA_APB__CFG_FLAG__CAP_CTRL_SEL_CLK__VALUE__TCK);
#endif
	REG32_SET_FIELD(&cfg_flag, ILA_APB__CFG_FLAG, OUT_PIN, 1);

	ret = ila_lscu_reg_write(device->tap, ILA_APB__CFG_FLAG__ADDR, cfg_flag);
	if (ret != ERROR_OK)
		return ret;

	count = 0;
	do {
		ret = ila_lscu_reg_read(device->tap, ILA_APB__CFG_FLAG__ADDR, &cfg_flag);
		if (ret != ERROR_OK)
			return ret;

		if (REG32_GET_FIELD(cfg_flag, ILA_APB__CFG_FLAG, START_CAP_RESP) == 0)
			break;

		if (count >= ILA_TIMEOUT) {
			command_print(CMD, "timeout waiting for data transfer");
			return ERROR_WAIT;
		}

		usleep(ILA_DELAY_US);
		count++;
	} while (1);

	return CALL_COMMAND_HANDLER(ila_device_read_vcd_data, device);
}
