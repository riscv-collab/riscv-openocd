// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2007, 2008 by Ben Dooks                                 *
 *   ben@fluff.org                                                         *
 ***************************************************************************/

/*
 * S3C2412 OpenOCD NAND Flash controller support.
 *
 * Many thanks to Simtec Electronics for sponsoring this work.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "s3c24xx.h"

NAND_DEVICE_COMMAND_HANDLER(s3c2412_nand_device_command)
{
	struct s3c24xx_nand_controller *info;
	CALL_S3C24XX_DEVICE_COMMAND(nand, &info);

	/* fill in the address fields for the core device */
	info->cmd = S3C2440_NFCMD;
	info->addr = S3C2440_NFADDR;
	info->data = S3C2440_NFDATA;
	info->nfstat = S3C2412_NFSTAT;

	return ERROR_OK;
}

static int s3c2412_init(struct nand_device *nand)
{
	struct target *target = nand->target;

	target_write_u32(target, S3C2410_NFCONF,
			 S3C2440_NFCONF_TACLS(3) |
			 S3C2440_NFCONF_TWRPH0(7) |
			 S3C2440_NFCONF_TWRPH1(7));

	target_write_u32(target, S3C2440_NFCONT,
			 S3C2412_NFCONT_INIT_MAIN_ECC |
			 S3C2440_NFCONT_ENABLE);

	return ERROR_OK;
}

struct nand_flash_controller s3c2412_nand_controller = {
	.name = "s3c2412",
	.nand_device_command = &s3c2412_nand_device_command,
	.init = &s3c2412_init,
	.reset = &s3c24xx_reset,
	.command = &s3c24xx_command,
	.address = &s3c24xx_address,
	.write_data = &s3c24xx_write_data,
	.read_data = &s3c24xx_read_data,
	.write_page = s3c24xx_write_page,
	.read_page = s3c24xx_read_page,
	.write_block_data = &s3c2440_write_block_data,
	.read_block_data = &s3c2440_read_block_data,
	.nand_ready = &s3c2440_nand_ready,
};
