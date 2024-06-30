// SPDX-License-Identifier: GPL-2.0-or-later

/*******************************************************************************
 *   Copyright (C) 2024 by Dinesh Annayya <dinesh@siplusplus.com>              *

  The Riscudino Quad SPI controller 
  specifically designed for SPI Flash Memories on Riscduino Score/Dcore/Qcore platforms.
    Riscduino Single Riscv Repo; https://github.com/riscduino/riscduino       * 
    Riscduino Dual Riscv Repo; https://github.com/riscduino/riscduino_dcore   * 
    Riscduino Quad Riscv Repo; https://github.com/riscduino/riscduino_qcore   * 
 
 ******************************************************************************/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "spi.h"
#include <jtag/jtag.h>
#include <helper/time_support.h>
#include <target/algorithm.h>
#include "target/riscv/riscv.h"

/* Register offsets */

#define RQSPIM_GLBL_CTRL          0x00
#define RQSPIM_DMEM_G0_RD_CTRL    0x04
#define RQSPIM_DMEM_G0_WR_CTRL    0x08
#define RQSPIM_DMEM_G1_RD_CTRL    0x0C
#define RQSPIM_DMEM_G1_WR_CTRL    0x10
#define RQSPIM_DMEM_CS_AMAP       0x14
#define RQSPIM_DMEM_CA_AMASK      0x18
#define RQSPIM_IMEM_CTRL1         0x1C
#define RQSPIM_IMEM_CTRL2         0x20
#define RQSPIM_IMEM_ADDR          0x24
#define RQSPIM_IMEM_WDATA         0x28
#define RQSPIM_IMEM_RDATA         0x2C
#define RQSPIM_SPI_STATUS         0x30

/* Fields */
#define RQSPIM_WREN               0x06 
#define RQSPIM_4KB_SECTOR_ERASE   0x20 /*4K Sector Erase */
#define RQSPIM_32KB_SECTOR_ERASE  0x52 /*4K Sector Erase */
#define RQSPIM_64KB_SECTOR_ERASE  0xD8 /*64K Sector Erase */
#define RQSPIM_BULK_ERASE         0xC7

#define P_FSM_C      0x0 // Command Phase Only
#define P_FSM_CW     0x1 // Command + Write DATA Phase Only
#define P_FSM_CA     0x2 // Command -> Address Phase Only
#define P_FSM_CAR    0x3 // Command -> Address -> Read Data
#define P_FSM_CADR   0x4 // Command -> Address -> Dummy -> Read Data
#define P_FSM_CAMR   0x5 // Command -> Address -> Mode -> Read Data
#define P_FSM_CAMDR  0x6 // Command -> Address -> Mode -> Dummy -> Read Data
#define P_FSM_CAW    0x7 // Command -> Address ->Write Data
#define P_FSM_CADW   0x8 // Command -> Address -> DUMMY + Write Data
#define P_FSM_CAMW   0x9 // Command -> Address -> MODE + Write Data
#define P_FSM_CDR    0xA // COMMAND -> DUMMY -> READ
#define P_FSM_CDW    0xB // COMMAND -> DUMMY -> WRITE
#define P_FSM_CR     0xC  // COMMAND -> READ

#define P_SINGLE     0x0 // SPI I/F Single
#define P_DOUBLE     0x1 // SPI I/F is Double
#define P_QUAD       0x2 // SPI I/F is Quad
#define P_QDDR       0x3 // SPI I/F QDDR

#define  P_MODE_SWITCH_IDLE     0x0 // SPI Buswidth Switch at Idle 
#define  P_MODE_SWITCH_AT_ADDR  0x1 // SPI Buswidth Switch at ADDR Phase
#define  P_MODE_SWITCH_AT_DATA  0x2 // SPI Buswidth Switch at Data Phase

/* Values */


/* Timeout in ms */
#define RQSPI_CMD_TIMEOUT   (100)
#define RQSPI_PROBE_TIMEOUT (100)
#define RQSPI_MAX_TIMEOUT  (30000)


struct rqspi_flash_bank {
	bool probed;
	target_addr_t ctrl_base;
	const struct flash_device *dev;
};

struct rqspi_target {
	char *name;
	uint32_t tap_idcode;
	uint32_t ctrl_base;
};

static const struct rqspi_target target_devices[] = {
	/* name,   tap_idcode, ctrl_base */
	{ "Riscduino core0 idcode", 0xdeb10c05, 0x10000000 },
	{ "Riscduino core1 idcode", 0xdeb11c05, 0x10000000 },
	{ "Riscduino core2 idcode", 0xdeb12c05, 0x10000000 },
	{ "Riscduino core3 idcode", 0xdeb13c05, 0x10000000 },
	{ NULL, 0, 0 }
};

