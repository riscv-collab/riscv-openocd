/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TARGET__ELCT__COMMON_HW_H
#define TARGET__ELCT__COMMON_HW_H

#include <stdint.h>

#include "jtag/jtag.h"

/*
 * Base modification functions
 */
static inline uint32_t reg32_get_field(uint32_t reg_value, int shift, uint32_t mask)
{
	return (reg_value & mask) >> shift;
}

static inline void reg32_set_field(uint32_t *reg_value, int shift, uint32_t mask, uint32_t value)
{
	*reg_value &= ~mask;
	*reg_value |= (value << shift) & mask;
}

static inline uint64_t reg64_get_field(uint64_t reg_value, int shift, uint64_t mask)
{
	return (reg_value & mask) >> shift;
}

static inline void reg64_set_field(uint64_t *reg_value, int shift, uint64_t mask, uint64_t value)
{
	*reg_value &= ~mask;
	*reg_value |= (value << shift) & mask;
}

#define REG32_GET_FIELD(reg_value, reg, field) \
	reg32_get_field(reg_value, reg ## __ ## field ## __SHIFT, reg ## __ ## field ## __MASK)

#define REG32_SET_FIELD(p_reg_value, reg, field, value) \
	reg32_set_field(p_reg_value, reg ## __ ## field ## __SHIFT, reg ## __ ## field ## __MASK, value)

#define REG64_GET_FIELD(reg_value, reg, field) \
	reg64_get_field(reg_value, reg ## __ ## field ## __SHIFT, reg ## __ ## field ## __MASK)

#define REG64_SET_FIELD(p_reg_value, reg, field, value) \
	reg64_set_field(p_reg_value, reg ## __ ## field ## __SHIFT, reg ## __ ## field ## __MASK, value)

/*
 * Base access functions
 */
struct jtag_tap *jtag_tap_by_idcode(uint32_t idcode);

int tap_reg_read(struct jtag_tap *tap, uint8_t *addr, int width, uint8_t *data);
int tap_reg_write(struct jtag_tap *tap, uint8_t *addr, int width, uint8_t *data);

#endif
