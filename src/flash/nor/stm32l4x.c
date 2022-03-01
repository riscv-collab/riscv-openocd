/***************************************************************************
 *   Copyright (C) 2015 by Uwe Bonnes                                      *
 *   bon@elektron.ikp.physik.tu-darmstadt.de                               *
 *                                                                         *
 *   Copyright (C) 2019 by Tarek Bochkati for STMicroelectronics           *
 *   tarek.bouchkati@gmail.com                                             *
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
#include <helper/align.h>
#include <helper/binarybuffer.h>
#include <helper/bits.h>
#include <target/algorithm.h>
#include <target/arm_adi_v5.h>
#include <target/cortex_m.h>
#include "stm32l4x.h"

/* STM32L4xxx series for reference.
 *
 * RM0351 (STM32L4x5/STM32L4x6)
 * http://www.st.com/resource/en/reference_manual/dm00083560.pdf
 *
 * RM0394 (STM32L43x/44x/45x/46x)
 * http://www.st.com/resource/en/reference_manual/dm00151940.pdf
 *
 * RM0432 (STM32L4R/4Sxx)
 * http://www.st.com/resource/en/reference_manual/dm00310109.pdf
 *
 * STM32L476RG Datasheet (for erase timing)
 * http://www.st.com/resource/en/datasheet/stm32l476rg.pdf
 *
 * The RM0351 devices have normally two banks, but on 512 and 256 kiB devices
 * an option byte is available to map all sectors to the first bank.
 * Both STM32 banks are treated as one OpenOCD bank, as other STM32 devices
 * handlers do!
 *
 * RM0394 devices have a single bank only.
 *
 * RM0432 devices have single and dual bank operating modes.
 *  - for STM32L4R/Sxx the FLASH size is 2Mbyte or 1Mbyte.
 *  - for STM32L4P/Q5x the FLASH size is 1Mbyte or 512Kbyte.
 * Bank page (sector) size is 4Kbyte (dual mode) or 8Kbyte (single mode).
 *
 * Bank mode is controlled by two different bits in option bytes register.
 *  - for STM32L4R/Sxx
 *    In 2M FLASH devices bit 22 (DBANK) controls Dual Bank mode.
 *    In 1M FLASH devices bit 21 (DB1M) controls Dual Bank mode.
 *  - for STM32L4P5/Q5x
 *    In 1M FLASH devices bit 22 (DBANK) controls Dual Bank mode.
 *    In 512K FLASH devices bit 21 (DB512K) controls Dual Bank mode.
 */

/* STM32WBxxx series for reference.
 *
 * RM0434 (STM32WB55/WB35x)
 * http://www.st.com/resource/en/reference_manual/dm00318631.pdf
 *
 * RM0471 (STM32WB50/WB30x)
 * http://www.st.com/resource/en/reference_manual/dm00622834.pdf
 *
 * RM0473 (STM32WB15x)
 * http://www.st.com/resource/en/reference_manual/dm00649196.pdf
 *
 * RM0478 (STM32WB10x)
 * http://www.st.com/resource/en/reference_manual/dm00689203.pdf
 */

/* STM32WLxxx series for reference.
 *
 * RM0461 (STM32WLEx)
 * http://www.st.com/resource/en/reference_manual/dm00530369.pdf
 *
 * RM0453 (STM32WL5x)
 * http://www.st.com/resource/en/reference_manual/dm00451556.pdf
 */

/* STM32G0xxx series for reference.
 *
 * RM0444 (STM32G0x1)
 * http://www.st.com/resource/en/reference_manual/dm00371828.pdf
 *
 * RM0454 (STM32G0x0)
 * http://www.st.com/resource/en/reference_manual/dm00463896.pdf
 */

/* STM32G4xxx series for reference.
 *
 * RM0440 (STM32G43x/44x/47x/48x/49x/4Ax)
 * http://www.st.com/resource/en/reference_manual/dm00355726.pdf
 *
 * Cat. 2 devices have single bank only, page size is 2kByte.
 *
 * Cat. 3 devices have single and dual bank operating modes,
 * Page size is 2kByte (dual mode) or 4kByte (single mode).
 *
 * Bank mode is controlled by bit 22 (DBANK) in option bytes register.
 * Both banks are treated as a single OpenOCD bank.
 *
 * Cat. 4 devices have single bank only, page size is 2kByte.
 */

/* STM32L5xxx series for reference.
 *
 * RM0428 (STM32L552xx/STM32L562xx)
 * http://www.st.com/resource/en/reference_manual/dm00346336.pdf
 */

/* Erase time can be as high as 25ms, 10x this and assume it's toast... */

#define FLASH_ERASE_TIMEOUT 250
#define FLASH_WRITE_TIMEOUT 50


/* relevant STM32L4 flags ****************************************************/
#define F_NONE              0
/* this flag indicates if the device flash is with dual bank architecture */
#define F_HAS_DUAL_BANK     BIT(0)
/* this flags is used for dual bank devices only, it indicates if the
 * 4 WRPxx are usable if the device is configured in single-bank mode */
#define F_USE_ALL_WRPXX     BIT(1)
/* this flag indicates if the device embeds a TrustZone security feature */
#define F_HAS_TZ            BIT(2)
/* this flag indicates if the device has the same flash registers as STM32L5 */
#define F_HAS_L5_FLASH_REGS BIT(3)
/* this flag indicates that programming should be done in quad-word
 * the default programming word size is double-word */
#define F_QUAD_WORD_PROG    BIT(4)
/* end of STM32L4 flags ******************************************************/


enum stm32l4_flash_reg_index {
	STM32_FLASH_ACR_INDEX,
	STM32_FLASH_KEYR_INDEX,
	STM32_FLASH_OPTKEYR_INDEX,
	STM32_FLASH_SR_INDEX,
	STM32_FLASH_CR_INDEX,
	/* for some devices like STM32WL5x, the CPU2 have a dedicated C2CR register w/o LOCKs,
	 * so it uses the C2CR for flash operations and CR for checking locks and locking */
	STM32_FLASH_CR_WLK_INDEX, /* FLASH_CR_WITH_LOCK */
	STM32_FLASH_OPTR_INDEX,
	STM32_FLASH_WRP1AR_INDEX,
	STM32_FLASH_WRP1BR_INDEX,
	STM32_FLASH_WRP2AR_INDEX,
	STM32_FLASH_WRP2BR_INDEX,
	STM32_FLASH_REG_INDEX_NUM,
};

enum stm32l4_rdp {
	RDP_LEVEL_0   = 0xAA,
	RDP_LEVEL_0_5 = 0x55, /* for devices with TrustZone enabled */
	RDP_LEVEL_1   = 0x00,
	RDP_LEVEL_2   = 0xCC
};

static const uint32_t stm32l4_flash_regs[STM32_FLASH_REG_INDEX_NUM] = {
	[STM32_FLASH_ACR_INDEX]      = 0x000,
	[STM32_FLASH_KEYR_INDEX]     = 0x008,
	[STM32_FLASH_OPTKEYR_INDEX]  = 0x00C,
	[STM32_FLASH_SR_INDEX]       = 0x010,
	[STM32_FLASH_CR_INDEX]       = 0x014,
	[STM32_FLASH_OPTR_INDEX]     = 0x020,
	[STM32_FLASH_WRP1AR_INDEX]   = 0x02C,
	[STM32_FLASH_WRP1BR_INDEX]   = 0x030,
	[STM32_FLASH_WRP2AR_INDEX]   = 0x04C,
	[STM32_FLASH_WRP2BR_INDEX]   = 0x050,
};

static const uint32_t stm32wl_cpu2_flash_regs[STM32_FLASH_REG_INDEX_NUM] = {
	[STM32_FLASH_ACR_INDEX]      = 0x000,
	[STM32_FLASH_KEYR_INDEX]     = 0x008,
	[STM32_FLASH_OPTKEYR_INDEX]  = 0x010,
	[STM32_FLASH_SR_INDEX]       = 0x060,
	[STM32_FLASH_CR_INDEX]       = 0x064,
	[STM32_FLASH_CR_WLK_INDEX]   = 0x014,
	[STM32_FLASH_OPTR_INDEX]     = 0x020,
	[STM32_FLASH_WRP1AR_INDEX]   = 0x02C,
	[STM32_FLASH_WRP1BR_INDEX]   = 0x030,
};

static const uint32_t stm32l5_ns_flash_regs[STM32_FLASH_REG_INDEX_NUM] = {
	[STM32_FLASH_ACR_INDEX]      = 0x000,
	[STM32_FLASH_KEYR_INDEX]     = 0x008, /* NSKEYR */
	[STM32_FLASH_OPTKEYR_INDEX]  = 0x010,
	[STM32_FLASH_SR_INDEX]       = 0x020, /* NSSR */
	[STM32_FLASH_CR_INDEX]       = 0x028, /* NSCR */
	[STM32_FLASH_OPTR_INDEX]     = 0x040,
	[STM32_FLASH_WRP1AR_INDEX]   = 0x058,
	[STM32_FLASH_WRP1BR_INDEX]   = 0x05C,
	[STM32_FLASH_WRP2AR_INDEX]   = 0x068,
	[STM32_FLASH_WRP2BR_INDEX]   = 0x06C,
};

static const uint32_t stm32l5_s_flash_regs[STM32_FLASH_REG_INDEX_NUM] = {
	[STM32_FLASH_ACR_INDEX]      = 0x000,
	[STM32_FLASH_KEYR_INDEX]     = 0x00C, /* SECKEYR */
	[STM32_FLASH_OPTKEYR_INDEX]  = 0x010,
	[STM32_FLASH_SR_INDEX]       = 0x024, /* SECSR */
	[STM32_FLASH_CR_INDEX]       = 0x02C, /* SECCR */
	[STM32_FLASH_OPTR_INDEX]     = 0x040,
	[STM32_FLASH_WRP1AR_INDEX]   = 0x058,
	[STM32_FLASH_WRP1BR_INDEX]   = 0x05C,
	[STM32_FLASH_WRP2AR_INDEX]   = 0x068,
	[STM32_FLASH_WRP2BR_INDEX]   = 0x06C,
};

struct stm32l4_rev {
	const uint16_t rev;
	const char *str;
};

struct stm32l4_part_info {
	uint16_t id;
	const char *device_str;
	const struct stm32l4_rev *revs;
	const size_t num_revs;
	const uint16_t max_flash_size_kb;
	const uint32_t flags; /* one bit per feature, see STM32L4 flags: macros F_XXX */
	const uint32_t flash_regs_base;
	const uint32_t fsize_addr;
	const uint32_t otp_base;
	const uint32_t otp_size;
};

