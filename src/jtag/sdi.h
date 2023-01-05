/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef OPENOCD_JTAG_SDI_H
#define OPENOCD_JTAG_SDI_H



struct sdi_driver {
	unsigned char (* transfer)(	  unsigned long iIndex, unsigned char iAddr,unsigned long iData,unsigned char iOP,unsigned char *oAddr,unsigned long *oData,unsigned char *oOP);
};

extern   int transfer(unsigned long iIndex, unsigned char iAddr,unsigned long iData,unsigned char iOP,unsigned char *oAddr,unsigned long *oData,unsigned char *oOP);

#endif 