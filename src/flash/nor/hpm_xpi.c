/*
 * Copyright (c) 2021 hpmicro
 *
 * SPDX-License-Identifier: BSD-3-Clause *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "imp.h"
#include <helper/bits.h>
#include <helper/binarybuffer.h>
#include <helper/time_support.h>
#include <target/algorithm.h>
#include <target/image.h>
#include "target/riscv/program.h"

#include "../../../contrib/loaders/flash/hpm_xpi/hpm_xpi_flash.h"
#define TIMEOUT_IN_MS (10000U)
#define ERASE_CHIP_TIMEOUT_IN_MS (100000U)
#define SECTOR_ERASE_TIMEOUT_IN_MS (100)
#define BLOCK_SIZE (4096U)
#define NOR_CFG_OPT_HEADER (0xFCF90000UL)

typedef struct {
	uint32_t total_sz_in_bytes;
	uint32_t sector_sz_in_bytes;
} hpm_flash_info_t;

typedef struct hpm_xpi_priv {
	uint32_t io_base;
	uint32_t header;
	uint32_t opt0;
	uint32_t opt1;
	bool probed;
} hpm_xpi_priv_t;

static int hpm_xpi_probe(struct flash_bank *bank)
{
	int retval = ERROR_OK;
	struct reg_param reg_params[5];
	hpm_xpi_priv_t *xpi_priv;
	int xlen;
	hpm_flash_info_t flash_info = {0};
	struct working_area *data_wa = NULL;
	struct target *target = bank->target;
	struct flash_sector *sectors = NULL;
	struct working_area *wa;

	LOG_DEBUG("%s", __func__);
	xpi_priv = bank->driver_priv;

	if (xpi_priv->probed) {
		xpi_priv->probed = false;
		bank->size = 0;
		bank->num_sectors = 0;
		bank->sectors = NULL;
	}

	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}
	if (target == NULL)
		target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		fflush(stdout);
		return ERROR_TARGET_NOT_HALTED;
	}
	xlen = riscv_xlen(target);

	if (target_alloc_working_area(target, sizeof(flash_algo),
					&wa) != ERROR_OK) {
		LOG_WARNING("Couldn't allocate %zd-byte working area.",
					sizeof(flash_algo));
		wa = NULL;
	} else {
		retval = target_write_buffer(target, wa->address,
				sizeof(flash_algo), flash_algo);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write code to 0x%" TARGET_PRIxADDR ": %d",
					wa->address, retval);
			target_free_working_area(target, wa);
			wa = NULL;
		}
	}

	if (wa == NULL)
		goto err;

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
	init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
	init_reg_param(&reg_params[4], "ra", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u64(reg_params[1].value, 0, xlen, xpi_priv->header);
	buf_set_u64(reg_params[2].value, 0, xlen, xpi_priv->opt0);
	buf_set_u64(reg_params[3].value, 0, xlen, xpi_priv->opt1);
	buf_set_u64(reg_params[4].value, 0, xlen, wa->address + FLASH_INIT + 4);
	retval = target_run_algorithm(target, 0, NULL, 5, reg_params,
			wa->address, wa->address + FLASH_INIT + 4, 500, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute run algorithm: %d", retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("init flash failed on target: 0x%" PRIx32, retval);
		goto err;
	}

	if (target_alloc_working_area(target, sizeof(flash_info),
					&data_wa) != ERROR_OK) {
		LOG_WARNING("Couldn't allocate %zd-byte working area.",
					sizeof(flash_info));
		goto err;
	}

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "ra", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u64(reg_params[1].value, 0, xlen, data_wa->address);
	buf_set_u64(reg_params[2].value, 0, xlen, wa->address + FLASH_GET_INFO + 4);

	retval = target_run_algorithm(target, 0, NULL, 3, reg_params,
			wa->address + FLASH_GET_INFO, wa->address + FLASH_GET_INFO + 4, 500, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to run algorithm at: %d", retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("flash get info failed on target: 0x%" PRIx32, retval);
		goto err;
	}

	retval = target_read_memory(target, data_wa->address, xlen >> 3,
			sizeof(flash_info) / (xlen >> 3), (uint8_t *)&flash_info);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to read memory at 0x%" TARGET_PRIxADDR ": %d", data_wa->address, retval);
		goto err;
	}

	bank->size = flash_info.total_sz_in_bytes;
	bank->num_sectors = flash_info.total_sz_in_bytes / flash_info.sector_sz_in_bytes;
	bank->write_start_alignment = 2;

	/* create and fill sectors array */
	sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);
	if (sectors == NULL) {
		LOG_ERROR("not enough memory");
		retval = ERROR_FAIL;
		goto err;
	}

	for (unsigned int sector = 0; sector < bank->num_sectors; sector++) {
		sectors[sector].offset = sector * (flash_info.sector_sz_in_bytes);
		sectors[sector].size =  flash_info.sector_sz_in_bytes;
		sectors[sector].is_erased = -1;
		sectors[sector].is_protected = 0;
	}

	bank->sectors = sectors;

	xpi_priv->probed = true;

