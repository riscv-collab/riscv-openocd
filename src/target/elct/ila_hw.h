/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__ILA_HW_H
#define TARGET__ELCT__ILA_HW_H

#include <limits.h>

#include "common_hw.h"
#include "helper/command.h"

/*
 * TAP registers
 */
/* ID */
#define ILA_TAP__ID__ADDR 0x01
#define ILA_TAP__ID__WIDTH 32

/* APB_ACCESS */
#define ILA_TAP__APB_ACCESS__ADDR 0x09
#define ILA_TAP__APB_ACCESS__WIDTH_TO1 42
#define ILA_TAP__APB_ACCESS__WIDTH_TO2 44

#define ILA_TAP__APB_ACCESS__STATUS__SHIFT 0
#define ILA_TAP__APB_ACCESS__STATUS__MASK 0x0000000000000003ULL
#define ILA_TAP__APB_ACCESS__STATUS__VALUE__IDLE 0x0
#define ILA_TAP__APB_ACCESS__STATUS__VALUE__WAIT 0x1
#define ILA_TAP__APB_ACCESS__STATUS__VALUE__OKAY 0x2
#define ILA_TAP__APB_ACCESS__STATUS__VALUE__ERR 0x3

#define ILA_TAP__APB_ACCESS__DATA__SHIFT 2
#define ILA_TAP__APB_ACCESS__DATA__MASK 0x00000003FFFFFFFCULL

#define ILA_TAP__APB_ACCESS__ADDRESS_TO1__SHIFT 34
#define ILA_TAP__APB_ACCESS__ADDRESS_TO1__MASK 0x000000FC00000000ULL
#define ILA_TAP__APB_ACCESS__ADDRESS_TO2__SHIFT 34
#define ILA_TAP__APB_ACCESS__ADDRESS_TO2__MASK 0x000003FC00000000ULL

#define ILA_TAP__APB_ACCESS__OPERATION_TO1__SHIFT 40
#define ILA_TAP__APB_ACCESS__OPERATION_TO1__MASK 0x0000030000000000ULL
#define ILA_TAP__APB_ACCESS__OPERATION_TO2__SHIFT 42
#define ILA_TAP__APB_ACCESS__OPERATION_TO2__MASK 0x00000C0000000000ULL
#define ILA_TAP__APB_ACCESS__OPERATION__VALUE__IDLE 0x0
#define ILA_TAP__APB_ACCESS__OPERATION__VALUE__WRITE 0x1
#define ILA_TAP__APB_ACCESS__OPERATION__VALUE__READ 0x2

/* TAP_VCD_REG */
#define ILA_TAP__TAP_VCD_REG__ADDR 0x1C
#define ILA_TAP__TAP_VCD_REG__WIDTH 32

/* BYPASS */
#define ILA_TAP__BYPASS__ADDR 0x1F
#define ILA_TAP__BYPASS__WIDTH 1

/*
 * APB registers
 */
#define ILA_APB__TRIG_CAP_MASK__COUNT 8
#define ILA_APB__TRIG_CAP_MASK__WIDTH 4
#define ILA_APB__TRIG_CAP_MASK__MASK 0xFU

#define ILA_APB__TRIG_CAP_MASK__X__SHIFT(X) ((X) * ILA_APB__TRIG_CAP_MASK__WIDTH)
#define ILA_APB__TRIG_CAP_MASK__X__MASK(X) (ILA_APB__TRIG_CAP_MASK__MASK << ILA_APB__TRIG_CAP_MASK__X__SHIFT(X))

#define ILA_APB__TRIG_CAP_MASK__VALUE__IMPOSSIBLE 0x0
#define ILA_APB__TRIG_CAP_MASK__VALUE__FALLING_EDGE 0x2
#define ILA_APB__TRIG_CAP_MASK__VALUE__CONST_0 0x3
#define ILA_APB__TRIG_CAP_MASK__VALUE__RISING_EDGE 0x4
#define ILA_APB__TRIG_CAP_MASK__VALUE__BOTH_EDGE 0x6
#define ILA_APB__TRIG_CAP_MASK__VALUE__NONE_EDGE 0x9
#define ILA_APB__TRIG_CAP_MASK__VALUE__CONST_1 0xC
#define ILA_APB__TRIG_CAP_MASK__VALUE__DONT_CARE 0xF

/* SCRATCH */
#define ILA_APB__SCRATCH__ADDR 0x00

/* CFG_FLAG */
#define ILA_APB__CFG_FLAG__ADDR 0x04

#define ILA_APB__CFG_FLAG__START_CAP__SHIFT 0
#define ILA_APB__CFG_FLAG__START_CAP__MASK 0x00000001U

#define ILA_APB__CFG_FLAG__CAP_RST_N__SHIFT 1
#define ILA_APB__CFG_FLAG__CAP_RST_N__MASK 0x00000002U

#define ILA_APB__CFG_FLAG__TRIG_MATCH_INV__SHIFT 2
#define ILA_APB__CFG_FLAG__TRIG_MATCH_INV__MASK 0x00000004U
#define ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__AND 0
#define ILA_APB__CFG_FLAG__TRIG_MATCH_INV__VALUE__OR 1

