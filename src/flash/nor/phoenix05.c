/***************************************************************************
 *   Copyright (C) 2021 by David Lin                                       *
 *   David Lin <peng.lin@fhsjdz.com>                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "target/target.h"
#include "target/algorithm.h"
#include "target/target_type.h"
#include "jtag/jtag.h"

#define FLASH_BASE      (0x00002000UL)	  /*!< ( FLASH   ) Base Address */
#define NVR_BASE        (0x00006000UL)	  /*!< ( NVR     ) Base Address */
#define EEPROM_BASE     (0x00007000UL)	  /*!< ( EEPROM  ) Base Address */
#define EFC_BASE        (0x0000C000UL)
#define SYSC_BASE       (0x0000C400UL)
#define MODEL_CHK       (0x0000C3FCUL)

#define EFC_CR          (EFC_BASE  + 0x00)
#define EFC_Tnvs        (EFC_BASE  + 0x04)
#define EFC_Tprog       (EFC_BASE  + 0x08)
#define EFC_Tpgs        (EFC_BASE  + 0x0C)
#define EFC_Trcv        (EFC_BASE  + 0x10)
#define EFC_Terase      (EFC_BASE  + 0x14)
#define EFC_WPT         (EFC_BASE  + 0x18)
#define EFC_OPR         (EFC_BASE  + 0x1C)
#define EFC_STS         (EFC_BASE  + 0x24)

#define SYSC_CLKCTRCFG  (SYSC_BASE + 0x00)
#define SYSC_WRPROCFG   (SYSC_BASE + 0x04)

struct phnx_info
{
	uint32_t page_size;
	int num_pages;
	int sector_size;
	int prot_block_size;

	bool probed;
	struct target *target;
};

/**************************** OOCD FLASH ACTIONS *********************************/
static int phnx_probe(struct flash_bank *bank)
{
	struct phnx_info *chip = (struct phnx_info *)bank->driver_priv;
	struct target *target = chip->target;
	int flash_kb, ram_kb;
	int res;
	unsigned int model;
	unsigned int status;
	if (chip->probed == true)
		return ERROR_OK;

	/* disable wdt */
	res = target_read_u32(target, SYSC_CLKCTRCFG, &status);
	if (res != ERROR_OK)
	{
		LOG_ERROR("Couldn't read SYSC_CLKCTRCFG register");
		return res;
	}

	status &= ~(0x01 << 2);
	target_write_u32(target, SYSC_WRPROCFG, 0x5a);
	target_write_u32(target, SYSC_WRPROCFG, 0xa5);
	res = target_write_u32(target, SYSC_CLKCTRCFG, status);
	if (res != ERROR_OK)
	{
		LOG_ERROR("Couldn't write SYSC_CLKCTRCFG register");
		return res;
	}

	res = target_read_u32(target, MODEL_CHK, &model);
	if (res != ERROR_OK)
	{
		LOG_ERROR("Couldn't read MODEL_CHK register");
		return res;
	}

	if (bank->base != FLASH_BASE)
	{
		LOG_ERROR("bank->base shall be 0x%08x.", (unsigned int)FLASH_BASE);
		return ERROR_FAIL;
	}

	if (model == 0xF05)
	{
		flash_kb = 16, ram_kb = 2;
	}
	else
	{
		LOG_ERROR("phoenix model probe failed.");
		return ERROR_FAIL;
	}

	chip->sector_size = chip->page_size = 128;
	chip->num_pages = flash_kb * 1024 / chip->sector_size;
	bank->size = flash_kb * 1024;
	bank->num_sectors = chip->num_pages;
	bank->sectors = alloc_block_array(0, chip->sector_size, bank->num_sectors);
	if (!bank->sectors)
	{
		LOG_ERROR("Couldn't alloc memory for sectors.");
		return ERROR_FAIL;
	}

	/* Done */
	chip->probed = true;

	LOG_INFO("flash: phoenix (%" PRIu32 "KB , %" PRIu32 "KB RAM)", flash_kb, ram_kb);

	return ERROR_OK;
}

static int phnx_protect(struct flash_bank *bank, int set, unsigned int first_prot_bl, unsigned int last_prot_bl)
{
	LOG_INFO("phnx_protect involked. set=%d, first=%d, last=%d.", set, (int)first_prot_bl, (int)last_prot_bl);
	// TODO:
	return ERROR_OK;
}

static int phnx_erase(struct flash_bank *bank, unsigned int first_sect, unsigned int last_sect)
{
	LOG_INFO("phnx_erase involked. first=%d, last=%d.", (int)first_sect, (int)last_sect);
	return ERROR_OK;
}