err:
	for (uint8_t k = 0; k < ARRAY_SIZE(reg_params); k++)
		destroy_reg_param(&reg_params[k]);
	if (data_wa)
		target_free_working_area(target, data_wa);
	if (wa)
		target_free_working_area(target, wa);
	return retval;
}

static int hpm_xpi_auto_probe(struct flash_bank *bank)
{
	hpm_xpi_priv_t *xpi_priv = bank->driver_priv;
	if (xpi_priv->probed)
		return ERROR_OK;
	return hpm_xpi_probe(bank);
}

static int hpm_xpi_write(struct flash_bank *bank, const uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
	struct reg_param reg_params[5];
	int retval = ERROR_OK;
	struct target *target;
	struct working_area *data_wa = NULL;
	struct working_area *wa = NULL;
	uint32_t data_size = BLOCK_SIZE;
	uint32_t left = count, i = 0;
	int xlen;
	hpm_xpi_priv_t *xpi_priv = bank->driver_priv;

	LOG_DEBUG("%s", __func__);
	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}

	if (target == NULL)
		target = bank->target;

	if (target_alloc_working_area(target, sizeof(flash_algo),
					&wa) != ERROR_OK) {
		LOG_WARNING("Couldn't allocate %zd-byte working area.",
					sizeof(flash_algo));
		wa = NULL;
	} else {
		retval = target_write_buffer(target, wa->address,
				sizeof(flash_algo), flash_algo);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write code to 0x%" TARGET_PRIxADDR ": %d",
					wa->address, retval);
			target_free_working_area(target, wa);
			wa = NULL;
		}
	}

	if (wa == NULL)
		goto err;

	xlen = riscv_xlen(target);
	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
	init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
	init_reg_param(&reg_params[4], "ra", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u64(reg_params[1].value, 0, xlen, xpi_priv->header);
	buf_set_u64(reg_params[2].value, 0, xlen, xpi_priv->opt0);
	buf_set_u64(reg_params[3].value, 0, xlen, xpi_priv->opt1);
	buf_set_u64(reg_params[4].value, 0, xlen, wa->address + FLASH_INIT + 4);
	retval = target_run_algorithm(target, 0, NULL, 5, reg_params,
			wa->address, wa->address + FLASH_INIT + 4, 500, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute run algorithm: %d", retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("init flash failed on target: 0x%" PRIx32, retval);
		goto err;
	}

	/* memory buffer */
	while (target_alloc_working_area_try(target, data_size, &data_wa) != ERROR_OK) {
		data_size /= 2;
		if (data_size <= 256) {
			/* we already allocated the writing code, but failed to get a
			 * buffer, free the algorithm */
			target_free_working_area(target, wa);

			LOG_WARNING("no large enough working area available, can't do block memory writes");
			return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		}
	}

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
	init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);

	while (left >= data_size) {
		retval = target_write_buffer(target, data_wa->address, data_size, buffer + i * data_size);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write buffer to 0x%" TARGET_PRIxADDR ": %d", data_wa->address, retval);
			goto err;
		}

		buf_set_u32(reg_params[0].value, 0, xlen, bank->base);
		buf_set_u32(reg_params[1].value, 0, xlen, offset + i * data_size);
		buf_set_u32(reg_params[2].value, 0, xlen, data_wa->address);
		buf_set_u32(reg_params[3].value, 0, xlen, data_size);

		retval = target_run_algorithm(target, 0, NULL, 4, reg_params,
				wa->address + FLASH_PROGRAM, wa->address + FLASH_PROGRAM + 4, TIMEOUT_IN_MS, NULL);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to execute algorithm at 0x%" TARGET_PRIxADDR ": %d", wa->address, retval);
			goto err;
		}

		retval = buf_get_u32(reg_params[0].value, 0, xlen);
		if (retval) {
			LOG_ERROR("flash write failed on target: 0x%" PRIx32, retval);
			goto err;
		}
		i++;
		left -= data_size;
	}

	if (left) {
		retval = target_write_buffer(target, data_wa->address, left, buffer + i * data_size);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write buffer to 0x%" TARGET_PRIxADDR ": %d", data_wa->address, retval);
			goto err;
		}

		buf_set_u32(reg_params[0].value, 0, xlen, bank->base);
		buf_set_u32(reg_params[1].value, 0, xlen, offset + i * data_size);
		buf_set_u32(reg_params[2].value, 0, xlen, data_wa->address);
		buf_set_u32(reg_params[3].value, 0, xlen, left);

		retval = target_run_algorithm(target, 0, NULL, 4, reg_params,
				wa->address + FLASH_PROGRAM, wa->address + FLASH_PROGRAM + 4, TIMEOUT_IN_MS, NULL);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to execute algorithm at 0x%" TARGET_PRIxADDR ": %d", wa->address, retval);
			goto err;
		}

		retval = buf_get_u32(reg_params[0].value, 0, xlen);
		if (retval) {
			LOG_ERROR("flash write failed on target: 0x%" PRIx32, retval);
			goto err;
		}
	}