#define ILA_APB__CFG_FLAG__CAP_MATCH_INV__SHIFT 3
#define ILA_APB__CFG_FLAG__CAP_MATCH_INV__MASK 0x00000008U
#define ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__AND 0
#define ILA_APB__CFG_FLAG__CAP_MATCH_INV__VALUE__OR 1

#define ILA_APB__CFG_FLAG__TRIG_EXT_EN__SHIFT 4
#define ILA_APB__CFG_FLAG__TRIG_EXT_EN__MASK 0x00000010U

#define ILA_APB__CFG_FLAG__CAP_CTRL_SEL_CLK__SHIFT 5
#define ILA_APB__CFG_FLAG__CAP_CTRL_SEL_CLK__MASK 0x00000020U
#define ILA_APB__CFG_FLAG__CAP_CTRL_SEL_CLK__VALUE__SYS 0
#define ILA_APB__CFG_FLAG__CAP_CTRL_SEL_CLK__VALUE__TCK 1

#define ILA_APB__CFG_FLAG__OUT_PIN__SHIFT 6
#define ILA_APB__CFG_FLAG__OUT_PIN__MASK 0x00000040U

#define ILA_APB__CFG_FLAG__START_CAP_RESP__SHIFT 7
#define ILA_APB__CFG_FLAG__START_CAP_RESP__MASK 0x00000080U

/* PROBE_SEL */
#define ILA_APB__PROBE_SEL__ADDR 0x08

/* CLK_SEL */
#define ILA_APB__CLK_SEL__ADDR 0x0C

/* TRIG_PTR */
#define ILA_APB__TRIG_PTR__ADDR 0x10

/* TRIG_STS */
#define ILA_APB__TRIG_STS__ADDR 0x14

#define ILA_APB__TRIG_STS__TRIG_STS__SHIFT 0
#define ILA_APB__TRIG_STS__TRIG_STS__MASK 0x00000007U
#define ILA_APB__TRIG_STS__TRIG_STS__VALUE__STDBY 0x0
#define ILA_APB__TRIG_STS__TRIG_STS__VALUE__ARMED 0x1
#define ILA_APB__TRIG_STS__TRIG_STS__VALUE__START 0x5
#define ILA_APB__TRIG_STS__TRIG_STS__VALUE__TRIG_DONE 0x6
#define ILA_APB__TRIG_STS__TRIG_STS__VALUE__CAP_DONE 0x7

/* TRIG_MASK_W */
#define ILA_APB__TRIG_MASK_W__ADDR 0x18

/* TRIG_MASK_R */
#define ILA_APB__TRIG_MASK_R__ADDR 0x1C

/* CAP_MASK_W */
#define ILA_APB__CAP_MASK_W__ADDR 0x20

/* CAP_MASK_R */
#define ILA_APB__CAP_MASK_R__ADDR 0x24

/* VCD_DATA */
#define ILA_APB__VCD_DATA__ADDR 0x28

/*
 * ILA access functions
 */
int ila_lscu_reg_read(struct jtag_tap *tap, uint8_t addr, uint32_t *value);
int ila_lscu_reg_write(struct jtag_tap *tap, uint8_t addr, uint32_t value);
int ila_tap_vcd_reg_read(struct jtag_tap *tap, uint8_t *data, uint32_t count);

/*
 * ILA configuration functions
 */
struct ila_part {
	char name[128];
	unsigned int msb;
	unsigned int lsb;
};

struct ila_probe {
	uint32_t id;
	uint32_t sel;
	uint32_t clk;
	struct ila_part *parts;
	uint32_t parts_size;
};

struct ila_device {
	struct ila_device *next;

	struct jtag_tap *tap;
	uint32_t probe_width;
	uint32_t sample_depth;
	uint32_t _masks_reg_count;
	struct ila_probe *probes;
	uint32_t probes_size;
	uint32_t nprobes;
	uint32_t nclk;

	uint32_t probe_id;
	uint32_t probe_sel;
	uint32_t clk_id;
	uint32_t trigger_pos;
	uint32_t *trigger_mask;
	uint32_t *capture_mask;
	int trigger_mode;
	int capture_mode;
	bool trigger_sync;

	char filename[PATH_MAX];
};

void ila_device_free(struct ila_device *device);
int ila_device_set_probe_by_signal_name(struct ila_device *device, const char *name);
uint32_t ila_device_get_trig_cap_mask_val(uint32_t *trig_cap_mask, uint32_t probe_width, uint32_t bit_num);
uint32_t ila_device_get_trigger_mask_val(struct ila_device *device, uint32_t bit_num);
uint32_t ila_device_get_capture_mask_val(struct ila_device *device, uint32_t bit_num);
void ila_device_set_trig_cap_mask_val(uint32_t *trig_cap_mask, uint32_t probe_width, uint32_t bit_num, uint32_t val);
void ila_device_set_trigger_mask_val(struct ila_device *device, uint32_t bit_num, uint32_t val);
void ila_device_set_capture_mask_val(struct ila_device *device, uint32_t bit_num, uint32_t val);
int ila_device_load_settings(struct ila_device *device);
COMMAND_HELPER(ila_device_arm_trigger, struct ila_device *device);
COMMAND_HELPER(ila_device_start_capture, struct ila_device *device);
COMMAND_HELPER(ila_device_wait_for_data, struct ila_device *device, double seconds);
int ila_get_version(void);
void ila_set_version(int version);

#endif
