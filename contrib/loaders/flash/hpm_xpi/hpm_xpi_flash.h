/*
 * Copyright (c) 2021 hpmicro
 *
 * SPDX-License-Identifier: BSD-3-Clause *
 */

#ifndef HPM_XPI_FLASH_H
#define HPM_XPI_FLASH_H

#define FLASH_INIT    (0)
#define FLASH_ERASE   (0x6)
#define FLASH_PROGRAM (0xc)
#define FLASH_READ (0x12)
#define FLASH_GET_INFO (0x18)
#define FLASH_ERASE_CHIP (0x1e)

uint8_t flash_algo[] = {
#include "hpm_xpi_flash.inc"
};

#endif