err:
	if (data_wa)
		target_free_working_area(target, data_wa);
	if (wa)
		target_free_working_area(target, wa);

	for (uint8_t k = 0; k < ARRAY_SIZE(reg_params); k++)
		destroy_reg_param(&reg_params[k]);
	return retval;
}

static int hpm_xpi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
	int retval = ERROR_OK;
	struct reg_param reg_params[5];
	struct target *target = bank->target;
	struct working_area *wa = NULL;
	int xlen;
	hpm_xpi_priv_t *xpi_priv = bank->driver_priv;

	LOG_DEBUG("%s", __func__);
	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}

	if (target == NULL)
		target = bank->target;

	xlen = riscv_xlen(target);
	if (target_alloc_working_area(target, sizeof(flash_algo),
					&wa) != ERROR_OK) {
		LOG_WARNING("Couldn't allocate %zd-byte working area.",
					sizeof(flash_algo));
		wa = NULL;
	} else {
		retval = target_write_buffer(target, wa->address,
				sizeof(flash_algo), flash_algo);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write code to 0x%" TARGET_PRIxADDR ": %d",
					wa->address, retval);
			target_free_working_area(target, wa);
			wa = NULL;
		}
	}

	if (wa == NULL)
		goto err;

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
	init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
	init_reg_param(&reg_params[4], "ra", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u64(reg_params[1].value, 0, xlen, xpi_priv->header);
	buf_set_u64(reg_params[2].value, 0, xlen, xpi_priv->opt0);
	buf_set_u64(reg_params[3].value, 0, xlen, xpi_priv->opt1);
	buf_set_u64(reg_params[4].value, 0, xlen, wa->address + FLASH_INIT + 4);
	retval = target_run_algorithm(target, 0, NULL, 5, reg_params,
			wa->address, wa->address + FLASH_INIT + 4, 500, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute run algorithm: %d", retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("init flash failed on target: 0x%" PRIx32, retval);
		goto err;
	}

	LOG_DEBUG("%s: from sector %u to sector %u", __func__, first, last);

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);

	buf_set_u32(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u32(reg_params[1].value, 0, xlen, first * bank->sectors[0].size);
	buf_set_u32(reg_params[2].value, 0, xlen, (last - first + 1) * bank->sectors[0].size);

	retval = target_run_algorithm(target, 0, NULL, 3, reg_params,
			wa->address + FLASH_ERASE, wa->address + FLASH_ERASE + 4,
			SECTOR_ERASE_TIMEOUT_IN_MS * (last - first + 1), NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute algorithm at 0x%" TARGET_PRIxADDR ": %d", wa->address, retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("flash erase failed on target: 0x%" PRIx32, retval);
		goto err;
	}

err:
	if (wa)
		target_free_working_area(target, wa);
	for (uint8_t k = 0; k < ARRAY_SIZE(reg_params); k++)
		destroy_reg_param(&reg_params[k]);
	return retval;
}