FLASH_BANK_COMMAND_HANDLER(rqspi_flash_bank_command)
{
	struct rqspi_flash_bank *rqspi_info;

	LOG_DEBUG("%s", __func__);

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	rqspi_info = malloc(sizeof(struct rqspi_flash_bank));
	if (!rqspi_info) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	bank->driver_priv = rqspi_info;
	rqspi_info->probed = false;
	rqspi_info->ctrl_base = 0;
	if (CMD_ARGC >= 7) {
        // Setting the Based address for the QSPI
		COMMAND_PARSE_ADDRESS(CMD_ARGV[6], rqspi_info->ctrl_base);
		LOG_DEBUG("ASSUMING RQSPI device at ctrl_base = " TARGET_ADDR_FMT,
				rqspi_info->ctrl_base);
	}

	return ERROR_OK;
}

/** Riscduino QSpi Register Read **/ 
static int rqspi_read_reg(struct flash_bank *bank, uint32_t *value, target_addr_t address)
{
	struct target *target = bank->target;
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;

	int result = target_read_u32(target, rqspi_info->ctrl_base + address, value);
	if (result != ERROR_OK) {
		LOG_ERROR("rqspi_read_reg() error at " TARGET_ADDR_FMT,
				rqspi_info->ctrl_base + address);
		return result;
	}
	return ERROR_OK;
}

/** Riscduino QSpi Register Write **/ 
static int rqspi_write_reg(struct flash_bank *bank, target_addr_t address, uint32_t value)
{
	struct target *target = bank->target;
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;

	int result = target_write_u32(target, rqspi_info->ctrl_base + address, value);
	if (result != ERROR_OK) {
		LOG_ERROR("rqspi_write_reg() error writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
				value, rqspi_info->ctrl_base + address);
		return result;
	}
	return ERROR_OK;
}


/** Check the Work in Progress Status flag ***/
static int rqspi_wip(struct flash_bank *bank, int timeout)
{
	long long endtime;
	long long curtime;
    uint32_t status;

    curtime = timeval_ms();
	endtime = curtime + timeout;

     if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL1, 0x00000001)  != ERROR_OK) return ERROR_FAIL;
     if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, 0x040c0005)  != ERROR_OK) return ERROR_FAIL;


	do {
        status = 0xFF;
	    if (rqspi_read_reg(bank, &status, RQSPIM_IMEM_RDATA) != ERROR_OK) return ERROR_FAIL;
	    if(status == 0) return ERROR_OK;
        curtime = timeval_ms();
	} while (curtime < endtime);

	endtime = timeval_ms();

	LOG_ERROR("timeout");
	return ERROR_FAIL;
}

/**** Erasing 64KB Sector  ***/
static int rqspi_erase_sector(struct flash_bank *bank, int sector)
{

    uint32_t wrData;
    LOG_INFO("Erasing Sector: %x Start Addr: 0x%x and End Addr: 0x%x ", sector, sector * 0x10000, ((sector+1) * 0x10000)-1);

	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL1, 0x1)                 != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, RQSPIM_WREN)         != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_WDATA, 0x0)                 != ERROR_OK) return ERROR_FAIL;

	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL1, 0x1)                 != ERROR_OK) return ERROR_FAIL;

    wrData =  (0x2 << 20) | (P_FSM_CA  << 16)| RQSPIM_64KB_SECTOR_ERASE;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, wrData)              != ERROR_OK) return ERROR_FAIL;

    wrData = sector << 16; // Each Sector is 64KB Size
	if (rqspi_write_reg(bank, RQSPIM_IMEM_ADDR, wrData)              != ERROR_OK) return ERROR_FAIL;

	if (rqspi_write_reg(bank, RQSPIM_IMEM_WDATA, 0x0)                 != ERROR_OK) return ERROR_FAIL;

	if (rqspi_wip(bank, RQSPI_MAX_TIMEOUT) != ERROR_OK) return ERROR_FAIL;


	return ERROR_OK;
}

static int rqspi_erase(struct flash_bank *bank, unsigned int first,
		unsigned int last)
{
	struct target *target = bank->target;
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;
	int retval = ERROR_OK;

	LOG_DEBUG("%s: from sector %u to sector %u", __func__, first, last);

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if ((last < first) || (last >= bank->num_sectors)) {
		LOG_ERROR("Flash sector invalid");
		return ERROR_FLASH_SECTOR_INVALID;
	}

	if (!(rqspi_info->probed)) {
		LOG_ERROR("Flash bank not probed");
		return ERROR_FLASH_BANK_NOT_PROBED;
	}

	for (unsigned int sector = first; sector <= last; sector++) {
		if (bank->sectors[sector].is_protected) {
			LOG_ERROR("Flash sector %u protected", sector);
			return ERROR_FAIL;
		}
	}

	for (unsigned int sector = first; sector <= last; sector++) {
		retval = rqspi_erase_sector(bank, sector);
		if (retval != ERROR_OK)
			goto done;
		keep_alive();
	}

	/* Switch to HW mode before return to prompt */
done:
	return retval;
}