struct stm32l4_flash_bank {
	bool probed;
	uint32_t idcode;
	unsigned int bank1_sectors;
	bool dual_bank_mode;
	int hole_sectors;
	uint32_t user_bank_size;
	uint32_t data_width;
	uint32_t cr_bker_mask;
	uint32_t sr_bsy_mask;
	uint32_t wrpxxr_mask;
	const struct stm32l4_part_info *part_info;
	uint32_t flash_regs_base;
	const uint32_t *flash_regs;
	bool otp_enabled;
	enum stm32l4_rdp rdp;
	bool tzen;
	uint32_t optr;
};

enum stm32_bank_id {
	STM32_BANK1,
	STM32_BANK2,
	STM32_ALL_BANKS
};

struct stm32l4_wrp {
	enum stm32l4_flash_reg_index reg_idx;
	uint32_t value;
	bool used;
	int first;
	int last;
	int offset;
};

/* human readable list of families this drivers supports (sorted alphabetically) */
static const char *device_families = "STM32G0/G4/L4/L4+/L5/U5/WB/WL";

static const struct stm32l4_rev stm32l47_l48xx_revs[] = {
	{ 0x1000, "1" }, { 0x1001, "2" }, { 0x1003, "3" }, { 0x1007, "4" }
};

static const struct stm32l4_rev stm32l43_l44xx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x2001, "Y" },
};

static const struct stm32l4_rev stm32g05_g06xx_revs[] = {
	{ 0x1000, "A" },
};

static const struct stm32l4_rev stm32_g07_g08xx_revs[] = {
	{ 0x1000, "A/Z" } /* A and Z, no typo in RM! */, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32l49_l4axx_revs[] = {
	{ 0x1000, "A" }, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32l45_l46xx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x2001, "Y" },
};

static const struct stm32l4_rev stm32l41_L42xx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x2001, "Y" },
};

static const struct stm32l4_rev stm32g03_g04xx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32g0b_g0cxx_revs[] = {
	{ 0x1000, "A" },
};

static const struct stm32l4_rev stm32g43_g44xx_revs[] = {
	{ 0x1000, "A" }, { 0x2000, "B" }, { 0x2001, "Z" },
};

static const struct stm32l4_rev stm32g47_g48xx_revs[] = {
	{ 0x1000, "A" }, { 0x2000, "B" }, { 0x2001, "Z" },
};

static const struct stm32l4_rev stm32l4r_l4sxx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x1003, "Y" }, { 0x100F, "W" },
};

static const struct stm32l4_rev stm32l4p_l4qxx_revs[] = {
	{ 0x1001, "Z" },
};

static const struct stm32l4_rev stm32l55_l56xx_revs[] = {
	{ 0x1000, "A" }, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32g49_g4axx_revs[] = {
	{ 0x1000, "A" },
};

static const struct stm32l4_rev stm32u57_u58xx_revs[] = {
	{ 0x1000, "A" }, { 0x1001, "Z" }, { 0x1003, "Y" }, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32wb1xx_revs[] = {
	{ 0x1000, "A" }, { 0x2000, "B" },
};

static const struct stm32l4_rev stm32wb5xx_revs[] = {
	{ 0x2001, "2.1" },
};

static const struct stm32l4_rev stm32wb3xx_revs[] = {
	{ 0x1000, "A" },
};

static const struct stm32l4_rev stm32wle_wl5xx_revs[] = {
	{ 0x1000, "1.0" },
};

static const struct stm32l4_part_info stm32l4_parts[] = {
	{
	  .id                    = DEVID_STM32L47_L48XX,
	  .revs                  = stm32l47_l48xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l47_l48xx_revs),
	  .device_str            = "STM32L47/L48xx",
	  .max_flash_size_kb     = 1024,
	  .flags                 = F_HAS_DUAL_BANK,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L43_L44XX,
	  .revs                  = stm32l43_l44xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l43_l44xx_revs),
	  .device_str            = "STM32L43/L44xx",
	  .max_flash_size_kb     = 256,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G05_G06XX,
	  .revs                  = stm32g05_g06xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g05_g06xx_revs),
	  .device_str            = "STM32G05/G06xx",
	  .max_flash_size_kb     = 64,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G07_G08XX,
	  .revs                  = stm32_g07_g08xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32_g07_g08xx_revs),
	  .device_str            = "STM32G07/G08xx",
	  .max_flash_size_kb     = 128,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L49_L4AXX,
	  .revs                  = stm32l49_l4axx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l49_l4axx_revs),
	  .device_str            = "STM32L49/L4Axx",
	  .max_flash_size_kb     = 1024,
	  .flags                 = F_HAS_DUAL_BANK,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L45_L46XX,
	  .revs                  = stm32l45_l46xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l45_l46xx_revs),
	  .device_str            = "STM32L45/L46xx",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L41_L42XX,
	  .revs                  = stm32l41_L42xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l41_L42xx_revs),
	  .device_str            = "STM32L41/L42xx",
	  .max_flash_size_kb     = 128,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G03_G04XX,
	  .revs                  = stm32g03_g04xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g03_g04xx_revs),
	  .device_str            = "STM32G03x/G04xx",
	  .max_flash_size_kb     = 64,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G0B_G0CXX,
	  .revs                  = stm32g0b_g0cxx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g0b_g0cxx_revs),
	  .device_str            = "STM32G0B/G0Cx",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_HAS_DUAL_BANK,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G43_G44XX,
	  .revs                  = stm32g43_g44xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g43_g44xx_revs),
	  .device_str            = "STM32G43/G44xx",
	  .max_flash_size_kb     = 128,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32G47_G48XX,
	  .revs                  = stm32g47_g48xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g47_g48xx_revs),
	  .device_str            = "STM32G47/G48xx",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_HAS_DUAL_BANK | F_USE_ALL_WRPXX,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L4R_L4SXX,
	  .revs                  = stm32l4r_l4sxx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l4r_l4sxx_revs),
	  .device_str            = "STM32L4R/L4Sxx",
	  .max_flash_size_kb     = 2048,
	  .flags                 = F_HAS_DUAL_BANK | F_USE_ALL_WRPXX,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L4P_L4QXX,
	  .revs                  = stm32l4p_l4qxx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l4p_l4qxx_revs),
	  .device_str            = "STM32L4P/L4Qxx",
	  .max_flash_size_kb     = 1024,
	  .flags                 = F_HAS_DUAL_BANK | F_USE_ALL_WRPXX,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32L55_L56XX,
	  .revs                  = stm32l55_l56xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32l55_l56xx_revs),
	  .device_str            = "STM32L55/L56xx",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_HAS_DUAL_BANK | F_USE_ALL_WRPXX | F_HAS_TZ | F_HAS_L5_FLASH_REGS,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x0BFA05E0,
	  .otp_base              = 0x0BFA0000,
	  .otp_size              = 512,
	},
	{
	  .id                    = DEVID_STM32G49_G4AXX,
	  .revs                  = stm32g49_g4axx_revs,
	  .num_revs              = ARRAY_SIZE(stm32g49_g4axx_revs),
	  .device_str            = "STM32G49/G4Axx",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32U57_U58XX,
	  .revs                  = stm32u57_u58xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32u57_u58xx_revs),
	  .device_str            = "STM32U57/U58xx",
	  .max_flash_size_kb     = 2048,
	  .flags                 = F_HAS_DUAL_BANK | F_QUAD_WORD_PROG | F_HAS_TZ | F_HAS_L5_FLASH_REGS,
	  .flash_regs_base       = 0x40022000,
	  .fsize_addr            = 0x0BFA07A0,
	  .otp_base              = 0x0BFA0000,
	  .otp_size              = 512,
	},
	{
	  .id                    = DEVID_STM32WB1XX,
	  .revs                  = stm32wb1xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32wb1xx_revs),
	  .device_str            = "STM32WB1x",
	  .max_flash_size_kb     = 320,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x58004000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32WB5XX,
	  .revs                  = stm32wb5xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32wb5xx_revs),
	  .device_str            = "STM32WB5x",
	  .max_flash_size_kb     = 1024,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x58004000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32WB3XX,
	  .revs                  = stm32wb3xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32wb3xx_revs),
	  .device_str            = "STM32WB3x",
	  .max_flash_size_kb     = 512,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x58004000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
	{
	  .id                    = DEVID_STM32WLE_WL5XX,
	  .revs                  = stm32wle_wl5xx_revs,
	  .num_revs              = ARRAY_SIZE(stm32wle_wl5xx_revs),
	  .device_str            = "STM32WLE/WL5x",
	  .max_flash_size_kb     = 256,
	  .flags                 = F_NONE,
	  .flash_regs_base       = 0x58004000,
	  .fsize_addr            = 0x1FFF75E0,
	  .otp_base              = 0x1FFF7000,
	  .otp_size              = 1024,
	},
};

/* flash bank stm32l4x <base> <size> 0 0 <target#> */
FLASH_BANK_COMMAND_HANDLER(stm32l4_flash_bank_command)
{
	struct stm32l4_flash_bank *stm32l4_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	/* fix-up bank base address: 0 is used for normal flash memory */
	if (bank->base == 0)
		bank->base = STM32_FLASH_BANK_BASE;

	stm32l4_info = calloc(1, sizeof(struct stm32l4_flash_bank));
	if (!stm32l4_info)
		return ERROR_FAIL; /* Checkme: What better error to use?*/
	bank->driver_priv = stm32l4_info;

	stm32l4_info->probed = false;
	stm32l4_info->otp_enabled = false;
	stm32l4_info->user_bank_size = bank->size;

	return ERROR_OK;
}

/* bitmap helper extension */
struct range {
	unsigned int start;
	unsigned int end;
};

static void bitmap_to_ranges(unsigned long *bitmap, unsigned int nbits,
		struct range *ranges, unsigned int *ranges_count) {
	*ranges_count = 0;
	bool last_bit = 0, cur_bit;
	for (unsigned int i = 0; i < nbits; i++) {
		cur_bit = test_bit(i, bitmap);

		if (cur_bit && !last_bit) {
			(*ranges_count)++;
			ranges[*ranges_count - 1].start = i;
			ranges[*ranges_count - 1].end = i;
		} else if (cur_bit && last_bit) {
			/* update (increment) the end this range */
			ranges[*ranges_count - 1].end = i;
		}

		last_bit = cur_bit;
	}
}