static int hpm_xpi_erase_chip(struct flash_bank *bank)
{
	int retval = ERROR_OK;
	struct reg_param reg_params[5];
	struct target *target = bank->target;
	struct working_area *wa = NULL;
	int xlen;
	hpm_xpi_priv_t *xpi_priv = bank->driver_priv;

	LOG_DEBUG("%s", __func__);
	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}

	if (target == NULL)
		target = bank->target;

	xlen = riscv_xlen(target);

	if (target_alloc_working_area(target, sizeof(flash_algo),
					&wa) != ERROR_OK) {
		LOG_WARNING("Couldn't allocate %zd-byte working area.",
					sizeof(flash_algo));
		wa = NULL;
	} else {
		retval = target_write_buffer(target, wa->address,
				sizeof(flash_algo), flash_algo);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write code to 0x%" TARGET_PRIxADDR ": %d",
					wa->address, retval);
			target_free_working_area(target, wa);
			wa = NULL;
		}
	}

	if (wa == NULL)
		goto err;

	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
	init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
	init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
	init_reg_param(&reg_params[4], "ra", xlen, PARAM_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);
	buf_set_u64(reg_params[1].value, 0, xlen, xpi_priv->header);
	buf_set_u64(reg_params[2].value, 0, xlen, xpi_priv->opt0);
	buf_set_u64(reg_params[3].value, 0, xlen, xpi_priv->opt1);
	buf_set_u64(reg_params[4].value, 0, xlen, wa->address + FLASH_INIT + 4);
	retval = target_run_algorithm(target, 0, NULL, 5, reg_params,
			wa->address, wa->address + FLASH_INIT + 4, 500, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute run algorithm: %d", retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("init flash failed on target: 0x%" PRIx32, retval);
		goto err;
	}
	init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
	buf_set_u64(reg_params[0].value, 0, xlen, bank->base);

	retval = target_run_algorithm(target, 0, NULL, 1, reg_params,
			wa->address + FLASH_ERASE_CHIP, wa->address + FLASH_ERASE_CHIP + 4, ERASE_CHIP_TIMEOUT_IN_MS, NULL);
	if (retval != ERROR_OK) {
		LOG_ERROR("Failed to execute algorithm at 0x%" TARGET_PRIxADDR ": %d", wa->address, retval);
		goto err;
	}

	retval = buf_get_u32(reg_params[0].value, 0, xlen);
	if (retval) {
		LOG_ERROR("flash erase chip failed on target: 0x%" PRIx32, retval);
		goto err;
	}

err:
	if (wa)
		target_free_working_area(target, wa);
	for (uint8_t k = 0; k < ARRAY_SIZE(reg_params); k++)
		destroy_reg_param(&reg_params[k]);
	return retval;
}


static int hpm_xpi_get_info(struct flash_bank *bank, struct command_invocation *cmd)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int hpm_xpi_protect(struct flash_bank *bank, int set,
	unsigned int first, unsigned int last)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int hpm_xpi_read(struct flash_bank *bank, uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
	LOG_DEBUG("%s", __func__);
	struct target *target = bank->target;
	int xlen;
	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}
	if (target == NULL)
		target = bank->target;

	xlen = riscv_xlen(target);

	return target_read_memory(bank->target, bank->base + offset, xlen >> 3, count / (xlen >> 3), buffer);
}

static int hpm_xpi_blank_check(struct flash_bank *bank)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int hpm_xpi_protect_check(struct flash_bank *bank)
{
	LOG_DEBUG("%s", __func__);
	/* Nothing to do. Protection is only handled in SW. */
	return ERROR_OK;
}

