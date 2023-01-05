/***************************************************************************
 *   WCH RISC-V mcu :CH32V103X CH32V20X CH32V30X CH56X CH57X CH58X         *
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
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
extern int wlink_erase(void);
extern unsigned char riscvchip;
extern void wlink_reset();
extern void wlink_chip_reset(void);
extern void wlink_getromram(uint32_t *rom, uint32_t *ram);
extern int wlink_write(const uint8_t *buffer, uint32_t offset, uint32_t count);
extern bool noloadflag;
extern int wlink_flash_protect(bool stat);
extern int wlnik_protect_check(void);
extern unsigned long wlink_address;
extern bool pageerase;
struct ch32vx_options
{
	uint8_t rdp;
	uint8_t user;
	uint16_t data;
	uint32_t protection;
};

struct ch32vx_flash_bank
{
	struct ch32vx_options option_bytes;
	int ppage_size;
	int probed;

	bool has_dual_banks;
	bool can_load_options;
	uint32_t register_base;
	uint8_t default_rdp;
	int user_data_offset;
	int option_offset;
	uint32_t user_bank_size;
};

FLASH_BANK_COMMAND_HANDLER(ch32vx_flash_bank_command)
{
	struct ch32vx_flash_bank *ch32vx_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	ch32vx_info = malloc(sizeof(struct ch32vx_flash_bank));

	bank->driver_priv = ch32vx_info;
	ch32vx_info->probed = 0;
	ch32vx_info->has_dual_banks = false;
	ch32vx_info->can_load_options = false;
	ch32vx_info->user_bank_size = bank->size;

	return ERROR_OK;
}
static int ch32x_protect(struct flash_bank *bank, int set, int first, int last)
{

	if ((riscvchip == 1) || (riscvchip == 5) || (riscvchip == 6) || (riscvchip == 9))
	{
		int retval = wlink_flash_protect(set);
		if (retval == ERROR_OK)
		{
			if (set)
				LOG_INFO("Success to Enable Read-Protect");
			else
				LOG_INFO("Success to Disable Read-Protect");
			return ERROR_OK;
		}
		else
		{
			LOG_ERROR("Operation Failed");
			return ERROR_FAIL;
		}
	}
	else
	{
		LOG_ERROR("This chip do not support function");
		return ERROR_FAIL;
	}
}

static int ch32vx_erase(struct flash_bank *bank, int first, int last)
{
	if (pageerase)
		return ERROR_OK;
	if ((riscvchip == 5) || (riscvchip == 6) || (riscvchip == 9))
	{
		int retval = wlnik_protect_check();
		if (retval == 4)
		{
			LOG_ERROR("Read-Protect Status Currently Enabled");
			return ERROR_FAIL;
		}
	}
	if (noloadflag)
		return ERROR_OK;

	int ret = wlink_erase();
	target_halt(bank->target);
	if (ret)
		return ERROR_OK;
	else
		return ERROR_FAIL;
	return ERROR_OK;
}

static int ch32vx_write(struct flash_bank *bank, const uint8_t *buffer,
						uint32_t offset, uint32_t count)
{

	if ((riscvchip == 5) || (riscvchip == 6) || (riscvchip == 9))
	{
		int retval = wlnik_protect_check();
		if (retval == 4)
		{
			LOG_ERROR("Read-Protect Status Currently Enabled");
			return ERROR_FAIL;
		}
	}
	if (noloadflag)
		return ERROR_OK;
	int ret = 0;
	int mod = offset % 256;
	if (mod)
	{
		if (offset < 256)
			offset = 0;
		else
			offset -= mod;
		uint8_t *buffer1;
		uint8_t *buffer2;
		buffer1 = malloc(count + mod);
		buffer2 = malloc(mod);
		target_read_memory(bank->target, offset, 1, mod, buffer2);
		memcpy(buffer1, buffer2, mod);
		memcpy(&buffer1[mod], buffer, count);
		ret = wlink_write(buffer1, offset, count + mod);
	}
	else
	{
		ret = wlink_write(buffer, offset, count);
	}
	wlink_chip_reset();
	return ret;
}

static int ch32vx_get_device_id(struct flash_bank *bank, uint32_t *device_id)
{
	if ((riscvchip != 0x02) && (riscvchip != 0x03)&& (riscvchip != 0x07))
	{	
		struct target *target = bank->target;
		int retval = target_read_u32(target, 0x1ffff7e8, device_id);
		if (retval != ERROR_OK)
			return retval;
	}
	return ERROR_OK;
}

static int ch32vx_get_flash_size(struct flash_bank *bank, uint16_t *flash_size_in_kb)
{

	struct target *target = bank->target;
	if ((riscvchip == 0x02) || (riscvchip == 0x03) || (riscvchip == 0x07))
	{
		*flash_size_in_kb = 448;
		return ERROR_OK;
	}
	int retval = target_read_u16(target, 0x1ffff7e0, flash_size_in_kb);
	if (retval != ERROR_OK)
		return retval;
	return ERROR_OK;
}

static int ch32vx_probe(struct flash_bank *bank)
{
	struct ch32vx_flash_bank *ch32vx_info = bank->driver_priv;
	uint16_t delfault_max_flash_size = 512;
	uint16_t flash_size_in_kb;
	uint32_t device_id = 0;
	uint32_t rom = 0;
	uint32_t ram = 0;
	int page_size;
	uint32_t base_address = (uint32_t)wlink_address;
	uint32_t rid = 0;
	ch32vx_info->probed = 0;

	/* read ch32 device id register */
	int retval = ch32vx_get_device_id(bank, &device_id);
	if (retval != ERROR_OK)
		return retval;
	if (device_id)
		LOG_INFO("device id = 0x%08" PRIx32 "", device_id);
	page_size = 1024;
	ch32vx_info->ppage_size = 4;

	/* get flash size from target. */
	retval = ch32vx_get_flash_size(bank, &flash_size_in_kb);

	if (flash_size_in_kb)
		LOG_INFO("flash size = %dkbytes", flash_size_in_kb);
	else
		flash_size_in_kb = delfault_max_flash_size;
	if ((riscvchip == 0x05) || (riscvchip == 0x06))
	{
		wlink_getromram(&rom, &ram);
		if ((rom != 0) && (ram != 0))
			LOG_INFO("ROM %d kbytes RAM %d kbytes", rom, ram);
	}
	// /* calculate numbers of pages */
	int num_pages = flash_size_in_kb * 1024 / page_size;
	bank->base = base_address;
	bank->size = (num_pages * page_size);
	bank->num_sectors = num_pages;
	bank->sectors = alloc_block_array(0, page_size, num_pages);
	ch32vx_info->probed = 1;

	return ERROR_OK;
}

static int ch32vx_auto_probe(struct flash_bank *bank)
{

	struct ch32vx_flash_bank *ch32vx_info = bank->driver_priv;
	if (ch32vx_info->probed)
		return ERROR_OK;
	return ch32vx_probe(bank);
}

static const struct command_registration ch32vx_command_handlers[] = {
	{
		.name = "wch_riscv",
		.mode = COMMAND_ANY,
		.help = "wch_riscv flash command group",
		.usage = "",

	},
	COMMAND_REGISTRATION_DONE};

const struct flash_driver wch_riscv_flash = {
	.name = "wch_riscv",
	.commands = ch32vx_command_handlers,
	.flash_bank_command = ch32vx_flash_bank_command,
	.erase = ch32vx_erase,
	.protect = ch32x_protect,
	.write = ch32vx_write,
	.read = default_flash_read,
	.probe = ch32vx_probe,
	.auto_probe = ch32vx_auto_probe,
	.erase_check = default_flash_blank_check,
	.free_driver_priv = default_flash_free_driver_priv,
};
