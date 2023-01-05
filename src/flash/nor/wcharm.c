/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   Copyright (C) 2011 by Andreas Fritiofson                              *
 *   andreas.fritiofson@gmail.com                                          *
 *
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
#include <target/armv7m.h>

/* ch32x register locations */

#define FLASH_REG_BASE_B0 0x40022000
#define FLASH_REG_BASE_B1 0x40022040

#define ch32_FLASH_ACR     0x00
#define ch32_FLASH_KEYR    0x04
#define ch32_FLASH_OPTKEYR 0x08
#define ch32_FLASH_SR      0x0C
#define ch32_FLASH_CR      0x10
#define ch32_FLASH_AR      0x14
#define ch32_FLASH_OBR     0x1C
#define ch32_FLASH_WRPR    0x20
#define CH32_FLASH_MODEKEYP 0x24 //chf103�����Ĵ���

/* TODO: Check if code using these really should be hard coded to bank 0.
 * There are valid cases, on dual flash devices the protection of the
 * second bank is done on the bank0 reg's. */
#define ch32_FLASH_ACR_B0     0x40022000
#define ch32_FLASH_KEYR_B0    0x40022004
#define ch32_FLASH_OPTKEYR_B0 0x40022008
#define ch32_FLASH_SR_B0      0x4002200C
#define ch32_FLASH_CR_B0      0x40022010
#define ch32_FLASH_AR_B0      0x40022014
#define ch32_FLASH_OBR_B0     0x4002201C
#define ch32_FLASH_WRPR_B0    0x40022020

/* option byte location */

#define ch32_OB_RDP		0x1FFFF800
#define ch32_OB_USER		0x1FFFF802
#define ch32_OB_DATA0		0x1FFFF804
#define ch32_OB_DATA1		0x1FFFF806
#define ch32_OB_WRP0		0x1FFFF808
#define ch32_OB_WRP1		0x1FFFF80A
#define ch32_OB_WRP2		0x1FFFF80C
#define ch32_OB_WRP3		0x1FFFF80E

/* FLASH_CR register bits */

#define FLASH_PG			(1 << 0)
#define FLASH_PER			(1 << 1)
#define FLASH_MER			(1 << 2)
#define FLASH_OPTPG			(1 << 4)
#define FLASH_OPTER			(1 << 5)
#define FLASH_STRT			(1 << 6)
#define FLASH_LOCK			(1 << 7)
#define FLASH_OPTWRE		(1 << 9)
#define FLASH_OBL_LAUNCH	(1 << 13)	/* except ch32f1x series */

/* CHf��Flash_CR�Ĵ���������λ */
#define FLASH_PAGE_PROGRAM	  0x00010000	//ҳ��̣�128Byteһҳ��
#define FLASH_PAGE_ERASE		  0x00020000	//ҳ������128Byteһҳ��
#define FLASH_STD_PAGE_ERASE  0x00000002  //��׼ҳ������1024Byte һҳ��
#define FLASH_STD_PAGE_PRG    0x00000001  //��׼ҳ��̣�1024Byte һҳ��
#define FLASH_BUF_LOAD			  0x00040000	//�������ݵ�FLASH�ڲ�������
#define FLASH_BUF_RTS				  0x00080000	//FLASH�ڲ���������λ



/* FLASH_SR register bits */

#define FLASH_BSY		(1 << 0)
#define FLASH_PGERR		(1 << 2)
#define FLASH_WRPRTERR	(1 << 4)
#define FLASH_EOP		(1 << 5)

/* ch32_FLASH_OBR bit definitions (reading) */

#define OPT_ERROR		0
#define OPT_READOUT		1
#define OPT_RDWDGSW		2
#define OPT_RDRSTSTOP	3
#define OPT_RDRSTSTDBY	4
#define OPT_BFB2		5	/* dual flash bank only */

/* register unlock keys */

#define KEY1			0x45670123
#define KEY2			0xCDEF89AB

/* timeout values */

#define FLASH_WRITE_TIMEOUT 10
#define FLASH_ERASE_TIMEOUT 1000