static inline int range_print_one(struct range *range, char *str)
{
	if (range->start == range->end)
		return sprintf(str, "[%d]", range->start);

	return sprintf(str, "[%d,%d]", range->start, range->end);
}

static char *range_print_alloc(struct range *ranges, unsigned int ranges_count)
{
	/* each range will be printed like the following: [start,end]
	 * start and end, both are unsigned int, an unsigned int takes 10 characters max
	 * plus 3 characters for '[', ',' and ']'
	 * thus means each range can take maximum 23 character
	 * after each range we add a ' ' as separator and finally we need the '\0'
	 * if the ranges_count is zero we reserve one char for '\0' to return an empty string */
	char *str = calloc(1, ranges_count * (24 * sizeof(char)) + 1);
	char *ptr = str;

	for (unsigned int i = 0; i < ranges_count; i++) {
		ptr += range_print_one(&(ranges[i]), ptr);

		if (i < ranges_count - 1)
			*(ptr++) = ' ';
	}

	return str;
}

/* end of bitmap helper extension */

static inline bool stm32l4_is_otp(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return bank->base == stm32l4_info->part_info->otp_base;
}

static int stm32l4_otp_enable(struct flash_bank *bank, bool enable)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;

	if (!stm32l4_is_otp(bank))
		return ERROR_FAIL;

	char *op_str = enable ? "enabled" : "disabled";

	LOG_INFO("OTP memory (bank #%d) is %s%s for write commands",
			bank->bank_number,
			stm32l4_info->otp_enabled == enable ? "already " : "",
			op_str);

	stm32l4_info->otp_enabled = enable;

	return ERROR_OK;
}

static inline bool stm32l4_otp_is_enabled(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return stm32l4_info->otp_enabled;
}

static void stm32l4_sync_rdp_tzen(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;

	bool tzen = false;

	if (stm32l4_info->part_info->flags & F_HAS_TZ)
		tzen = (stm32l4_info->optr & FLASH_TZEN) != 0;

	uint32_t rdp = stm32l4_info->optr & FLASH_RDP_MASK;

	/* for devices without TrustZone:
	 *   RDP level 0 and 2 values are to 0xAA and 0xCC
	 *   Any other value corresponds to RDP level 1
	 * for devices with TrusZone:
	 *   RDP level 0 and 2 values are 0xAA and 0xCC
	 *   RDP level 0.5 value is 0x55 only if TZEN = 1
	 *   Any other value corresponds to RDP level 1, including 0x55 if TZEN = 0
	 */

	if (rdp != RDP_LEVEL_0 && rdp != RDP_LEVEL_2) {
		if (!tzen || (tzen && rdp != RDP_LEVEL_0_5))
			rdp = RDP_LEVEL_1;
	}

	stm32l4_info->tzen = tzen;
	stm32l4_info->rdp = rdp;
}

static inline uint32_t stm32l4_get_flash_reg(struct flash_bank *bank, uint32_t reg_offset)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return stm32l4_info->flash_regs_base + reg_offset;
}

static inline uint32_t stm32l4_get_flash_reg_by_index(struct flash_bank *bank,
	enum stm32l4_flash_reg_index reg_index)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return stm32l4_get_flash_reg(bank, stm32l4_info->flash_regs[reg_index]);
}

static inline int stm32l4_read_flash_reg(struct flash_bank *bank, uint32_t reg_offset, uint32_t *value)
{
	return target_read_u32(bank->target, stm32l4_get_flash_reg(bank, reg_offset), value);
}

static inline int stm32l4_read_flash_reg_by_index(struct flash_bank *bank,
	enum stm32l4_flash_reg_index reg_index, uint32_t *value)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return stm32l4_read_flash_reg(bank, stm32l4_info->flash_regs[reg_index], value);
}

static inline int stm32l4_write_flash_reg(struct flash_bank *bank, uint32_t reg_offset, uint32_t value)
{
	return target_write_u32(bank->target, stm32l4_get_flash_reg(bank, reg_offset), value);
}

static inline int stm32l4_write_flash_reg_by_index(struct flash_bank *bank,
	enum stm32l4_flash_reg_index reg_index, uint32_t value)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return stm32l4_write_flash_reg(bank, stm32l4_info->flash_regs[reg_index], value);
}

static int stm32l4_wait_status_busy(struct flash_bank *bank, int timeout)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	uint32_t status;
	int retval = ERROR_OK;

	/* wait for busy to clear */
	for (;;) {
		retval = stm32l4_read_flash_reg_by_index(bank, STM32_FLASH_SR_INDEX, &status);
		if (retval != ERROR_OK)
			return retval;
		LOG_DEBUG("status: 0x%" PRIx32 "", status);
		if ((status & stm32l4_info->sr_bsy_mask) == 0)
			break;
		if (timeout-- <= 0) {
			LOG_ERROR("timed out waiting for flash");
			return ERROR_FAIL;
		}
		alive_sleep(1);
	}

	if (status & FLASH_WRPERR) {
		LOG_ERROR("stm32x device protected");
		retval = ERROR_FAIL;
	}

	/* Clear but report errors */
	if (status & FLASH_ERROR) {
		if (retval == ERROR_OK)
			retval = ERROR_FAIL;
		/* If this operation fails, we ignore it and report the original
		 * retval
		 */
		stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_SR_INDEX, status & FLASH_ERROR);
	}

	return retval;
}

/** set all FLASH_SECBB registers to the same value */
static int stm32l4_set_secbb(struct flash_bank *bank, uint32_t value)
{
	/* This function should be used only with device with TrustZone, do just a security check */
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	assert(stm32l4_info->part_info->flags & F_HAS_TZ);

	/* based on RM0438 Rev6 for STM32L5x devices:
	 * to modify a page block-based security attribution, it is recommended to
	 *  1- check that no flash operation is ongoing on the related page
	 *  2- add ISB instruction after modifying the page security attribute in SECBBxRy
	 *     this step is not need in case of JTAG direct access
	 */
	int retval = stm32l4_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;

	/* write SECBBxRy registers */
	LOG_DEBUG("setting secure block-based areas registers (SECBBxRy) to 0x%08x", value);

	const uint8_t secbb_regs[] = {
			FLASH_SECBB1(1), FLASH_SECBB1(2), FLASH_SECBB1(3), FLASH_SECBB1(4), /* bank 1 SECBB register offsets */
			FLASH_SECBB2(1), FLASH_SECBB2(2), FLASH_SECBB2(3), FLASH_SECBB2(4)  /* bank 2 SECBB register offsets */
	};


	unsigned int num_secbb_regs = ARRAY_SIZE(secbb_regs);

	/* in single bank mode, it's useless to modify FLASH_SECBB2Rx registers
	 * then consider only the first half of secbb_regs
	 */
	if (!stm32l4_info->dual_bank_mode)
		num_secbb_regs /= 2;

	for (unsigned int i = 0; i < num_secbb_regs; i++) {
		retval = stm32l4_write_flash_reg(bank, secbb_regs[i], value);
		if (retval != ERROR_OK)
			return retval;
	}

	return ERROR_OK;
}

static inline int stm32l4_get_flash_cr_with_lock_index(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	return (stm32l4_info->flash_regs[STM32_FLASH_CR_WLK_INDEX]) ?
		STM32_FLASH_CR_WLK_INDEX : STM32_FLASH_CR_INDEX;
}

static int stm32l4_unlock_reg(struct flash_bank *bank)
{
	const uint32_t flash_cr_index = stm32l4_get_flash_cr_with_lock_index(bank);
	uint32_t ctrl;

	/* first check if not already unlocked
	 * otherwise writing on STM32_FLASH_KEYR will fail
	 */
	int retval = stm32l4_read_flash_reg_by_index(bank, flash_cr_index, &ctrl);
	if (retval != ERROR_OK)
		return retval;

	if ((ctrl & FLASH_LOCK) == 0)
		return ERROR_OK;

	/* unlock flash registers */
	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_KEYR_INDEX, KEY1);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_KEYR_INDEX, KEY2);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_read_flash_reg_by_index(bank, flash_cr_index, &ctrl);
	if (retval != ERROR_OK)
		return retval;

	if (ctrl & FLASH_LOCK) {
		LOG_ERROR("flash not unlocked STM32_FLASH_CR: %" PRIx32, ctrl);
		return ERROR_TARGET_FAILURE;
	}

	return ERROR_OK;
}

static int stm32l4_unlock_option_reg(struct flash_bank *bank)
{
	const uint32_t flash_cr_index = stm32l4_get_flash_cr_with_lock_index(bank);
	uint32_t ctrl;

	int retval = stm32l4_read_flash_reg_by_index(bank, flash_cr_index, &ctrl);
	if (retval != ERROR_OK)
		return retval;

	if ((ctrl & FLASH_OPTLOCK) == 0)
		return ERROR_OK;

	/* unlock option registers */
	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_OPTKEYR_INDEX, OPTKEY1);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_OPTKEYR_INDEX, OPTKEY2);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_read_flash_reg_by_index(bank, flash_cr_index, &ctrl);
	if (retval != ERROR_OK)
		return retval;

	if (ctrl & FLASH_OPTLOCK) {
		LOG_ERROR("options not unlocked STM32_FLASH_CR: %" PRIx32, ctrl);
		return ERROR_TARGET_FAILURE;
	}

	return ERROR_OK;
}

static int stm32l4_perform_obl_launch(struct flash_bank *bank)
{
	int retval, retval2;

	retval = stm32l4_unlock_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_unlock_option_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	/* Set OBL_LAUNCH bit in CR -> system reset and option bytes reload,
	 * but the RMs explicitly do *NOT* list this as power-on reset cause, and:
	 * "Note: If the read protection is set while the debugger is still
	 * connected through JTAG/SWD, apply a POR (power-on reset) instead of a system reset."
	 */

	/* "Setting OBL_LAUNCH generates a reset so the option byte loading is performed under system reset" */
	/* Due to this reset ST-Link reports an SWD_DP_ERROR, despite the write was successful,
	 * then just ignore the returned value */
	stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, FLASH_OBL_LAUNCH);

	/* Need to re-probe after change */
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	stm32l4_info->probed = false;

err_lock:
	retval2 = stm32l4_write_flash_reg_by_index(bank, stm32l4_get_flash_cr_with_lock_index(bank),
			FLASH_LOCK | FLASH_OPTLOCK);

	if (retval != ERROR_OK)
		return retval;

	return retval2;
}