static int rqspi_protect(struct flash_bank *bank, int set,
		unsigned int first, unsigned int last)
{
	for (unsigned int sector = first; sector <= last; sector++)
		bank->sectors[sector].is_protected = set;
	return ERROR_OK;
}
//---------------------------------------------------------------------------------
// Setup the Flash Write Command, Maximum Burst Supported in Riscduino is 250 Byte
//---------------------------------------------------------------------------------

static int rqspi_flash_write_cmd(struct flash_bank *bank,uint32_t offset, uint32_t burst_size) {

   uint32_t cmd;

	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, 0x00000006)    != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_WDATA, 0x00000000)    != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL1, 0x00000001)    != ERROR_OK) return ERROR_FAIL;
    cmd = 0x00270002 | burst_size << 24;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, cmd)       != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_ADDR, offset)         != ERROR_OK) return ERROR_FAIL;

   return ERROR_OK;


}



static int rqspi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
	struct target *target = bank->target;
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;
	uint32_t dataout,ncnt,addr,tcnt,burst_size;
	uint32_t writeData,readData,raddr,rcnt;

	LOG_DEBUG("bank->size=0x%x offset=0x%08" PRIx32 " count=0x%08" PRIx32,
			bank->size, offset, count);

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	if (offset + count > rqspi_info->dev->size_in_bytes) {
		LOG_WARNING("Write past end of flash. Extra data discarded.");
		count = rqspi_info->dev->size_in_bytes - offset;
	}

	/* Check sector protection */
	for (unsigned int sector = 0; sector < bank->num_sectors; sector++) {
		/* Start offset in or before this sector? */
		/* End offset in or behind this sector? */
		if ((offset <
					(bank->sectors[sector].offset + bank->sectors[sector].size))
				&& ((offset + count - 1) >= bank->sectors[sector].offset)
				&& bank->sectors[sector].is_protected) {
			LOG_ERROR("Flash sector %u protected", sector);
			return ERROR_FAIL;
		}
	}


   addr = offset;
   dataout = 0;
   ncnt = 0;
   tcnt = 0;
   rcnt = 0;

   raddr = 0;

   // As Max 256 Byte Write Burst is supported by External Flash, we are spliting the burst in multiple of 128 Byte

   do {
       burst_size = (count-tcnt) > 128 ? 128 : (count-tcnt);

       // Setup the command
       rqspi_flash_write_cmd(bank,addr, burst_size);

       dataout = 0;
       for(uint32_t i =0; i < burst_size; i++) {
            int tShift = (8 * ncnt);
            dataout |= buffer[tcnt] << tShift; 
            ncnt = ncnt + 1;
            if(ncnt == 4){
               if (rqspi_write_reg(bank, RQSPIM_IMEM_WDATA, dataout)   != ERROR_OK) return ERROR_FAIL;
               addr = addr+4;
               ncnt = 0;
               dataout = 0x00;
            }
            tcnt++;
        }

        // if pending byte is has less than 4 bytes
        if(ncnt > 0 && ncnt < 4) {  
            LOG_INFO("Writing Flash Partial DW, Address: 0x%08x Data: %08x Cnt:%d", addr, dataout, ncnt);
            LOG_INFO("Writing Flash Address: 0x%08x Data: 0x%08x\n", addr, dataout);
            if (rqspi_write_reg(bank, RQSPIM_IMEM_WDATA, dataout)   != ERROR_OK) return ERROR_FAIL;
            ncnt = 0;
        }

        // check for Busy command
	    if (rqspi_wip(bank, RQSPI_MAX_TIMEOUT) != ERROR_OK) return ERROR_FAIL;

        /** Read Back and validate  **/
       ncnt = 0;
       writeData = 0;
       readData = 0;
       for(uint32_t i =0; i < burst_size; i++) {
            int tShift = (8 * ncnt);
            writeData |= buffer[rcnt] << tShift; 
            ncnt = ncnt + 1;
            if(ncnt == 4){
	           int result = target_read_u32(target, 0x04000000 + raddr, &readData);
	           if (result == ERROR_OK) {
                  if(readData != writeData) { 
                     return ERROR_FAIL;
                  } 

               }
               raddr = raddr+4;
               ncnt = 0;
               writeData = 0x00;
            }
            rcnt++;
        }


     } while(tcnt != count);


	return ERROR_OK;

}
static int rqspi_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count) {

	return ERROR_OK;

}