int wch_arm_chip;
static const uint8_t ch32f2_flash_write_code[] = {   
     0x80, 0xB4, 0x89, 0xB0, 0x00, 0xAF, 0x78, 0x60, 0x39, 0x60, 0x7B, 0x68, 0xBB, 0x61, 0x35, 0x4B, 
    0xFB, 0x61, 0x3B, 0x68, 0xFF, 0x33, 0x1B, 0x0A, 0x3B, 0x61, 0x33, 0x4B, 0x1B, 0x69, 0x32, 0x4A, 
    0x43, 0xF4, 0x80, 0x33, 0x13, 0x61, 0x30, 0x4B, 0x1B, 0x69, 0x2F, 0x4A, 0x43, 0xF4, 0x80, 0x33, 
    0x13, 0x61, 0x00, 0xBF, 0x2C, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x01, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 
    0xBB, 0x69, 0x7B, 0x61, 0x40, 0x23, 0xFB, 0x60, 0x12, 0xE0, 0xFB, 0x69, 0x1A, 0x1D, 0xFA, 0x61, 
    0x7A, 0x69, 0x1B, 0x68, 0x13, 0x60, 0x7B, 0x69, 0x04, 0x33, 0x7B, 0x61, 0xFB, 0x68, 0x01, 0x3B, 
    0xFB, 0x60, 0x00, 0xBF, 0x20, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x02, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 
    0xFB, 0x68, 0x00, 0x2B, 0xE9, 0xD1, 0x1C, 0x4B, 0x1B, 0x69, 0x1B, 0x4A, 0x43, 0xF4, 0x00, 0x13, 
    0x13, 0x61, 0x00, 0xBF, 0x18, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x01, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 
    0x15, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x10, 0x03, 0x00, 0x2B, 0x0D, 0xD0, 0x12, 0x4B, 0xDB, 0x68, 
    0x11, 0x4A, 0x43, 0xF0, 0x10, 0x03, 0xD3, 0x60, 0x0F, 0x4B, 0x1B, 0x69, 0x0E, 0x4A, 0x23, 0xF4, 
    0x80, 0x33, 0x13, 0x61, 0x08, 0x23, 0x0F, 0xE0, 0xBB, 0x69, 0x03, 0xF5, 0x80, 0x73, 0xBB, 0x61, 
    0x3B, 0x69, 0x01, 0x3B, 0x3B, 0x61, 0x00, 0x2B, 0xAD, 0xD1, 0x07, 0x4B, 0x1B, 0x69, 0x06, 0x4A, 
    0x23, 0xF4, 0x80, 0x33, 0x13, 0x61, 0x00, 0x23, 0x18, 0x46, 0x24, 0x37, 0xBD, 0x46, 0x80, 0xBC, 
    0x00, 0xBE, 0x00, 0xBF, 0x00, 0x10, 0x00, 0x20, 0x00, 0x20, 0x02, 0x40,
};
static const uint8_t ch32f1_flash_write_code[]= 
{
    0x80, 0xB4, 0x89, 0xB0, 0x00, 0xAF, 0x78, 0x60, 0x39, 0x60, 0x7B, 0x68, 0xBB, 0x61, 0x42, 0x4B, 
    0xFB, 0x61, 0x3B, 0x68, 0x7F, 0x33, 0xDB, 0x09, 0x3B, 0x61, 0x40, 0x4B, 0x1B, 0x69, 0x3F, 0x4A, 
    0x43, 0xF4, 0x80, 0x33, 0x13, 0x61, 0x3D, 0x4B, 0x1B, 0x69, 0x3C, 0x4A, 0x43, 0xF4, 0x00, 0x23, 
    0x13, 0x61, 0x00, 0xBF, 0x39, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x01, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 
    0xBB, 0x69, 0x7B, 0x61, 0x08, 0x23, 0xFB, 0x60, 0xFB, 0x69, 0x1A, 0x1D, 0xFA, 0x61, 0x7A, 0x69, 
    0x1B, 0x68, 0x13, 0x60, 0xFB, 0x69, 0x1A, 0x1D, 0xFA, 0x61, 0x7A, 0x69, 0x04, 0x32, 0x1B, 0x68, 
    0x13, 0x60, 0xFB, 0x69, 0x1A, 0x1D, 0xFA, 0x61, 0x7A, 0x69, 0x08, 0x32, 0x1B, 0x68, 0x13, 0x60, 
    0xFB, 0x69, 0x1A, 0x1D, 0xFA, 0x61, 0x7A, 0x69, 0x0C, 0x32, 0x1B, 0x68, 0x13, 0x60, 0x27, 0x4B, 
    0x1B, 0x69, 0x26, 0x4A, 0x43, 0xF4, 0x80, 0x23, 0x13, 0x61, 0x00, 0xBF, 0x23, 0x4B, 0xDB, 0x68, 
    0x03, 0xF0, 0x01, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 0x7B, 0x69, 0x10, 0x33, 0x7B, 0x61, 0xFB, 0x68, 
    0x01, 0x3B, 0xFB, 0x60, 0x00, 0x2B, 0xCF, 0xD1, 0x1C, 0x4A, 0xBB, 0x69, 0x53, 0x61, 0x1B, 0x4B, 
    0x1B, 0x69, 0x1A, 0x4A, 0x43, 0xF0, 0x40, 0x03, 0x13, 0x61, 0x00, 0xBF, 0x17, 0x4B, 0xDB, 0x68, 
    0x03, 0xF0, 0x01, 0x03, 0x00, 0x2B, 0xF9, 0xD1, 0x14, 0x4B, 0xDB, 0x68, 0x03, 0xF0, 0x14, 0x03, 
    0x00, 0x2B, 0x0D, 0xD0, 0x11, 0x4B, 0xDB, 0x68, 0x10, 0x4A, 0x43, 0xF0, 0x14, 0x03, 0xD3, 0x60, 
    0x0E, 0x4B, 0x1B, 0x69, 0x0D, 0x4A, 0x23, 0xF4, 0x50, 0x23, 0x13, 0x61, 0x08, 0x23, 0x0E, 0xE0, 
    0xBB, 0x69, 0x80, 0x33, 0xBB, 0x61, 0x3B, 0x69, 0x01, 0x3B, 0x3B, 0x61, 0x00, 0x2B, 0x92, 0xD1, 
    0x06, 0x4B, 0x1B, 0x69, 0x05, 0x4A, 0x23, 0xF4, 0x50, 0x23, 0x13, 0x61, 0x00, 0x23, 0x18, 0x46, 
    0x24, 0x37, 0xBD, 0x46, 0x80, 0xBC, 0x00, 0xBE, 0x00, 0x10, 0x00, 0x20, 0x00, 0x20, 0x02, 0x40, 
    
};