static int stm32l4_write_option(struct flash_bank *bank, uint32_t reg_offset,
	uint32_t value, uint32_t mask)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	uint32_t optiondata;
	int retval, retval2;

	retval = stm32l4_read_flash_reg(bank, reg_offset, &optiondata);
	if (retval != ERROR_OK)
		return retval;

	/* for STM32L5 and similar devices, use always non-secure
	 * registers for option bytes programming */
	const uint32_t *saved_flash_regs = stm32l4_info->flash_regs;
	if (stm32l4_info->part_info->flags & F_HAS_L5_FLASH_REGS)
		stm32l4_info->flash_regs = stm32l5_ns_flash_regs;

	retval = stm32l4_unlock_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_unlock_option_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	optiondata = (optiondata & ~mask) | (value & mask);

	retval = stm32l4_write_flash_reg(bank, reg_offset, optiondata);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, FLASH_OPTSTRT);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);

err_lock:
	retval2 = stm32l4_write_flash_reg_by_index(bank, stm32l4_get_flash_cr_with_lock_index(bank),
			FLASH_LOCK | FLASH_OPTLOCK);
	stm32l4_info->flash_regs = saved_flash_regs;

	if (retval != ERROR_OK)
		return retval;

	return retval2;
}

static int stm32l4_get_one_wrpxy(struct flash_bank *bank, struct stm32l4_wrp *wrpxy,
		enum stm32l4_flash_reg_index reg_idx, int offset)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	int ret;

	wrpxy->reg_idx = reg_idx;
	wrpxy->offset = offset;

	ret = stm32l4_read_flash_reg_by_index(bank, wrpxy->reg_idx , &wrpxy->value);
	if (ret != ERROR_OK)
		return ret;

	wrpxy->first = (wrpxy->value & stm32l4_info->wrpxxr_mask) + wrpxy->offset;
	wrpxy->last = ((wrpxy->value >> 16) & stm32l4_info->wrpxxr_mask) + wrpxy->offset;
	wrpxy->used = wrpxy->first <= wrpxy->last;

	return ERROR_OK;
}

static int stm32l4_get_all_wrpxy(struct flash_bank *bank, enum stm32_bank_id dev_bank_id,
		struct stm32l4_wrp *wrpxy, unsigned int *n_wrp)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	int ret;

	*n_wrp = 0;

	/* for single bank devices there is 2 WRP regions.
	 * for dual bank devices there is 2 WRP regions per bank,
	 *   if configured as single bank only 2 WRP are usable
	 *   except for STM32L4R/S/P/Q, G4 cat3, L5 ... all 4 WRP are usable
	 * note: this should be revised, if a device will have the SWAP banks option
	 */

	int wrp2y_sectors_offset = -1; /* -1 : unused */

	/* if bank_id is BANK1 or ALL_BANKS */
	if (dev_bank_id != STM32_BANK2) {
		/* get FLASH_WRP1AR */
		ret = stm32l4_get_one_wrpxy(bank, &wrpxy[(*n_wrp)++], STM32_FLASH_WRP1AR_INDEX, 0);
		if (ret != ERROR_OK)
			return ret;

		/* get WRP1BR */
		ret = stm32l4_get_one_wrpxy(bank, &wrpxy[(*n_wrp)++], STM32_FLASH_WRP1BR_INDEX, 0);
		if (ret != ERROR_OK)
			return ret;

		/* for some devices (like STM32L4R/S) in single-bank mode, the 4 WRPxx are usable */
		if ((stm32l4_info->part_info->flags & F_USE_ALL_WRPXX) && !stm32l4_info->dual_bank_mode)
			wrp2y_sectors_offset = 0;
	}

	/* if bank_id is BANK2 or ALL_BANKS */
	if (dev_bank_id != STM32_BANK1 && stm32l4_info->dual_bank_mode)
		wrp2y_sectors_offset = stm32l4_info->bank1_sectors;

	if (wrp2y_sectors_offset > -1) {
		/* get WRP2AR */
		ret = stm32l4_get_one_wrpxy(bank, &wrpxy[(*n_wrp)++], STM32_FLASH_WRP2AR_INDEX, wrp2y_sectors_offset);
		if (ret != ERROR_OK)
			return ret;

		/* get WRP2BR */
		ret = stm32l4_get_one_wrpxy(bank, &wrpxy[(*n_wrp)++], STM32_FLASH_WRP2BR_INDEX, wrp2y_sectors_offset);
		if (ret != ERROR_OK)
			return ret;
	}

	return ERROR_OK;
}

static int stm32l4_write_one_wrpxy(struct flash_bank *bank, struct stm32l4_wrp *wrpxy)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;

	int wrp_start = wrpxy->first - wrpxy->offset;
	int wrp_end = wrpxy->last - wrpxy->offset;

	uint32_t wrp_value = (wrp_start & stm32l4_info->wrpxxr_mask) | ((wrp_end & stm32l4_info->wrpxxr_mask) << 16);

	return stm32l4_write_option(bank, stm32l4_info->flash_regs[wrpxy->reg_idx], wrp_value, 0xffffffff);
}

static int stm32l4_write_all_wrpxy(struct flash_bank *bank, struct stm32l4_wrp *wrpxy, unsigned int n_wrp)
{
	int ret;

	for (unsigned int i = 0; i < n_wrp; i++) {
		ret = stm32l4_write_one_wrpxy(bank, &wrpxy[i]);
		if (ret != ERROR_OK)
			return ret;
	}

	return ERROR_OK;
}

static int stm32l4_protect_check(struct flash_bank *bank)
{
	unsigned int n_wrp;
	struct stm32l4_wrp wrpxy[4];

	int ret = stm32l4_get_all_wrpxy(bank, STM32_ALL_BANKS, wrpxy, &n_wrp);
	if (ret != ERROR_OK)
		return ret;

	/* initialize all sectors as unprotected */
	for (unsigned int i = 0; i < bank->num_sectors; i++)
		bank->sectors[i].is_protected = 0;

	/* now check WRPxy and mark the protected sectors */
	for (unsigned int i = 0; i < n_wrp; i++) {
		if (wrpxy[i].used) {
			for (int s = wrpxy[i].first; s <= wrpxy[i].last; s++)
				bank->sectors[s].is_protected = 1;
		}
	}

	return ERROR_OK;
}

static int stm32l4_erase(struct flash_bank *bank, unsigned int first,
		unsigned int last)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	int retval, retval2;

	assert((first <= last) && (last < bank->num_sectors));

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("cannot erase OTP memory");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* set all FLASH pages as secure */
		retval = stm32l4_set_secbb(bank, FLASH_SECBB_SECURE);
		if (retval != ERROR_OK) {
			/* restore all FLASH pages as non-secure */
			stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE); /* ignore the return value */
			return retval;
		}
	}

	retval = stm32l4_unlock_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	/*
	Sector Erase
	To erase a sector, follow the procedure below:
	1. Check that no Flash memory operation is ongoing by
	   checking the BSY bit in the FLASH_SR register
	2. Set the PER bit and select the page and bank
	   you wish to erase in the FLASH_CR register
	3. Set the STRT bit in the FLASH_CR register
	4. Wait for the BSY bit to be cleared
	 */

	for (unsigned int i = first; i <= last; i++) {
		uint32_t erase_flags;
		erase_flags = FLASH_PER | FLASH_STRT;

		if (i >= stm32l4_info->bank1_sectors) {
			uint8_t snb;
			snb = i - stm32l4_info->bank1_sectors;
			erase_flags |= snb << FLASH_PAGE_SHIFT | stm32l4_info->cr_bker_mask;
		} else
			erase_flags |= i << FLASH_PAGE_SHIFT;
		retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, erase_flags);
		if (retval != ERROR_OK)
			break;

		retval = stm32l4_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
		if (retval != ERROR_OK)
			break;
	}

err_lock:
	retval2 = stm32l4_write_flash_reg_by_index(bank, stm32l4_get_flash_cr_with_lock_index(bank), FLASH_LOCK);

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* restore all FLASH pages as non-secure */
		int retval3 = stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE);
		if (retval3 != ERROR_OK)
			return retval3;
	}

	if (retval != ERROR_OK)
		return retval;

	return retval2;
}

static int stm32l4_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
	struct target *target = bank->target;
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	int ret = ERROR_OK;
	unsigned int i;

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("cannot protect/unprotect OTP memory");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* the requested sectors could be located into bank1 and/or bank2 */
	bool use_bank2 = false;
	if (last >= stm32l4_info->bank1_sectors) {
		if (first < stm32l4_info->bank1_sectors) {
			/* the requested sectors for (un)protection are shared between
			 * bank 1 and 2, then split the operation */

			/*  1- deal with bank 1 sectors */
			LOG_DEBUG("The requested sectors for %s are shared between bank 1 and 2",
					set ? "protection" : "unprotection");
			ret = stm32l4_protect(bank, set, first, stm32l4_info->bank1_sectors - 1);
			if (ret != ERROR_OK)
				return ret;

			/*  2- then continue with bank 2 sectors */
			first = stm32l4_info->bank1_sectors;
		}

		use_bank2 = true;
	}

	/* refresh the sectors' protection */
	ret = stm32l4_protect_check(bank);
	if (ret != ERROR_OK)
		return ret;

	/* check if the desired protection is already configured */
	for (i = first; i <= last; i++) {
		if (bank->sectors[i].is_protected != set)
			break;
		else if (i == last) {
			LOG_INFO("The specified sectors are already %s", set ? "protected" : "unprotected");
			return ERROR_OK;
		}
	}

	/* all sectors from first to last (or part of them) could have different
	 * protection other than the requested */
	unsigned int n_wrp;
	struct stm32l4_wrp wrpxy[4];

	ret = stm32l4_get_all_wrpxy(bank, use_bank2 ? STM32_BANK2 : STM32_BANK1, wrpxy, &n_wrp);
	if (ret != ERROR_OK)
		return ret;

	/* use bitmap and range helpers to optimize the WRP usage */
	DECLARE_BITMAP(pages, bank->num_sectors);
	bitmap_zero(pages, bank->num_sectors);

	for (i = 0; i < n_wrp; i++) {
		if (wrpxy[i].used) {
			for (int p = wrpxy[i].first; p <= wrpxy[i].last; p++)
				set_bit(p, pages);
		}
	}

	/* we have at most 'n_wrp' WRP areas
	 * add one range if the user is trying to protect a fifth range */
	struct range ranges[n_wrp + 1];
	unsigned int ranges_count = 0;

	bitmap_to_ranges(pages, bank->num_sectors, ranges, &ranges_count);

	/* pretty-print the currently protected ranges */
	if (ranges_count > 0) {
		char *ranges_str = range_print_alloc(ranges, ranges_count);
		LOG_DEBUG("current protected areas: %s", ranges_str);
		free(ranges_str);
	} else
		LOG_DEBUG("current protected areas: none");

	if (set) { /* flash protect */
		for (i = first; i <= last; i++)
			set_bit(i, pages);
	} else { /* flash unprotect */
		for (i = first; i <= last; i++)
			clear_bit(i, pages);
	}

	/* check the ranges_count after the user request */
	bitmap_to_ranges(pages, bank->num_sectors, ranges, &ranges_count);

	/* pretty-print the requested areas for protection */
	if (ranges_count > 0) {
		char *ranges_str = range_print_alloc(ranges, ranges_count);
		LOG_DEBUG("requested areas for protection: %s", ranges_str);
		free(ranges_str);
	} else
		LOG_DEBUG("requested areas for protection: none");

	if (ranges_count > n_wrp) {
		LOG_ERROR("cannot set the requested protection "
				"(only %u write protection areas are available)" , n_wrp);
		return ERROR_FAIL;
	}

	/* re-init all WRPxy as disabled (first > last)*/
	for (i = 0; i < n_wrp; i++) {
		wrpxy[i].first = wrpxy[i].offset + 1;
		wrpxy[i].last = wrpxy[i].offset;
	}

	/* then configure WRPxy areas */
	for (i = 0; i < ranges_count; i++) {
		wrpxy[i].first = ranges[i].start;
		wrpxy[i].last = ranges[i].end;
	}

	/* finally write WRPxy registers */
	return stm32l4_write_all_wrpxy(bank, wrpxy, n_wrp);
}

