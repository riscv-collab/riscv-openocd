/* SPDX-License-Identifier: GPL-2.0-or-later */

/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2007,2008 Øyvind Harboe                                 *
 *   oyvind.harboe@zylin.com                                               *
 ***************************************************************************/

#ifndef OPENOCD_JTAG_DRIVERS_BITBANG_H
#define OPENOCD_JTAG_DRIVERS_BITBANG_H

#include <jtag/swd.h>

typedef enum {
	BB_LOW,
	BB_HIGH,
	BB_ERROR
} bb_value_t;

/** Low level callbacks (for bitbang).
 *
 * Either read(), or sample() and read_sample() must be implemented.
 *
 * The sample functions allow an interface to batch a number of writes and
 * sample requests together. Not waiting for a value to come back can greatly
 * increase throughput. */
struct bitbang_interface {
	/** Sample TDO and return the value. */
	bb_value_t (*read)(void);

	/** The number of TDO samples that can be buffered up before the caller has
	 * to call read_sample. */
	size_t buf_size;

	/** Sample TDO and put the result in a buffer. */
	int (*sample)(void);

	/** Return the next unread value from the buffer. */
	bb_value_t (*read_sample)(void);

	/** Set TCK, TMS, and TDI to the given values. */
	int (*write)(int tck, int tms, int tdi);

	/** Blink led (optional). */
	int (*blink)(int on);

	/** Sample SWDIO and return the value. */
	int (*swdio_read)(void);

	/** Set direction of SWDIO. */
	void (*swdio_drive)(bool on);

	/** Set SWCLK and SWDIO to the given value. */
	int (*swd_write)(int swclk, int swdio);
};

extern const struct swd_driver bitbang_swd;

int bitbang_execute_queue(void);

extern struct bitbang_interface *bitbang_interface;

#endif /* OPENOCD_JTAG_DRIVERS_BITBANG_H */