static int phnx_batch_write(struct flash_bank *bank, const uint8_t *buffer,
							uint32_t offset, uint32_t count)
{
	struct phnx_info *chip = (struct phnx_info *)bank->driver_priv;
	struct target *target = bank->target;
	uint32_t buffer_size = 1024;
	struct working_area *write_algorithm;
	struct working_area *source;
	struct reg_param reg_params[3];
	int retval = ERROR_OK;

	LOG_INFO("phnx_batch_write offset=%u, count=%u.", offset, count);
	if (offset % chip->sector_size != 0)
	{
		LOG_ERROR("offset not aligned by sector size %d", chip->sector_size);
		return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
	}
	if (bank->target->state != TARGET_HALTED)
	{
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}
	if (!chip->probed)
	{
		if (phnx_probe(bank) != ERROR_OK)
			return ERROR_FLASH_BANK_NOT_PROBED;
	}

	static const uint8_t flash_write_code[] = {
#include "../../../contrib/loaders/flash/phoenix05/phoenix05_write.inc"
	};

	/* flash write code */
	if (target_alloc_working_area(target, sizeof(flash_write_code),
								  &write_algorithm) != ERROR_OK)
	{
		LOG_WARNING("no working area available, can't do block memory writes");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	retval = target_write_buffer(target, write_algorithm->address,
								 sizeof(flash_write_code), flash_write_code);
	if (retval != ERROR_OK)
	{
		target_free_working_area(target, write_algorithm);
		return retval;
	}

	/* memory buffer */
	while (target_alloc_working_area_try(target, buffer_size, &source) != ERROR_OK)
	{
		buffer_size /= 2;
		buffer_size &= ~3UL; /* Make sure it's 4 byte aligned */
		if (buffer_size <= 128)
		{
			/* we already allocated the writing code, but failed to get a
			 * buffer, free the algorithm */
			target_free_working_area(target, write_algorithm);

			LOG_WARNING("no large enough working area available, can't do block memory writes");
			return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		}
	}

	init_reg_param(&reg_params[0], "a0", 32, PARAM_IN_OUT); /* flash offset */
	init_reg_param(&reg_params[1], "a1", 32, PARAM_OUT);	/* buffer address */
	init_reg_param(&reg_params[2], "a2", 32, PARAM_OUT);	/* byte count */

	// LOG_INFO("buffer_size = %d", buffer_size);
	int total = count;
	while (count > 0)
	{
		uint32_t run_bytes = count > buffer_size ? buffer_size : count;

		/* Write data to fifo */
		retval = target_write_buffer(target, source->address, run_bytes, buffer);
		if (retval != ERROR_OK)
			break;

		buf_set_u32(reg_params[0].value, 0, 32, offset);
		buf_set_u32(reg_params[1].value, 0, 32, source->address);
		buf_set_u32(reg_params[2].value, 0, 32, run_bytes);

		retval = target_run_algorithm(target, 0, NULL, 3, reg_params,
									  write_algorithm->address, write_algorithm->address + 2,
									  100000, NULL);

		if (retval != ERROR_OK)
		{
			LOG_ERROR("Failed to execute algorithm at 0x%" TARGET_PRIxADDR ": %d",
					  write_algorithm->address, retval);
			break;
		}

		retval = buf_get_u32(reg_params[0].value, 0, 32);
		if (retval != 1)
		{
			LOG_ERROR("flash write failed, retval=%x", (uint32_t)retval);
			retval = ERROR_FLASH_OPERATION_FAILED;
			break;
		}
		else
		{
			retval = ERROR_OK;
		}

		/* Update counters and wrap write pointer */
		buffer += run_bytes;
		offset += run_bytes;
		count -= run_bytes;
		int percentage = (total - count) * 100 / total;
		LOG_INFO(" ... %d%%", percentage);
	}

	if (retval == ERROR_OK)
		LOG_INFO(" done ...");
	target_free_working_area(target, source);
	target_free_working_area(target, write_algorithm);

	destroy_reg_param(&reg_params[0]);
	destroy_reg_param(&reg_params[1]);
	destroy_reg_param(&reg_params[2]);

	return retval;
}

FLASH_BANK_COMMAND_HANDLER(phnx05_flash_bank_command)
{

	if (bank->base != FLASH_BASE && bank->base != NVR_BASE && bank->base != EEPROM_BASE)
	{
		LOG_ERROR("Address " TARGET_ADDR_FMT PRIx32
				  " invalid bank address (try " TARGET_ADDR_FMT PRIx32 "/" TARGET_ADDR_FMT PRIx32 "/" TARGET_ADDR_FMT PRIx32
				  "[phoenix series] )",
				  (target_addr_t)bank->base, (target_addr_t)FLASH_BASE, (target_addr_t)NVR_BASE, (target_addr_t)EEPROM_BASE);
		return ERROR_FAIL;
	}

	struct phnx_info *chip;
	chip = calloc(1, sizeof(*chip));

	if (!chip)
	{
		LOG_ERROR("No memory for flash bank chip info");
		return ERROR_FAIL;
	}
	chip->target = bank->target;
	chip->probed = false;

	bank->driver_priv = chip;
	return ERROR_OK;
}

COMMAND_HANDLER(phnx05_handle_info_command)
{
	struct flash_bank *bank;
	LOG_INFO("phnx05_handle_info_command involked.");
	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;
	int bankid = atoi(CMD_ARGV[0]);
	int retval;
	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, bankid, &bank);
	if (retval != ERROR_OK)
		return retval;

	struct phnx_info *chip = (struct phnx_info *)bank->driver_priv;
	command_print(CMD, "bank %d [%s]: " TARGET_ADDR_FMT PRIx32 ", size=%u, pagesize=%u, npages=%d, %s",
				  bankid, bank->name, bank->base, bank->size,
				  chip->page_size, chip->num_pages, chip->probed ? "probed" : "notprobed");
	return ERROR_OK;
}

static const struct command_registration phoenix05_exec_command_handlers[] = {
	{
		.name = "info",
		.handler = phnx05_handle_info_command,
		.mode = COMMAND_EXEC,
		.help = "Print information about the current bank",
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE};

static const struct command_registration phoenix05_command_handlers[] = {
	{
		.name = "phoenix05",
		.mode = COMMAND_ANY,
		.help = "phoenix05 flash command group",
		.usage = "",
		.chain = phoenix05_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE};

struct flash_driver phoenix05_flash = {
	.name = "phoenix05",
	.commands = phoenix05_command_handlers,
	.flash_bank_command = phnx05_flash_bank_command,
	.erase = phnx_erase,
	.protect = phnx_protect,
	.write = phnx_batch_write,
	.read = default_flash_read,
	.probe = phnx_probe,
	.auto_probe = phnx_probe,
	.erase_check = default_flash_blank_check,
	.free_driver_priv = default_flash_free_driver_priv,
};