/* count is the size divided by stm32l4_info->data_width */
static int stm32l4_write_block(struct flash_bank *bank, const uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
	struct target *target = bank->target;
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	struct working_area *write_algorithm;
	struct working_area *source;
	uint32_t address = bank->base + offset;
	struct reg_param reg_params[5];
	struct armv7m_algorithm armv7m_info;
	int retval = ERROR_OK;

	static const uint8_t stm32l4_flash_write_code[] = {
#include "../../../contrib/loaders/flash/stm32/stm32l4x.inc"
	};

	if (target_alloc_working_area(target, sizeof(stm32l4_flash_write_code),
			&write_algorithm) != ERROR_OK) {
		LOG_WARNING("no working area available, can't do block memory writes");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	retval = target_write_buffer(target, write_algorithm->address,
			sizeof(stm32l4_flash_write_code),
			stm32l4_flash_write_code);
	if (retval != ERROR_OK) {
		target_free_working_area(target, write_algorithm);
		return retval;
	}

	/* data_width should be multiple of double-word */
	assert(stm32l4_info->data_width % 8 == 0);
	const size_t extra_size = sizeof(struct stm32l4_work_area);
	uint32_t buffer_size = target_get_working_area_avail(target) - extra_size;
	/* buffer_size should be multiple of stm32l4_info->data_width */
	buffer_size &= ~(stm32l4_info->data_width - 1);

	if (buffer_size < 256) {
		LOG_WARNING("large enough working area not available, can't do block memory writes");
		target_free_working_area(target, write_algorithm);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	} else if (buffer_size > 16384) {
		/* probably won't benefit from more than 16k ... */
		buffer_size = 16384;
	}

	if (target_alloc_working_area_try(target, buffer_size + extra_size, &source) != ERROR_OK) {
		LOG_ERROR("allocating working area failed");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	armv7m_info.common_magic = ARMV7M_COMMON_MAGIC;
	armv7m_info.core_mode = ARM_MODE_THREAD;

	/* contrib/loaders/flash/stm32/stm32l4x.c:write() arguments */
	init_reg_param(&reg_params[0], "r0", 32, PARAM_IN_OUT);	/* stm32l4_work_area ptr , status (out) */
	init_reg_param(&reg_params[1], "r1", 32, PARAM_OUT);	/* buffer end */
	init_reg_param(&reg_params[2], "r2", 32, PARAM_OUT);	/* target address */
	init_reg_param(&reg_params[3], "r3", 32, PARAM_OUT);	/* count (of stm32l4_info->data_width) */

	buf_set_u32(reg_params[0].value, 0, 32, source->address);
	buf_set_u32(reg_params[1].value, 0, 32, source->address + source->size);
	buf_set_u32(reg_params[2].value, 0, 32, address);
	buf_set_u32(reg_params[3].value, 0, 32, count);

	/* write algo stack pointer */
	init_reg_param(&reg_params[4], "sp", 32, PARAM_OUT);
	buf_set_u32(reg_params[4].value, 0, 32, source->address +
			offsetof(struct stm32l4_work_area, stack) + LDR_STACK_SIZE);

	struct stm32l4_loader_params loader_extra_params;

	target_buffer_set_u32(target, (uint8_t *) &loader_extra_params.flash_sr_addr,
			stm32l4_get_flash_reg_by_index(bank, STM32_FLASH_SR_INDEX));
	target_buffer_set_u32(target, (uint8_t *) &loader_extra_params.flash_cr_addr,
			stm32l4_get_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX));
	target_buffer_set_u32(target, (uint8_t *) &loader_extra_params.flash_word_size,
			stm32l4_info->data_width);
	target_buffer_set_u32(target, (uint8_t *) &loader_extra_params.flash_sr_bsy_mask,
			stm32l4_info->sr_bsy_mask);

	retval = target_write_buffer(target, source->address, sizeof(loader_extra_params),
			(uint8_t *) &loader_extra_params);
	if (retval != ERROR_OK)
		return retval;

	retval = target_run_flash_async_algorithm(target, buffer, count, stm32l4_info->data_width,
			0, NULL,
			ARRAY_SIZE(reg_params), reg_params,
			source->address + offsetof(struct stm32l4_work_area, fifo),
			source->size - offsetof(struct stm32l4_work_area, fifo),
			write_algorithm->address, 0,
			&armv7m_info);

	if (retval == ERROR_FLASH_OPERATION_FAILED) {
		LOG_ERROR("error executing stm32l4 flash write algorithm");

		uint32_t error;
		stm32l4_read_flash_reg_by_index(bank, STM32_FLASH_SR_INDEX, &error);
		error &= FLASH_ERROR;

		if (error & FLASH_WRPERR)
			LOG_ERROR("flash memory write protected");

		if (error != 0) {
			LOG_ERROR("flash write failed = %08" PRIx32, error);
			/* Clear but report errors */
			stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_SR_INDEX, error);
			retval = ERROR_FAIL;
		}
	}

	target_free_working_area(target, source);
	target_free_working_area(target, write_algorithm);

	destroy_reg_param(&reg_params[0]);
	destroy_reg_param(&reg_params[1]);
	destroy_reg_param(&reg_params[2]);
	destroy_reg_param(&reg_params[3]);
	destroy_reg_param(&reg_params[4]);

	return retval;
}

/* count is the size divided by stm32l4_info->data_width */
static int stm32l4_write_block_without_loader(struct flash_bank *bank, const uint8_t *buffer,
				uint32_t offset, uint32_t count)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	struct target *target = bank->target;
	uint32_t address = bank->base + offset;
	int retval = ERROR_OK;

	/* wait for BSY bit */
	retval = stm32l4_wait_status_busy(bank, FLASH_WRITE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;

	/* set PG in FLASH_CR */
	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, FLASH_PG);
	if (retval != ERROR_OK)
		return retval;


	/* write directly to flash memory */
	const uint8_t *src = buffer;
	const uint32_t data_width_in_words = stm32l4_info->data_width / 4;
	while (count--) {
		retval = target_write_memory(target, address, 4, data_width_in_words, src);
		if (retval != ERROR_OK)
			return retval;

		/* wait for BSY bit */
		retval = stm32l4_wait_status_busy(bank, FLASH_WRITE_TIMEOUT);
		if (retval != ERROR_OK)
			return retval;

		src += stm32l4_info->data_width;
		address += stm32l4_info->data_width;
	}

	/* reset PG in FLASH_CR */
	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, 0);
	if (retval != ERROR_OK)
		return retval;

	return retval;
}

static int stm32l4_write(struct flash_bank *bank, const uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	int retval = ERROR_OK, retval2;

	if (stm32l4_is_otp(bank) && !stm32l4_otp_is_enabled(bank)) {
		LOG_ERROR("OTP memory is disabled for write commands");
		return ERROR_FAIL;
	}

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* ensure that stm32l4_info->data_width is 'at least' a multiple of dword */
	assert(stm32l4_info->data_width % 8 == 0);

	/* The flash write must be aligned to the 'stm32l4_info->data_width' boundary.
	 * The flash infrastructure ensures it, do just a security check */
	assert(offset % stm32l4_info->data_width == 0);
	assert(count % stm32l4_info->data_width == 0);

	/* STM32G4xxx Cat. 3 devices may have gaps between banks, check whether
	 * data to be written does not go into a gap:
	 * suppose buffer is fully contained in bank from sector 0 to sector
	 * num->sectors - 1 and sectors are ordered according to offset
	 */
	struct flash_sector *head = &bank->sectors[0];
	struct flash_sector *tail = &bank->sectors[bank->num_sectors - 1];

	while ((head < tail) && (offset >= (head + 1)->offset)) {
		/* buffer does not intersect head nor gap behind head */
		head++;
	}

	while ((head < tail) && (offset + count <= (tail - 1)->offset + (tail - 1)->size)) {
		/* buffer does not intersect tail nor gap before tail */
		--tail;
	}

	LOG_DEBUG("data: 0x%08" PRIx32 " - 0x%08" PRIx32 ", sectors: 0x%08" PRIx32 " - 0x%08" PRIx32,
		offset, offset + count - 1, head->offset, tail->offset + tail->size - 1);

	/* Now check that there is no gap from head to tail, this should work
	 * even for multiple or non-symmetric gaps
	 */
	while (head < tail) {
		if (head->offset + head->size != (head + 1)->offset) {
			LOG_ERROR("write into gap from " TARGET_ADDR_FMT " to " TARGET_ADDR_FMT,
				bank->base + head->offset + head->size,
				bank->base + (head + 1)->offset - 1);
			retval = ERROR_FLASH_DST_OUT_OF_BANK;
		}
		head++;
	}

	if (retval != ERROR_OK)
		return retval;

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* set all FLASH pages as secure */
		retval = stm32l4_set_secbb(bank, FLASH_SECBB_SECURE);
		if (retval != ERROR_OK) {
			/* restore all FLASH pages as non-secure */
			stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE); /* ignore the return value */
			return retval;
		}
	}

	retval = stm32l4_unlock_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;


	/* For TrustZone enabled devices, when TZEN is set and RDP level is 0.5,
	 * the debug is possible only in non-secure state.
	 * Thus means the flashloader will run in non-secure mode,
	 * and the workarea need to be in non-secure RAM */
	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0_5))
		LOG_WARNING("RDP = 0x55, the work-area should be in non-secure RAM (check SAU partitioning)");

	/* first try to write using the loader, for better performance */
	retval = stm32l4_write_block(bank, buffer, offset,
			count / stm32l4_info->data_width);

	/* if resources are not available write without a loader */
	if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
		LOG_WARNING("falling back to programming without a flash loader (slower)");
		retval = stm32l4_write_block_without_loader(bank, buffer, offset,
				count / stm32l4_info->data_width);
	}

