/* SPDX-License-Identifier: GPL-2.0-or-later */

/***************************************************************************
 *   Copyright (C) 2008 digenius technology GmbH.                          *
 *   Michael Bruck                                                         *
 *                                                                         *
 *   Copyright (C) 2008 Georg Acher <acher@in.tum.de>                      *
 ***************************************************************************/

#ifndef OPENOCD_TARGET_ARM11_H
#define OPENOCD_TARGET_ARM11_H

#include "arm.h"
#include "arm_dpm.h"

#define ARM11_TAP_DEFAULT                       TAP_INVALID

#define CHECK_RETVAL(action)			\
	do {					\
		int __retval = (action);	\
		if (__retval != ERROR_OK) {	\
			LOG_DEBUG("error while calling \"%s\"",	\
				# action);     \
			return __retval;	\
		}				\
	} while (0)

/* bits from ARMv7 DIDR */
enum arm11_debug_version {
	ARM11_DEBUG_V6                  = 0x01,
	ARM11_DEBUG_V61                 = 0x02,
	ARM11_DEBUG_V7                  = 0x03,
	ARM11_DEBUG_V7_CP14             = 0x04,
};

struct arm11_common {
	struct arm arm;

	/** Debug module state. */
	struct arm_dpm dpm;
	struct arm11_sc7_action *bpwp_actions;
	unsigned bpwp_n;

	size_t brp;			/**< Number of Breakpoint Register Pairs from DIDR	*/
	size_t free_brps;		/**< Number of breakpoints allocated */

	uint32_t dscr;			/**< Last retrieved DSCR value. */

	uint32_t saved_rdtr;
	uint32_t saved_wdtr;

	bool is_rdtr_saved;
	bool is_wdtr_saved;

	bool simulate_reset_on_next_halt;	/**< Perform cleanups of the ARM state on next halt **/

	/* Per-core configurable options.
	 * NOTE that several of these boolean options should not exist
	 * once the relevant code is known to work correctly.
	 */
	bool memwrite_burst;
	bool memwrite_error_fatal;
	bool step_irq_enable;
	bool hardware_step;

	/** Configured Vector Catch Register settings. */
	uint32_t vcr;

	struct arm_jtag jtag_info;
};

static inline struct arm11_common *target_to_arm11(struct target *target)
{
	return container_of(target->arch_info, struct arm11_common, arm);
}

/**
 * ARM11 DBGTAP instructions
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.ddi0301f/I1006229.html
 */
enum arm11_instructions {
	ARM11_EXTEST    = 0x00,
	ARM11_SCAN_N    = 0x02,
	ARM11_RESTART   = 0x04,
	ARM11_HALT          = 0x08,
	ARM11_INTEST    = 0x0C,
	ARM11_ITRSEL    = 0x1D,
	ARM11_IDCODE    = 0x1E,
	ARM11_BYPASS    = 0x1F,
};

enum arm11_sc7 {
	ARM11_SC7_NULL                          = 0,
	ARM11_SC7_VCR                           = 7,
	ARM11_SC7_PC                            = 8,
	ARM11_SC7_BVR0                          = 64,
	ARM11_SC7_BCR0                          = 80,
	ARM11_SC7_WVR0                          = 96,
	ARM11_SC7_WCR0                          = 112,
};

#endif /* OPENOCD_TARGET_ARM11_H */