struct ch32x_options {
	uint8_t rdp;
	uint8_t user;
	uint16_t data;
	uint32_t protection;
};

struct ch32x_flash_bank {
	struct ch32x_options option_bytes;
	int ppage_size;
	int probed;

	bool has_dual_banks;
	/* used to access dual flash bank ch32xl */
	bool can_load_options;
	uint32_t register_base;
	uint8_t default_rdp;
	int user_data_offset;
	int option_offset;
	uint32_t user_bank_size;
};




static int ch32x_mass_erase(struct flash_bank *bank);
static int ch32x_get_device_id(struct flash_bank *bank, uint32_t *device_id);
static int ch32x_write_block(struct flash_bank *bank, const uint8_t *buffer,
		uint32_t address, uint32_t count);

/* flash bank ch32x <base> <size> 0 0 <target#>
 */
FLASH_BANK_COMMAND_HANDLER(ch32x_flash_bank_command)
{
	struct ch32x_flash_bank *ch32x_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	ch32x_info = malloc(sizeof(struct ch32x_flash_bank));

	bank->driver_priv = ch32x_info;
	ch32x_info->probed = 0;
	ch32x_info->has_dual_banks = false;
	ch32x_info->can_load_options = false;
	ch32x_info->register_base = FLASH_REG_BASE_B0;
	ch32x_info->user_bank_size = bank->size;

	return ERROR_OK;
}
static int get_ch32x_info(struct flash_bank *bank, char *buf, int buf_size)
{
	return ERROR_OK;
}
static inline int ch32x_get_flash_reg(struct flash_bank *bank, uint32_t reg)
{
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	return reg + ch32x_info->register_base;
}