err_lock:
	retval2 = stm32l4_write_flash_reg_by_index(bank, stm32l4_get_flash_cr_with_lock_index(bank), FLASH_LOCK);

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* restore all FLASH pages as non-secure */
		int retval3 = stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE);
		if (retval3 != ERROR_OK)
			return retval3;
	}

	if (retval != ERROR_OK) {
		LOG_ERROR("block write failed");
		return retval;
	}
	return retval2;
}

static int stm32l4_read_idcode(struct flash_bank *bank, uint32_t *id)
{
	int retval = ERROR_OK;
	struct target *target = bank->target;

	/* try reading possible IDCODE registers, in the following order */
	uint32_t dbgmcu_idcode[] = {DBGMCU_IDCODE_L4_G4, DBGMCU_IDCODE_G0, DBGMCU_IDCODE_L5};

	for (unsigned int i = 0; i < ARRAY_SIZE(dbgmcu_idcode); i++) {
		retval = target_read_u32(target, dbgmcu_idcode[i], id);
		if ((retval == ERROR_OK) && ((*id & 0xfff) != 0) && ((*id & 0xfff) != 0xfff))
			return ERROR_OK;
	}

	/* Workaround for STM32WL5x devices:
	 * DBGMCU_IDCODE cannot be read using CPU1 (Cortex-M0+) at AP1,
	 * to solve this read the UID64 (IEEE 64-bit unique device ID register) */

	struct armv7m_common *armv7m = target_to_armv7m_safe(target);
	if (!armv7m) {
		LOG_ERROR("Flash requires Cortex-M target");
		return ERROR_TARGET_INVALID;
	}

	/* CPU2 (Cortex-M0+) is supported only with non-hla adapters because it is on AP1.
	 * Using HLA adapters armv7m.debug_ap is null, and checking ap_num triggers a segfault */
	if (cortex_m_get_partno_safe(target) == CORTEX_M0P_PARTNO &&
			armv7m->debug_ap && armv7m->debug_ap->ap_num == 1) {
		uint32_t uid64_ids;

		/* UID64 is contains
		 *  - Bits 63:32 : DEVNUM (unique device number, different for each individual device)
		 *  - Bits 31:08 : STID (company ID) = 0x0080E1
		 *  - Bits 07:00 : DEVID (device ID) = 0x15
		 *
		 *  read only the fixed values {STID,DEVID} from UID64_IDS to identify the device as STM32WLx
		 */
		retval = target_read_u32(target, UID64_IDS, &uid64_ids);
		if (retval == ERROR_OK && uid64_ids == UID64_IDS_STM32WL) {
			/* force the DEV_ID to DEVID_STM32WLE_WL5XX and the REV_ID to unknown */
			*id = DEVID_STM32WLE_WL5XX;
			return ERROR_OK;
		}
	}

	LOG_ERROR("can't get the device id");
	return (retval == ERROR_OK) ? ERROR_FAIL : retval;
}

static const char *get_stm32l4_rev_str(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	const struct stm32l4_part_info *part_info = stm32l4_info->part_info;
	assert(part_info);

	const uint16_t rev_id = stm32l4_info->idcode >> 16;
	for (unsigned int i = 0; i < part_info->num_revs; i++) {
		if (rev_id == part_info->revs[i].rev)
			return part_info->revs[i].str;
	}
	return "'unknown'";
}

static const char *get_stm32l4_bank_type_str(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	assert(stm32l4_info->part_info);
	return stm32l4_is_otp(bank) ? "OTP" :
			stm32l4_info->dual_bank_mode ? "Flash dual" :
			"Flash single";
}

