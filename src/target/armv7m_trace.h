/***************************************************************************
 *   Copyright (C) 2015  Paul Fertser <fercerpav@gmail.com>                *
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

#ifndef OPENOCD_TARGET_ARMV7M_TRACE_H
#define OPENOCD_TARGET_ARMV7M_TRACE_H

#include <target/target.h>
#include <command.h>

/**
 * @file
 * Holds the interface to TPIU, ITM and DWT configuration functions.
 */

enum trace_config_type {
	TRACE_CONFIG_TYPE_DISABLED,	/**< tracing is disabled */
	TRACE_CONFIG_TYPE_EXTERNAL,	/**< trace output is captured externally */
	TRACE_CONFIG_TYPE_INTERNAL	/**< trace output is handled by OpenOCD adapter driver */
};

enum tpiu_pin_protocol {
	TPIU_PIN_PROTOCOL_SYNC,			/**< synchronous trace output */
	TPIU_PIN_PROTOCOL_ASYNC_MANCHESTER,	/**< asynchronous output with Manchester coding */
	TPIU_PIN_PROTOCOL_ASYNC_UART		/**< asynchronous output with NRZ coding */
};

enum itm_ts_prescaler {
	ITM_TS_PRESCALE1,	/**< no prescaling for the timestamp counter */
	ITM_TS_PRESCALE4,	/**< refclock divided by 4 for the timestamp counter */
	ITM_TS_PRESCALE16,	/**< refclock divided by 16 for the timestamp counter */
	ITM_TS_PRESCALE64,	/**< refclock divided by 64 for the timestamp counter */
};

struct armv7m_trace_config {
	/** Currently active trace capture mode */
	enum trace_config_type config_type;

	/** Currently active trace output mode */
	enum tpiu_pin_protocol pin_protocol;
	/** TPIU formatter enable/disable (in async mode) */
	bool formatter;
	/** Synchronous output port width */
	uint32_t port_size;

	/** Bitmask of currenty enabled ITM stimuli */
	uint32_t itm_ter[8];
	/** Identifier for multi-source trace stream formatting */
	unsigned int trace_bus_id;
	/** Prescaler for the timestamp counter */
	enum itm_ts_prescaler itm_ts_prescale;
	/** Enable differential timestamps */
	bool itm_diff_timestamps;
	/** Enable async timestamps model */
	bool itm_async_timestamps;
	/** Enable synchronisation packet transmission (for sync port only) */
	bool itm_synchro_packets;

	/** Current frequency of TRACECLKIN (usually matches HCLK) */
	unsigned int traceclkin_freq;
	/** Current frequency of trace port */
	unsigned int trace_freq;
	/** Handle to output trace data in INTERNAL capture mode */
	FILE *trace_file;
};

extern const struct command_registration armv7m_trace_command_handlers[];

/**
 * Configure hardware accordingly to the current TPIU target settings
 */
int armv7m_trace_tpiu_config(struct target *target);
/**
 * Configure hardware accordingly to the current ITM target settings
 */
int armv7m_trace_itm_config(struct target *target);

#endif /* OPENOCD_TARGET_ARMV7M_TRACE_H */