static inline int ch32x_get_flash_status(struct flash_bank *bank, uint32_t *status)
{
	struct target *target = bank->target;
	return target_read_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_SR), status);
}

static int ch32x_wait_status_busy(struct flash_bank *bank, int timeout)
{
	struct target *target = bank->target;
	uint32_t status;
	int retval = ERROR_OK;

	/* wait for busy to clear */
	for (;;) {
		retval = ch32x_get_flash_status(bank, &status);
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("status: 0x%" PRIx32 "", status);
		if ((status & FLASH_BSY) == 0)
			break;
		if (timeout-- <= 0) {
			LOG_ERROR("timed out waiting for flash");
			return ERROR_FAIL;
		}
		alive_sleep(1);
	}

	if (status & FLASH_WRPRTERR) {
		LOG_ERROR("ch32x device protected");
		retval = ERROR_FAIL;
	}

	if (status & FLASH_PGERR) {
		LOG_ERROR("ch32x device programming failed");
		retval = ERROR_FAIL;
	}

	/* Clear but report errors */
	if (status & (FLASH_WRPRTERR | FLASH_PGERR)) {
		/* If this operation fails, we ignore it and report the original
		 * retval
		 */
		target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_SR),
				FLASH_WRPRTERR | FLASH_PGERR);
	}
	return retval;
}

static int ch32x_check_operation_supported(struct flash_bank *bank)
{
	
	
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;

	/* if we have a dual flash bank device then
	 * we need to perform option byte stuff on bank0 only */
	if (ch32x_info->register_base != FLASH_REG_BASE_B0) {
		LOG_ERROR("Option Byte Operation's must use bank0");
		return ERROR_FLASH_OPERATION_FAILED;
	}

	return ERROR_OK;
}

static int ch32x_read_options(struct flash_bank *bank)
{

	
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	struct target *target = bank->target;
	uint32_t option_bytes;
	int retval;

	/* read user and read protection option bytes */
	retval = target_read_u32(target, ch32_OB_RDP, &option_bytes);
	if (retval != ERROR_OK)
		return retval;

	ch32x_info->option_bytes.rdp = option_bytes & 0xFF;
	ch32x_info->option_bytes.user = (option_bytes >> 16) & 0xFF;

	/* read user data option bytes */
	retval = target_read_u32(target, ch32_OB_DATA0, &option_bytes);
	if (retval != ERROR_OK)
		return retval;

	ch32x_info->option_bytes.data = ((option_bytes >> 8) & 0xFF00) | (option_bytes & 0xFF);

	/* read write protection option bytes */
	retval = target_read_u32(target, ch32_OB_WRP0, &option_bytes);
	if (retval != ERROR_OK)
		return retval;

	ch32x_info->option_bytes.protection = ((option_bytes >> 8) & 0xFF00) | (option_bytes & 0xFF);

	retval = target_read_u32(target, ch32_OB_WRP2, &option_bytes);
	if (retval != ERROR_OK)
		return retval;

	ch32x_info->option_bytes.protection |= (((option_bytes >> 8) & 0xFF00) | (option_bytes & 0xFF)) << 16;

	return ERROR_OK;
}