static int stm32l4_probe(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	const struct stm32l4_part_info *part_info;
	uint16_t flash_size_kb = 0xffff;

	if (!target_was_examined(target)) {
		LOG_ERROR("Target not examined yet");
		return ERROR_TARGET_NOT_EXAMINED;
	}

	struct armv7m_common *armv7m = target_to_armv7m_safe(target);
	if (!armv7m) {
		LOG_ERROR("Flash requires Cortex-M target");
		return ERROR_TARGET_INVALID;
	}

	stm32l4_info->probed = false;

	/* read stm32 device id registers */
	int retval = stm32l4_read_idcode(bank, &stm32l4_info->idcode);
	if (retval != ERROR_OK)
		return retval;

	const uint32_t device_id = stm32l4_info->idcode & 0xFFF;

	for (unsigned int n = 0; n < ARRAY_SIZE(stm32l4_parts); n++) {
		if (device_id == stm32l4_parts[n].id) {
			stm32l4_info->part_info = &stm32l4_parts[n];
			break;
		}
	}

	if (!stm32l4_info->part_info) {
		LOG_WARNING("Cannot identify target as an %s family device.", device_families);
		return ERROR_FAIL;
	}

	part_info = stm32l4_info->part_info;
	const char *rev_str = get_stm32l4_rev_str(bank);
	const uint16_t rev_id = stm32l4_info->idcode >> 16;

	LOG_INFO("device idcode = 0x%08" PRIx32 " (%s - Rev %s : 0x%04x)",
			stm32l4_info->idcode, part_info->device_str, rev_str, rev_id);

	stm32l4_info->flash_regs_base = stm32l4_info->part_info->flash_regs_base;
	stm32l4_info->data_width = (part_info->flags & F_QUAD_WORD_PROG) ? 16 : 8;
	stm32l4_info->cr_bker_mask = FLASH_BKER;
	stm32l4_info->sr_bsy_mask = FLASH_BSY;

	/* Set flash write alignment boundaries.
	 * Ask the flash infrastructure to ensure required alignment */
	bank->write_start_alignment = bank->write_end_alignment = stm32l4_info->data_width;

	/* initialise the flash registers layout */
	if (part_info->flags & F_HAS_L5_FLASH_REGS)
		stm32l4_info->flash_regs = stm32l5_ns_flash_regs;
	else
		stm32l4_info->flash_regs = stm32l4_flash_regs;

	/* read flash option register */
	retval = stm32l4_read_flash_reg_by_index(bank, STM32_FLASH_OPTR_INDEX, &stm32l4_info->optr);
	if (retval != ERROR_OK)
		return retval;

	stm32l4_sync_rdp_tzen(bank);

	/* for devices with trustzone, use flash secure registers when TZEN=1 and RDP is LEVEL_0 */
	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		if (part_info->flags & F_HAS_L5_FLASH_REGS) {
			stm32l4_info->flash_regs_base |= STM32L5_REGS_SEC_OFFSET;
			stm32l4_info->flash_regs = stm32l5_s_flash_regs;
		} else {
			LOG_ERROR("BUG: device supported incomplete");
			return ERROR_NOT_IMPLEMENTED;
		}
	}

	if (part_info->flags & F_HAS_TZ)
		LOG_INFO("TZEN = %d : TrustZone %s by option bytes",
				stm32l4_info->tzen,
				stm32l4_info->tzen ? "enabled" : "disabled");

	LOG_INFO("RDP level %s (0x%02X)",
			stm32l4_info->rdp == RDP_LEVEL_0 ? "0" : stm32l4_info->rdp == RDP_LEVEL_0_5 ? "0.5" : "1",
			stm32l4_info->rdp);

	if (stm32l4_is_otp(bank)) {
		bank->size = part_info->otp_size;

		LOG_INFO("OTP size is %d bytes, base address is " TARGET_ADDR_FMT, bank->size, bank->base);

		/* OTP memory is considered as one sector */
		free(bank->sectors);
		bank->num_sectors = 1;
		bank->sectors = alloc_block_array(0, part_info->otp_size, 1);

		if (!bank->sectors) {
			LOG_ERROR("failed to allocate bank sectors");
			return ERROR_FAIL;
		}

		stm32l4_info->probed = true;
		return ERROR_OK;
	} else if (bank->base != STM32_FLASH_BANK_BASE && bank->base != STM32_FLASH_S_BANK_BASE) {
		LOG_ERROR("invalid bank base address");
		return ERROR_FAIL;
	}

	/* get flash size from target. */
	retval = target_read_u16(target, part_info->fsize_addr, &flash_size_kb);

	/* failed reading flash size or flash size invalid (early silicon),
	 * default to max target family */
	if (retval != ERROR_OK || flash_size_kb == 0xffff || flash_size_kb == 0
			|| flash_size_kb > part_info->max_flash_size_kb) {
		LOG_WARNING("STM32 flash size failed, probe inaccurate - assuming %dk flash",
			part_info->max_flash_size_kb);
		flash_size_kb = part_info->max_flash_size_kb;
	}

	/* if the user sets the size manually then ignore the probed value
	 * this allows us to work around devices that have a invalid flash size register value */
	if (stm32l4_info->user_bank_size) {
		LOG_WARNING("overriding size register by configured bank size - MAY CAUSE TROUBLE");
		flash_size_kb = stm32l4_info->user_bank_size / 1024;
	}

	LOG_INFO("flash size = %dkbytes", flash_size_kb);

	/* did we assign a flash size? */
	assert((flash_size_kb != 0xffff) && flash_size_kb);

	const bool is_max_flash_size = flash_size_kb == stm32l4_info->part_info->max_flash_size_kb;

	stm32l4_info->bank1_sectors = 0;
	stm32l4_info->hole_sectors = 0;

	int num_pages = 0;
	int page_size_kb = 0;

	stm32l4_info->dual_bank_mode = false;

	switch (device_id) {
	case DEVID_STM32L47_L48XX:
	case DEVID_STM32L49_L4AXX:
		/* if flash size is max (1M) the device is always dual bank
		 * STM32L47/L48xx: has variants with 512K
		 * STM32L49/L4Axx: has variants with 512 and 256
		 * for these variants:
		 *   if DUAL_BANK = 0 -> single bank
		 *   else -> dual bank without gap
		 * note: the page size is invariant
		 */
		page_size_kb = 2;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;

		/* check DUAL_BANK option bit if the flash is less than 1M */
		if (is_max_flash_size || (stm32l4_info->optr & FLASH_L4_DUAL_BANK)) {
			stm32l4_info->dual_bank_mode = true;
			stm32l4_info->bank1_sectors = num_pages / 2;
		}
		break;
	case DEVID_STM32L43_L44XX:
	case DEVID_STM32G05_G06XX:
	case DEVID_STM32G07_G08XX:
	case DEVID_STM32L45_L46XX:
	case DEVID_STM32L41_L42XX:
	case DEVID_STM32G03_G04XX:
	case DEVID_STM32G43_G44XX:
	case DEVID_STM32G49_G4AXX:
	case DEVID_STM32WB1XX:
		/* single bank flash */
		page_size_kb = 2;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		break;
	case DEVID_STM32G0B_G0CXX:
		/* single/dual bank depending on DUAL_BANK option bit */
		page_size_kb = 2;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		stm32l4_info->cr_bker_mask = FLASH_BKER_G0;

		/* check DUAL_BANK bit */
		if (stm32l4_info->optr & FLASH_G0_DUAL_BANK) {
			stm32l4_info->sr_bsy_mask = FLASH_BSY | FLASH_BSY2;
			stm32l4_info->dual_bank_mode = true;
			stm32l4_info->bank1_sectors = num_pages / 2;
		}
		break;
	case DEVID_STM32G47_G48XX:
		/* STM32G47/8 can be single/dual bank:
		 *   if DUAL_BANK = 0 -> single bank
		 *   else -> dual bank WITH gap
		 */
		page_size_kb = 4;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		if (stm32l4_info->optr & FLASH_G4_DUAL_BANK) {
			stm32l4_info->dual_bank_mode = true;
			page_size_kb = 2;
			num_pages = flash_size_kb / page_size_kb;
			stm32l4_info->bank1_sectors = num_pages / 2;

			/* for devices with trimmed flash, there is a gap between both banks */
			stm32l4_info->hole_sectors =
				(part_info->max_flash_size_kb - flash_size_kb) / (2 * page_size_kb);
		}
		break;
	case DEVID_STM32L4R_L4SXX:
	case DEVID_STM32L4P_L4QXX:
		/* STM32L4R/S can be single/dual bank:
		 *   if size = 2M check DBANK bit
		 *   if size = 1M check DB1M bit
		 * STM32L4P/Q can be single/dual bank
		 *   if size = 1M check DBANK bit
		 *   if size = 512K check DB512K bit (same as DB1M bit)
		 */
		page_size_kb = 8;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		if ((is_max_flash_size && (stm32l4_info->optr & FLASH_L4R_DBANK)) ||
			(!is_max_flash_size && (stm32l4_info->optr & FLASH_LRR_DB1M))) {
			stm32l4_info->dual_bank_mode = true;
			page_size_kb = 4;
			num_pages = flash_size_kb / page_size_kb;
			stm32l4_info->bank1_sectors = num_pages / 2;
		}
		break;
	case DEVID_STM32L55_L56XX:
		/* STM32L55/L56xx can be single/dual bank:
		 *   if size = 512K check DBANK bit
		 *   if size = 256K check DB256K bit
		 *
		 * default page size is 4kb, if DBANK = 1, the page size is 2kb.
		 */

		page_size_kb = (stm32l4_info->optr & FLASH_L5_DBANK) ? 2 : 4;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;

		if ((is_max_flash_size && (stm32l4_info->optr & FLASH_L5_DBANK)) ||
			(!is_max_flash_size && (stm32l4_info->optr & FLASH_L5_DB256))) {
			stm32l4_info->dual_bank_mode = true;
			stm32l4_info->bank1_sectors = num_pages / 2;
		}
		break;
	case DEVID_STM32U57_U58XX:
		/* if flash size is max (2M) the device is always dual bank
		 * otherwise check DUALBANK
		 */
		page_size_kb = 8;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		if (is_max_flash_size || (stm32l4_info->optr & FLASH_U5_DUALBANK)) {
			stm32l4_info->dual_bank_mode = true;
			stm32l4_info->bank1_sectors = num_pages / 2;
		}
		break;
	case DEVID_STM32WB5XX:
	case DEVID_STM32WB3XX:
		/* single bank flash */
		page_size_kb = 4;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;
		break;
	case DEVID_STM32WLE_WL5XX:
		/* single bank flash */
		page_size_kb = 2;
		num_pages = flash_size_kb / page_size_kb;
		stm32l4_info->bank1_sectors = num_pages;

		/* CPU2 (Cortex-M0+) is supported only with non-hla adapters because it is on AP1.
		 * Using HLA adapters armv7m->debug_ap is null, and checking ap_num triggers a segfault */
		if (armv7m->debug_ap && armv7m->debug_ap->ap_num == 1)
			stm32l4_info->flash_regs = stm32wl_cpu2_flash_regs;
		break;
	default:
		LOG_ERROR("unsupported device");
		return ERROR_FAIL;
	}

	LOG_INFO("flash mode : %s-bank", stm32l4_info->dual_bank_mode ? "dual" : "single");

	const int gap_size_kb = stm32l4_info->hole_sectors * page_size_kb;

	if (gap_size_kb != 0) {
		LOG_INFO("gap detected from 0x%08x to 0x%08x",
			STM32_FLASH_BANK_BASE + stm32l4_info->bank1_sectors
				* page_size_kb * 1024,
			STM32_FLASH_BANK_BASE + (stm32l4_info->bank1_sectors
				* page_size_kb + gap_size_kb) * 1024 - 1);
	}

	/* number of significant bits in WRPxxR differs per device,
	 * always right adjusted, on some devices non-implemented
	 * bits read as '0', on others as '1' ...
	 * notably G4 Cat. 2 implement only 6 bits, contradicting the RM
	 */

	/* use *max_flash_size* instead of actual size as the trimmed versions
	 * certainly use the same number of bits
	 */
	uint32_t max_pages = stm32l4_info->part_info->max_flash_size_kb / page_size_kb;

	/* in dual bank mode number of pages is doubled, but extra bit is bank selection */
	stm32l4_info->wrpxxr_mask = ((max_pages >> (stm32l4_info->dual_bank_mode ? 1 : 0)) - 1);
	assert((stm32l4_info->wrpxxr_mask & 0xFFFF0000) == 0);
	LOG_DEBUG("WRPxxR mask 0x%04" PRIx16, (uint16_t)stm32l4_info->wrpxxr_mask);

	free(bank->sectors);

	bank->size = (flash_size_kb + gap_size_kb) * 1024;
	bank->num_sectors = num_pages;
	bank->sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);
	if (!bank->sectors) {
		LOG_ERROR("failed to allocate bank sectors");
		return ERROR_FAIL;
	}

	for (unsigned int i = 0; i < bank->num_sectors; i++) {
		bank->sectors[i].offset = i * page_size_kb * 1024;
		/* in dual bank configuration, if there is a gap between banks
		 * we fix up the sector offset to consider this gap */
		if (i >= stm32l4_info->bank1_sectors && stm32l4_info->hole_sectors)
			bank->sectors[i].offset += gap_size_kb * 1024;
		bank->sectors[i].size = page_size_kb * 1024;
		bank->sectors[i].is_erased = -1;
		bank->sectors[i].is_protected = 1;
	}

	stm32l4_info->probed = true;
	return ERROR_OK;
}

static int stm32l4_auto_probe(struct flash_bank *bank)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	if (stm32l4_info->probed) {
		uint32_t optr_cur;

		/* read flash option register and re-probe if optr value is changed */
		int retval = stm32l4_read_flash_reg_by_index(bank, STM32_FLASH_OPTR_INDEX, &optr_cur);
		if (retval != ERROR_OK)
			return retval;

		if (stm32l4_info->optr == optr_cur)
			return ERROR_OK;
	}

	return stm32l4_probe(bank);
}

static int get_stm32l4_info(struct flash_bank *bank, struct command_invocation *cmd)
{
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	const struct stm32l4_part_info *part_info = stm32l4_info->part_info;

	if (part_info) {
		const uint16_t rev_id = stm32l4_info->idcode >> 16;
		command_print_sameline(cmd, "%s - Rev %s : 0x%04x", part_info->device_str,
				get_stm32l4_rev_str(bank), rev_id);
		if (stm32l4_info->probed)
			command_print_sameline(cmd, " - %s-bank", get_stm32l4_bank_type_str(bank));
	} else {
		command_print_sameline(cmd, "Cannot identify target as an %s device", device_families);
	}

	return ERROR_OK;
}

static int stm32l4_mass_erase(struct flash_bank *bank)
{
	int retval, retval2;
	struct target *target = bank->target;
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("cannot erase OTP memory");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	uint32_t action = FLASH_MER1;

	if (stm32l4_info->part_info->flags & F_HAS_DUAL_BANK)
		action |= FLASH_MER2;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* set all FLASH pages as secure */
		retval = stm32l4_set_secbb(bank, FLASH_SECBB_SECURE);
		if (retval != ERROR_OK) {
			/* restore all FLASH pages as non-secure */
			stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE); /* ignore the return value */
			return retval;
		}
	}

	retval = stm32l4_unlock_reg(bank);
	if (retval != ERROR_OK)
		goto err_lock;

	/* mass erase flash memory */
	retval = stm32l4_wait_status_busy(bank, FLASH_ERASE_TIMEOUT / 10);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, action);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_write_flash_reg_by_index(bank, STM32_FLASH_CR_INDEX, action | FLASH_STRT);
	if (retval != ERROR_OK)
		goto err_lock;

	retval = stm32l4_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);

err_lock:
	retval2 = stm32l4_write_flash_reg_by_index(bank, stm32l4_get_flash_cr_with_lock_index(bank), FLASH_LOCK);

	if (stm32l4_info->tzen && (stm32l4_info->rdp == RDP_LEVEL_0)) {
		/* restore all FLASH pages as non-secure */
		int retval3 = stm32l4_set_secbb(bank, FLASH_SECBB_NON_SECURE);
		if (retval3 != ERROR_OK)
			return retval3;
	}

	if (retval != ERROR_OK)
		return retval;

	return retval2;
}

