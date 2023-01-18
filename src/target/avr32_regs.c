// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2010 by Oleksandr Tymoshenko <gonzo@bluezbox.com>       *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "target.h"
#include "jtag/jtag.h"
#include "avr32_jtag.h"
#include "avr32_regs.h"

static int avr32_jtag_read_reg(struct avr32_jtag *jtag_info, int reg,
		uint32_t *val)
{
	int retval;
	uint32_t dcsr;

	retval = avr32_jtag_exec(jtag_info, MTDR(AVR32_OCDREG_DCCPU, reg));
	if (retval != ERROR_OK)
		return retval;

	do {
		retval = avr32_jtag_nexus_read(jtag_info,
			AVR32_OCDREG_DCSR, &dcsr);

		if (retval != ERROR_OK)
			return retval;
	} while (!(dcsr & OCDREG_DCSR_CPUD));

	retval = avr32_jtag_nexus_read(jtag_info,
			AVR32_OCDREG_DCCPU, val);

	return retval;
}

static int avr32_jtag_write_reg(struct avr32_jtag *jtag_info, int reg,
		uint32_t val)
{
	int retval;
	uint32_t dcsr;

	/* Restore Status reg */
	retval = avr32_jtag_nexus_write(jtag_info,
				AVR32_OCDREG_DCEMU, val);
	if (retval != ERROR_OK)
		return retval;

	retval = avr32_jtag_exec(jtag_info, MFDR(reg, AVR32_OCDREG_DCEMU));
	if (retval != ERROR_OK)
		return retval;
	do {
		retval = avr32_jtag_nexus_read(jtag_info,
			AVR32_OCDREG_DCSR, &dcsr);
	} while (!(dcsr & OCDREG_DCSR_EMUD) && (retval == ERROR_OK));

	return retval;
}



int avr32_jtag_read_regs(struct avr32_jtag *jtag_info, uint32_t *regs)
{
	int i, retval;

	/* read core registers */
	for (i = 0; i < AVR32NUMCOREREGS - 1; i++)
		avr32_jtag_read_reg(jtag_info, i, regs + i);

	/* read status register */
	retval = avr32_jtag_exec(jtag_info, MFSR(0, 0));
	if (retval != ERROR_OK)
		return retval;

	retval = avr32_jtag_read_reg(jtag_info, 0, regs + AVR32_REG_SR);

	return retval;
}

int avr32_jtag_write_regs(struct avr32_jtag *jtag_info, uint32_t *regs)
{
	int i, retval;

	retval = avr32_jtag_write_reg(jtag_info, 0, regs[AVR32_REG_SR]);
	if (retval != ERROR_OK)
		return retval;

	/* Restore Status reg */
	retval = avr32_jtag_exec(jtag_info, MTSR(0, 0));
	if (retval != ERROR_OK)
		return retval;

	/*
	 * And now the rest of registers
	 */
	for (i = 0; i < AVR32NUMCOREREGS - 1; i++)
		avr32_jtag_write_reg(jtag_info, i, regs[i]);

	return ERROR_OK;
}