static int ch32x_erase_options(struct flash_bank *bank)
{
	
	uint32_t option_bytes;
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	struct target *target = bank->target;

	/* read current options */
	ch32x_read_options(bank);

	/* unlock flash registers */
	int retval = target_write_u32(target, ch32_FLASH_KEYR_B0, KEY1);
	if (retval != ERROR_OK)
		return retval;

	retval = target_write_u32(target, ch32_FLASH_KEYR_B0, KEY2);
	if (retval != ERROR_OK)
		return retval;

	/* unlock option flash registers */
	retval = target_write_u32(target, ch32_FLASH_OPTKEYR_B0, KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_OPTKEYR_B0, KEY2);
	if (retval != ERROR_OK)
		return retval;

	/* erase option bytes */
	retval = target_write_u32(target, ch32_FLASH_CR_B0, FLASH_OPTER);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_OB_RDP,((ch32x_info->option_bytes.rdp & 0xffff0000)| ch32x_info->default_rdp) );
	if (retval != ERROR_OK)
		return retval;
	
	ch32x_info->option_bytes.rdp = ch32x_info->default_rdp;

	retval = ch32x_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;
	for(int i=0;i<8;i++){
		retval = target_write_u16(target, ch32_OB_RDP+16*i,0xffff );
			if (retval != ERROR_OK)
				return retval;
	}
	retval = target_read_u32(target, ch32_FLASH_CR_B0, &option_bytes);
	if (retval != ERROR_OK)
		return retval;
	option_bytes &=0xffffffef;

	retval = target_write_u32(target, ch32_FLASH_CR_B0, option_bytes);
	if (retval != ERROR_OK)
		return retval;
	return ERROR_OK;
}

static int ch32x_write_options(struct flash_bank *bank)
{
	

	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	struct target *target = bank->target;
	uint16_t pbuf[8];
	uint32_t option_bytes;
	ch32x_info = bank->driver_priv;

	/* unlock flash registers */
	int retval = target_write_u32(target, ch32_FLASH_KEYR_B0, KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_KEYR_B0, KEY2);
	if (retval != ERROR_OK)
		return retval;

	/* unlock option flash registers */
	retval = target_write_u32(target, ch32_FLASH_OPTKEYR_B0, KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_OPTKEYR_B0, KEY2);
	if (retval != ERROR_OK)
		return retval;

	/* program option bytes */
	for(int i=0;i<8;i++){
		retval = target_read_u16(target, ch32_OB_RDP+ 16*i , pbuf[i]);
		if (retval != ERROR_OK)
			return retval;
	}
	if(ch32x_info->option_bytes.protection)
		pbuf[0]=0x5aa5;
	else
		pbuf[0]=0x00ff;
	
	retval = target_write_u32(target, ch32_FLASH_CR_B0, FLASH_OPTER);
	if (retval != ERROR_OK)
		return retval;
	for(int i=0;i<8;i++){
		retval = target_write_u16(target, ch32_OB_RDP+16*i,pbuf[i]);
			if (retval != ERROR_OK)
				return retval;
	}
	retval = target_read_u32(target, ch32_FLASH_CR_B0, &option_bytes);
	if (retval != ERROR_OK)
		return retval;
	option_bytes &=0xffffffef;

	retval = target_write_u32(target, ch32_FLASH_CR_B0, option_bytes);
	if (retval != ERROR_OK)
		return retval;
	return ERROR_OK;
}

static int ch32x_protect_check(struct flash_bank *bank)
{
	
	
	struct target *target = bank->target;
	uint32_t protection;

	int retval = ch32x_check_operation_supported(bank);
	if (ERROR_OK != retval)
		return retval;

	/* medium density - each bit refers to a 4 sector protection block
	 * high density - each bit refers to a 2 sector protection block
	 * bit 31 refers to all remaining sectors in a bank */
	retval = target_read_u32(target, ch32_FLASH_WRPR_B0, &protection);
	if (retval != ERROR_OK)
		return retval;

	for (int i = 0; i < bank->num_prot_blocks; i++)
		bank->prot_blocks[i].is_protected = (protection & (1 << i)) ? 0 : 1;

	return ERROR_OK;
}

static int ch32x_erase(struct flash_bank *bank, int first, int last)
{
	
	
	struct target *target = bank->target;
	int i;
    uint32_t cr_reg; uint32_t sr_reg;
	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* unlock flash registers */

	 int retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY2);
	if (retval != ERROR_OK)
		return retval;
	

	 retval = target_read_u32(target, ch32_FLASH_CR_B0, &cr_reg);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_CR_B0, cr_reg | FLASH_MER);
	if (retval != ERROR_OK)
		return retval;
	
	 retval = target_read_u32(target, ch32_FLASH_CR_B0, &cr_reg);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_CR_B0, cr_reg | FLASH_STRT);
	if (retval != ERROR_OK)
		return retval;


	retval = ch32x_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;


	retval = target_read_u32(target, ch32_FLASH_CR_B0, &cr_reg);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32_FLASH_CR_B0, (cr_reg)&(~(1<<2)));
	if (retval != ERROR_OK)
		return retval;
	alive_sleep(300);

	return ERROR_OK;
}