static int hpm_xpi_verify(struct flash_bank *bank, const uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
	int retval = ERROR_OK;
	hpm_xpi_priv_t *xpi_priv;
	struct target *target = bank->target;
	uint8_t *buf_on_target = NULL;
	int xlen;

	LOG_DEBUG("%s", __func__);
	xpi_priv = bank->driver_priv;
	if (!xpi_priv->probed) {
		LOG_ERROR("Flash bank not probed");
		return ERROR_FLASH_BANK_NOT_PROBED;
	}

	for (target = all_targets; target; target = target->next) {
		if (target->target_number == 0) {
			riscv_set_current_hartid(target, 0);
			target->coreid = 0;
			break;
		}
	}

	if (target == NULL)
		target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	buf_on_target = malloc(count);
	if (buf_on_target == NULL) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	xlen = riscv_xlen(target);
	retval = target_read_memory(target, bank->base + offset, xlen >> 3, count / (xlen >> 3), buf_on_target);
	if (ERROR_OK != retval)
		return retval;

	if (!memcmp(buf_on_target, buffer, count))
		return ERROR_OK;
	return ERROR_FAIL;
}

COMMAND_HANDLER(hpm_xpi_handle_erase_chip_command)
{
	int retval;
	struct flash_bank *bank;
	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (ERROR_OK != retval)
		return retval;

	LOG_DEBUG("%s", __func__);

	return hpm_xpi_erase_chip(bank);
}


static const struct command_registration hpm_xpi_exec_command_handlers[] = {
	{
		.name = "erase_chip",
		.handler = hpm_xpi_handle_erase_chip_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "erase entire flash device.",
	},
	COMMAND_REGISTRATION_DONE
};

const struct command_registration hpm_xpi_command_handlers[] = {
	{
		.name = "hpm_xpi",
		.mode = COMMAND_ANY,
		.help = "hpm_xpi command group",
		.usage = "",
		.chain = hpm_xpi_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

FLASH_BANK_COMMAND_HANDLER(hpm_xpi_flash_bank_command)
{
	hpm_xpi_priv_t *xpi_priv;
	uint32_t io_base;
	uint32_t header;
	uint32_t opt0;
	uint32_t opt1;

	LOG_DEBUG("%s", __func__);

	if (CMD_ARGC < 7)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[6], io_base);

	switch (CMD_ARGC) {
		case 7:
			header = NOR_CFG_OPT_HEADER + 1;
			opt1 = 0;
			opt0 = 7;
			break;
		case 8:
			header = NOR_CFG_OPT_HEADER + 1;
			opt1 = 0;
			COMMAND_PARSE_NUMBER(u32, CMD_ARGV[7], opt0);
			break;
		case 9:
			header = NOR_CFG_OPT_HEADER + 2;
			COMMAND_PARSE_NUMBER(u32, CMD_ARGV[7], opt0);
			COMMAND_PARSE_NUMBER(u32, CMD_ARGV[8], opt1);
			break;
		default:
			return ERROR_COMMAND_SYNTAX_ERROR;
	}
	xpi_priv = malloc(sizeof(struct hpm_xpi_priv));
	if (xpi_priv == NULL) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	bank->driver_priv = xpi_priv;
	xpi_priv->io_base = io_base;
	xpi_priv->header = header;
	xpi_priv->opt0 = opt0;
	xpi_priv->opt1 = opt1;
	xpi_priv->probed = false;

	return ERROR_OK;
}

static void hpm_xpi_free_driver_priv(struct flash_bank *bank)
{
	default_flash_free_driver_priv(bank);
}

struct flash_driver hpm_xpi_flash = {
	.name = "hpm_xpi",
	.flash_bank_command = hpm_xpi_flash_bank_command,
	.commands = hpm_xpi_command_handlers,
	.erase = hpm_xpi_erase,
	.protect = hpm_xpi_protect,
	.write = hpm_xpi_write,
	.read = hpm_xpi_read,
	.verify = hpm_xpi_verify,
	.probe = hpm_xpi_probe,
	.auto_probe = hpm_xpi_auto_probe,
	.erase_check = hpm_xpi_blank_check,
	.protect_check = hpm_xpi_protect_check,
	.info = hpm_xpi_get_info,
	.free_driver_priv = hpm_xpi_free_driver_priv,
};