/* Return ID of flash device */
static int rqspi_read_flash_id(struct flash_bank *bank, uint32_t *id)
{
	struct target *target = bank->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	/* Send SPI command "read ID" */
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL1, 0x1) != ERROR_OK) return ERROR_FAIL;
	if (rqspi_write_reg(bank, RQSPIM_IMEM_CTRL2, 0x040c009f) != ERROR_OK) return ERROR_FAIL;

	if (rqspi_read_reg(bank, id, RQSPIM_IMEM_RDATA) != ERROR_OK) return ERROR_FAIL;

    if(*id != 0x001640ef && *id != 0x00190201) {
		LOG_ERROR("SPI Flash Device ID => 0x%08x [BAD]", *id);
	    return ERROR_OK;
    } else {
		LOG_DEBUG("SPI Flash Device ID => 0x%08x [GOOD]\n",*id);
    }

	return ERROR_OK;
}

static int rqspi_probe(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;
	struct flash_sector *sectors;
	uint32_t id = 0; /* silence uninitialized warning */
	const struct rqspi_target *target_device;
	uint32_t sectorsize;

	if (rqspi_info->probed)
		free(bank->sectors);

	if (rqspi_info->ctrl_base == 0) {
		for (target_device = target_devices ; target_device->name ; ++target_device)
			if (target_device->tap_idcode == target->tap->idcode)
				break;

		if (!target_device->name) {
			LOG_ERROR("Device ID 0x%" PRIx32 " is not known as FESPI capable",
					target->tap->idcode);
			return ERROR_FAIL;
		}

		rqspi_info->ctrl_base = target_device->ctrl_base;

		LOG_DEBUG("Valid FESPI on device %s at address " TARGET_ADDR_FMT,
				target_device->name, bank->base);

	} else {
	  LOG_DEBUG("Assuming FESPI as specified at address " TARGET_ADDR_FMT
			  " with ctrl at " TARGET_ADDR_FMT, rqspi_info->ctrl_base,
			  bank->base);
	}



	rqspi_read_flash_id(bank, &id);

	rqspi_info->dev = NULL;
	for (const struct flash_device *p = flash_devices; p->name ; p++)
		if (p->device_id == id) {
			rqspi_info->dev = p;
			break;
		}

	if (!rqspi_info->dev) {
		LOG_ERROR("Unknown flash device (ID 0x%08" PRIx32 ")", id);
		return ERROR_FAIL;
	}

	LOG_INFO("Found flash device \'%s\' (ID 0x%08" PRIx32 ")",
			rqspi_info->dev->name, rqspi_info->dev->device_id);

	/* Set correct size value */
	bank->size = rqspi_info->dev->size_in_bytes;

	if (bank->size <= (1UL << 16))
		LOG_WARNING("device needs 2-byte addresses - not implemented");

	/* if no sectors, treat whole bank as single sector */
	sectorsize = rqspi_info->dev->sectorsize ?
		rqspi_info->dev->sectorsize : rqspi_info->dev->size_in_bytes;

	/* create and fill sectors array */
	bank->num_sectors = rqspi_info->dev->size_in_bytes / sectorsize;
	sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);
	if (!sectors) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	for (unsigned int sector = 0; sector < bank->num_sectors; sector++) {
		sectors[sector].offset = sector * sectorsize;
		sectors[sector].size = sectorsize;
		sectors[sector].is_erased = -1;
		sectors[sector].is_protected = 0;
	}

	bank->sectors = sectors;
	rqspi_info->probed = true;
	return ERROR_OK;

}

static int rqspi_auto_probe(struct flash_bank *bank)
{
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;
	if (rqspi_info->probed)
		return ERROR_OK;
	return rqspi_probe(bank);
}

static int rqspi_protect_check(struct flash_bank *bank)
{
	/* Nothing to do. Protection is only handled in SW. */
	return ERROR_OK;
}

static int get_rqspi_info(struct flash_bank *bank, struct command_invocation *cmd)
{
	struct rqspi_flash_bank *rqspi_info = bank->driver_priv;

	if (!(rqspi_info->probed)) {
		command_print_sameline(cmd, "\nRQSPI flash bank not probed yet\n");
		return ERROR_OK;
	}

	command_print_sameline(cmd, "\nRQSPI flash information:\n"
			"  Device \'%s\' (ID 0x%08" PRIx32 ")\n",
			rqspi_info->dev->name, rqspi_info->dev->device_id);

	return ERROR_OK;
}

const struct flash_driver rqspi_flash = {
	.name = "rqspi",
	.flash_bank_command = rqspi_flash_bank_command,
	.erase = rqspi_erase,
	.protect = rqspi_protect,
	.write = rqspi_write,
	.verify = rqspi_verify,
	.read = default_flash_read,
	.probe = rqspi_probe,
	.auto_probe = rqspi_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = rqspi_protect_check,
	.info = get_rqspi_info,
	.free_driver_priv = default_flash_free_driver_priv
};