static int ch32x_protect(struct flash_bank *bank, int set, int first, int last)
{
	
	struct target *target = bank->target;
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	int retval = ch32x_check_operation_supported(bank);
	if (retval != ERROR_OK)
		return retval;

	retval = ch32x_erase_options(bank);
	if (retval != ERROR_OK) {
		LOG_ERROR("ch32x failed to erase options");
		return retval;
	}

	for (int i = first; i <= last; i++) {
		if (set)
			ch32x_info->option_bytes.protection &= ~(1 << i);
		else
			ch32x_info->option_bytes.protection |= (1 << i);
	}

	return ch32x_write_options(bank);
}

static int ch32x_write_block(struct flash_bank *bank, const uint8_t *buffer,
		uint32_t address, uint32_t count)
{
	
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	struct target *target = bank->target;
	uint32_t buffer_size = 16384;
	struct working_area *write_algorithm;
	struct working_area *source;
	struct reg_param reg_params[4];
	struct armv7m_algorithm armv7m_info;
	uint32_t basaddr=0x08000000;
	uint32_t pagesize=0x100;
	uint32_t sp=0x20002800;
	int retval = ERROR_OK;
	uint8_t ch32x_flash_write_code[320]={0};
	uint8_t bufpage[256];
	if(wch_arm_chip==1)	
		memcpy(ch32x_flash_write_code,ch32f1_flash_write_code,sizeof(ch32f1_flash_write_code));
	else
		memcpy(ch32x_flash_write_code,ch32f2_flash_write_code,sizeof(ch32f2_flash_write_code));
			
	/* flash write code */
	if (target_alloc_working_area(target, sizeof(ch32x_flash_write_code),
			&write_algorithm) != ERROR_OK) {
		LOG_WARNING("no working area available, can't do block memory writes");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}
	
	retval = target_write_buffer(target, write_algorithm->address,
			sizeof(ch32x_flash_write_code) , ch32x_flash_write_code);
	LOG_INFO("write_algorithm->address%x",write_algorithm->address);
	if (retval != ERROR_OK) {
		target_free_working_area(target, write_algorithm);
		return retval;
	}

	init_reg_param(&reg_params[0], "r0", 32, PARAM_OUT);	/* flash base (in), status (out) */
	init_reg_param(&reg_params[1], "r1", 32, PARAM_OUT);	/* count (halfword-16bit) */
	init_reg_param(&reg_params[2], "sp", 32, PARAM_OUT);	/* buffer start */

	buf_set_u32(reg_params[0].value, 0, 32, basaddr);
	buf_set_u32(reg_params[1].value, 0, 32, pagesize  );
	buf_set_u32(reg_params[2].value, 0, 32, sp);
	armv7m_info.common_magic = ARMV7M_COMMON_MAGIC;
	armv7m_info.core_mode = ARM_MODE_THREAD;
	int mod =address%256;	
	int len=count+mod;
	uint8_t *buffer1;	
	uint8_t *buffer2;
	int loop=0;
	if(mod){
		if(address<256)
			address=0;
		else
			address-=mod;
			
		buffer1=malloc(len);
		buffer2=malloc(mod);
	
		target_read_memory(bank->target,address,1,mod,buffer2);
		memcpy(buffer1,buffer2,mod);
		memcpy(&buffer1[mod],buffer,count);
	}else{
		buffer1=malloc(len);
		memcpy(buffer1,buffer,count);
	}
	while(len>0){
			buf_set_u32(reg_params[0].value, 0, 32, basaddr);
			if(len<256){
				memset(bufpage,0xff,256);
				memcpy(bufpage,buffer1+loop*256,len);
			}else
				memcpy(bufpage,buffer1+loop*256,256);
			target_write_buffer(target, 0x20001000,256,bufpage);
		    retval=target_run_algorithm(target,0,NULL,3,reg_params,0x20000000,0,100000,&armv7m_info);
			len-=256;
			loop++;
			basaddr +=256;

	}
	if (retval == ERROR_FLASH_OPERATION_FAILED) {
		LOG_ERROR("flash write failed at address 0x%"PRIx32,
				buf_get_u32(reg_params[4].value, 0, 32));

		if (buf_get_u32(reg_params[0].value, 0, 32) & FLASH_PGERR) {
			LOG_ERROR("flash memory not erased before writing");
			/* Clear but report errors */
			target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_SR), FLASH_PGERR);
		}

		if (buf_get_u32(reg_params[0].value, 0, 32) & FLASH_WRPRTERR) {
			LOG_ERROR("flash memory write protected");
			/* Clear but report errors */
			target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_SR), FLASH_WRPRTERR);
		}
	}

	target_free_working_area(target, source);
	target_free_working_area(target, write_algorithm);

	destroy_reg_param(&reg_params[0]);
	destroy_reg_param(&reg_params[1]);
	destroy_reg_param(&reg_params[2]);

	return retval;
}