COMMAND_HANDLER(stm32l4_handle_mass_erase_command)
{
	if (CMD_ARGC < 1) {
		command_print(CMD, "stm32l4x mass_erase <STM32L4 bank>");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_mass_erase(bank);
	if (retval == ERROR_OK)
		command_print(CMD, "stm32l4x mass erase complete");
	else
		command_print(CMD, "stm32l4x mass erase failed");

	return retval;
}

COMMAND_HANDLER(stm32l4_handle_option_read_command)
{
	if (CMD_ARGC < 2) {
		command_print(CMD, "stm32l4x option_read <STM32L4 bank> <option_reg offset>");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	uint32_t reg_offset, reg_addr;
	uint32_t value = 0;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], reg_offset);
	reg_addr = stm32l4_get_flash_reg(bank, reg_offset);

	retval = stm32l4_read_flash_reg(bank, reg_offset, &value);
	if (retval != ERROR_OK)
		return retval;

	command_print(CMD, "Option Register: <0x%" PRIx32 "> = 0x%" PRIx32 "", reg_addr, value);

	return retval;
}

COMMAND_HANDLER(stm32l4_handle_option_write_command)
{
	if (CMD_ARGC < 3) {
		command_print(CMD, "stm32l4x option_write <STM32L4 bank> <option_reg offset> <value> [mask]");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	uint32_t reg_offset;
	uint32_t value = 0;
	uint32_t mask = 0xFFFFFFFF;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], reg_offset);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], value);

	if (CMD_ARGC > 3)
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[3], mask);

	command_print(CMD, "%s Option written.\n"
				"INFO: a reset or power cycle is required "
				"for the new settings to take effect.", bank->driver->name);

	retval = stm32l4_write_option(bank, reg_offset, value, mask);
	return retval;
}

COMMAND_HANDLER(stm32l4_handle_trustzone_command)
{
	if (CMD_ARGC < 1 || CMD_ARGC > 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	if (!(stm32l4_info->part_info->flags & F_HAS_TZ)) {
		LOG_ERROR("This device does not have a TrustZone");
		return ERROR_FAIL;
	}

	retval = stm32l4_read_flash_reg_by_index(bank, STM32_FLASH_OPTR_INDEX, &stm32l4_info->optr);
	if (retval != ERROR_OK)
		return retval;

	stm32l4_sync_rdp_tzen(bank);

	if (CMD_ARGC == 1) {
		/* only display the TZEN value */
		LOG_INFO("Global TrustZone Security is %s", stm32l4_info->tzen ? "enabled" : "disabled");
		return ERROR_OK;
	}

	bool new_tzen;
	COMMAND_PARSE_ENABLE(CMD_ARGV[1], new_tzen);

	if (new_tzen == stm32l4_info->tzen) {
		LOG_INFO("The requested TZEN is already programmed");
		return ERROR_OK;
	}

	if (new_tzen) {
		if (stm32l4_info->rdp != RDP_LEVEL_0) {
			LOG_ERROR("TZEN can be set only when RDP level is 0");
			return ERROR_FAIL;
		}
		retval = stm32l4_write_option(bank, stm32l4_info->flash_regs[STM32_FLASH_OPTR_INDEX],
				FLASH_TZEN, FLASH_TZEN);
	} else {
		/* Deactivation of TZEN (from 1 to 0) is only possible when the RDP is
		 * changing to level 0 (from level 1 to level 0 or from level 0.5 to level 0). */
		if (stm32l4_info->rdp != RDP_LEVEL_1 && stm32l4_info->rdp != RDP_LEVEL_0_5) {
			LOG_ERROR("Deactivation of TZEN is only possible when the RDP is changing to level 0");
			return ERROR_FAIL;
		}

		retval = stm32l4_write_option(bank, stm32l4_info->flash_regs[STM32_FLASH_OPTR_INDEX],
				RDP_LEVEL_0, FLASH_RDP_MASK | FLASH_TZEN);
	}

	if (retval != ERROR_OK)
		return retval;

	return stm32l4_perform_obl_launch(bank);
}

COMMAND_HANDLER(stm32l4_handle_option_load_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	retval = stm32l4_perform_obl_launch(bank);
	if (retval != ERROR_OK) {
		command_print(CMD, "stm32l4x option load failed");
		return retval;
	}


	command_print(CMD, "stm32l4x option load completed. Power-on reset might be required");

	return ERROR_OK;
}

COMMAND_HANDLER(stm32l4_handle_lock_command)
{
	struct target *target = NULL;

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("cannot lock/unlock OTP memory");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* set readout protection level 1 by erasing the RDP option byte */
	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	if (stm32l4_write_option(bank, stm32l4_info->flash_regs[STM32_FLASH_OPTR_INDEX],
			RDP_LEVEL_1, FLASH_RDP_MASK) != ERROR_OK) {
		command_print(CMD, "%s failed to lock device", bank->driver->name);
		return ERROR_OK;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(stm32l4_handle_unlock_command)
{
	struct target *target = NULL;

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("cannot lock/unlock OTP memory");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	if (stm32l4_write_option(bank, stm32l4_info->flash_regs[STM32_FLASH_OPTR_INDEX],
			RDP_LEVEL_0, FLASH_RDP_MASK) != ERROR_OK) {
		command_print(CMD, "%s failed to unlock device", bank->driver->name);
		return ERROR_OK;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(stm32l4_handle_wrp_info_command)
{
	if (CMD_ARGC < 1 || CMD_ARGC > 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	if (stm32l4_is_otp(bank)) {
		LOG_ERROR("OTP memory does not have write protection areas");
		return ERROR_FLASH_OPER_UNSUPPORTED;
	}

	struct stm32l4_flash_bank *stm32l4_info = bank->driver_priv;
	enum stm32_bank_id dev_bank_id = STM32_ALL_BANKS;
	if (CMD_ARGC == 2) {
		if (strcmp(CMD_ARGV[1], "bank1") == 0)
			dev_bank_id = STM32_BANK1;
		else if (strcmp(CMD_ARGV[1], "bank2") == 0)
			dev_bank_id = STM32_BANK2;
		else
			return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	if (dev_bank_id == STM32_BANK2) {
		if (!(stm32l4_info->part_info->flags & F_HAS_DUAL_BANK)) {
			LOG_ERROR("this device has no second bank");
			return ERROR_FAIL;
		} else if (!stm32l4_info->dual_bank_mode) {
			LOG_ERROR("this device is configured in single bank mode");
			return ERROR_FAIL;
		}
	}

	int ret;
	unsigned int n_wrp, i;
	struct stm32l4_wrp wrpxy[4];

	ret = stm32l4_get_all_wrpxy(bank, dev_bank_id, wrpxy, &n_wrp);
	if (ret != ERROR_OK)
		return ret;

	/* use bitmap and range helpers to better describe protected areas */
	DECLARE_BITMAP(pages, bank->num_sectors);
	bitmap_zero(pages, bank->num_sectors);

	for (i = 0; i < n_wrp; i++) {
		if (wrpxy[i].used) {
			for (int p = wrpxy[i].first; p <= wrpxy[i].last; p++)
				set_bit(p, pages);
		}
	}

	/* we have at most 'n_wrp' WRP areas */
	struct range ranges[n_wrp];
	unsigned int ranges_count = 0;

	bitmap_to_ranges(pages, bank->num_sectors, ranges, &ranges_count);

	if (ranges_count > 0) {
		/* pretty-print the protected ranges */
		char *ranges_str = range_print_alloc(ranges, ranges_count);
		command_print(CMD, "protected areas: %s", ranges_str);
		free(ranges_str);
	} else
		command_print(CMD, "no protected areas");

	return ERROR_OK;
}

COMMAND_HANDLER(stm32l4_handle_otp_command)
{
	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct flash_bank *bank;
	int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK)
		return retval;

	if (!stm32l4_is_otp(bank)) {
		command_print(CMD, "the specified bank is not an OTP memory");
		return ERROR_FAIL;
	}
	if (strcmp(CMD_ARGV[1], "enable") == 0)
		stm32l4_otp_enable(bank, true);
	else if (strcmp(CMD_ARGV[1], "disable") == 0)
		stm32l4_otp_enable(bank, false);
	else if (strcmp(CMD_ARGV[1], "show") == 0)
		command_print(CMD, "OTP memory bank #%d is %s for write commands.",
				bank->bank_number, stm32l4_otp_is_enabled(bank) ? "enabled" : "disabled");
	else
		return ERROR_COMMAND_SYNTAX_ERROR;

	return ERROR_OK;
}

static const struct command_registration stm32l4_exec_command_handlers[] = {
	{
		.name = "lock",
		.handler = stm32l4_handle_lock_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "Lock entire flash device.",
	},
	{
		.name = "unlock",
		.handler = stm32l4_handle_unlock_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "Unlock entire protected flash device.",
	},
	{
		.name = "mass_erase",
		.handler = stm32l4_handle_mass_erase_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "Erase entire flash device.",
	},
	{
		.name = "option_read",
		.handler = stm32l4_handle_option_read_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id reg_offset",
		.help = "Read & Display device option bytes.",
	},
	{
		.name = "option_write",
		.handler = stm32l4_handle_option_write_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id reg_offset value mask",
		.help = "Write device option bit fields with provided value.",
	},
	{
		.name = "trustzone",
		.handler = stm32l4_handle_trustzone_command,
		.mode = COMMAND_EXEC,
		.usage = "<bank_id> [enable|disable]",
		.help = "Configure TrustZone security",
	},
	{
		.name = "wrp_info",
		.handler = stm32l4_handle_wrp_info_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id [bank1|bank2]",
		.help = "list the protected areas using WRP",
	},
	{
		.name = "option_load",
		.handler = stm32l4_handle_option_load_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "Force re-load of device options (will cause device reset).",
	},
	{
		.name = "otp",
		.handler = stm32l4_handle_otp_command,
		.mode = COMMAND_EXEC,
		.usage = "<bank_id> <enable|disable|show>",
		.help = "OTP (One Time Programmable) memory write enable/disable",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration stm32l4_command_handlers[] = {
	{
		.name = "stm32l4x",
		.mode = COMMAND_ANY,
		.help = "stm32l4x flash command group",
		.usage = "",
		.chain = stm32l4_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

const struct flash_driver stm32l4x_flash = {
	.name = "stm32l4x",
	.commands = stm32l4_command_handlers,
	.flash_bank_command = stm32l4_flash_bank_command,
	.erase = stm32l4_erase,
	.protect = stm32l4_protect,
	.write = stm32l4_write,
	.read = default_flash_read,
	.probe = stm32l4_probe,
	.auto_probe = stm32l4_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = stm32l4_protect_check,
	.info = get_stm32l4_info,
	.free_driver_priv = default_flash_free_driver_priv,
};