static int ch32x_write(struct flash_bank *bank, const uint8_t *buffer,
		uint32_t offset, uint32_t count)
{

	struct target *target = bank->target;
	uint8_t *new_buffer = NULL;
    uint32_t choffset=offset;
	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (offset & 0x1) {
		LOG_ERROR("offset 0x%" PRIx32 " breaks required 2-byte alignment", offset);
		return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
	}

	int retval;
		
	/* unlock flash registers */
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY2);
	if (retval != ERROR_OK)
		return retval;

	retval=target_write_u32(target,ch32x_get_flash_reg(bank, CH32_FLASH_MODEKEYP) , KEY1);   
	if (retval != ERROR_OK)
		return retval;                  //��������ģʽ
	retval =target_write_u32(target, ch32x_get_flash_reg(bank, CH32_FLASH_MODEKEYP), KEY2); 
	if (retval != ERROR_OK)
		return retval;

	 retval = ch32x_write_block(bank, buffer, bank->base + offset, count);

	return retval;
}

static int ch32x_get_device_id(struct flash_bank *bank, uint32_t *device_id)
{
	
	struct target *target = bank->target;
	int retval = target_read_u32(target, 0x1ffff884, device_id);
	if (retval != ERROR_OK)
		return retval;
	 if(*device_id >>24 ==0x20){
	 		wch_arm_chip=1;
			return ERROR_OK;
	 }
	retval = target_read_u32(target, 0x1ffff704, device_id);
	if (retval != ERROR_OK)
		return retval;
	if((*device_id >>20 ==0x203)||(*device_id >>20 ==0x205)||(*device_id >>20 ==0x207)||(*device_id >>20 ==0x208)){
		    wch_arm_chip=2;
			return ERROR_OK;

	}
	return ERROR_FAIL;

}

static int ch32x_get_flash_size(struct flash_bank *bank, uint16_t *flash_size_in_kb)
{
	struct target *target = bank->target;
	uint32_t cpuid, flash_size_reg;
    uint32_t temp;
	int retval = target_read_u32(target, 0x1ffff7e0, flash_size_in_kb);	
	if (retval != ERROR_OK)
		return retval;

	return retval;
	
}

static int ch32x_probe(struct flash_bank *bank)
{

	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	uint16_t flash_size_in_kb;
	uint16_t max_flash_size_in_kb;
	uint32_t device_id;
	int page_size;
	uint32_t base_address = 0x08000000;
  	uint32_t rid=0;
	ch32x_info->probed = 0;
	ch32x_info->register_base = FLASH_REG_BASE_B0;
	ch32x_info->user_data_offset = 10;
	ch32x_info->option_offset = 0;

	/* default factory read protection level 0 */
	ch32x_info->default_rdp = 0xA5;

	/* read ch32 device id register */
	int retval = ch32x_get_device_id(bank, &device_id);
	if (retval != ERROR_OK)
		return retval;
	
	LOG_INFO("device id = 0x%08" PRIx32 "", device_id);

	page_size = 1024;
	ch32x_info->ppage_size = 4;
	max_flash_size_in_kb = 128;
	
	/* get flash size from target. */
	retval = ch32x_get_flash_size(bank, &flash_size_in_kb);

	/* failed reading flash size or flash size invalid (early silicon),
	 * default to max target family */
	if (retval != ERROR_OK || flash_size_in_kb == 0xffff || flash_size_in_kb == 0) {
		//LOG_WARNING("ch32 flash size failed, probe inaccurate - assuming %dk flash",
			//max_flash_size_in_kb);
		flash_size_in_kb = max_flash_size_in_kb;
	}

	if (ch32x_info->has_dual_banks) {
		/* split reported size into matching bank */
		if (bank->base != 0x08080000) {
			/* bank 0 will be fixed 512k */
			flash_size_in_kb = 512;
		} else {
			flash_size_in_kb -= 512;
			/* bank1 also uses a register offset */
			ch32x_info->register_base = FLASH_REG_BASE_B1;
			base_address = 0x08080000;
		}
	}
	LOG_INFO("flash size = %dkbytes", flash_size_in_kb);

	/* did we assign flash size? */
	assert(flash_size_in_kb != 0xffff);

	/* calculate numbers of pages */
	int num_pages = flash_size_in_kb * 1024 / page_size;

	/* check that calculation result makes sense */
	assert(num_pages > 0);

	if (bank->sectors) {
		free(bank->sectors);
		bank->sectors = NULL;
	}

	if (bank->prot_blocks) {
		free(bank->prot_blocks);
		bank->prot_blocks = NULL;
	}

	bank->base = base_address;
	bank->size = (num_pages * page_size);

	bank->num_sectors = num_pages;
	bank->sectors = alloc_block_array(0, page_size, num_pages);
	if (!bank->sectors)
		return ERROR_FAIL;

	/* calculate number of write protection blocks */
	int num_prot_blocks = num_pages / ch32x_info->ppage_size;
	if (num_prot_blocks > 32)
		num_prot_blocks = 32;

	bank->num_prot_blocks = num_prot_blocks;
	bank->prot_blocks = alloc_block_array(0, ch32x_info->ppage_size * page_size, num_prot_blocks);
	if (!bank->prot_blocks)
		return ERROR_FAIL;

	if (num_prot_blocks == 32)
		bank->prot_blocks[31].size = (num_pages - (31 * ch32x_info->ppage_size)) * page_size;

	ch32x_info->probed = 1;

	return ERROR_OK;
}

static int ch32x_auto_probe(struct flash_bank *bank)
{
	struct ch32x_flash_bank *ch32x_info = bank->driver_priv;
	if (ch32x_info->probed)
		return ERROR_OK;
	return ch32x_probe(bank);
}
static int ch32x_mass_erase(struct flash_bank *bank)
{
	
	struct target *target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* unlock option flash registers */
	int retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY1);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_KEYR), KEY2);
	if (retval != ERROR_OK)
		return retval;

	/* mass erase flash memory */
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_CR), FLASH_MER);
	if (retval != ERROR_OK)
		return retval;
	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_CR),
			FLASH_MER | FLASH_STRT);
	if (retval != ERROR_OK)
		return retval;

	retval = ch32x_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;

	retval = target_write_u32(target, ch32x_get_flash_reg(bank, ch32_FLASH_CR), FLASH_LOCK);
	if (retval != ERROR_OK)
		return retval;

	return ERROR_OK;
}


static const struct command_registration ch32x_command_handlers[] = {
	{
		.name = "ch32f1x",
		.mode = COMMAND_ANY,
		.help = "ch32f1x flash command group",
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE
};

const struct flash_driver wch_arm_flash = {
	.name = "wch_arm",
	.commands = ch32x_command_handlers,
	.flash_bank_command = ch32x_flash_bank_command,
	.erase = ch32x_erase,
	.protect = ch32x_protect,
	.write = ch32x_write,
	.read = default_flash_read,
	.probe = ch32x_probe,
	.auto_probe = ch32x_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = ch32x_protect_check,
	.info = get_ch32x_info,
	.free_driver_priv = default_flash_free_driver_priv,
};
