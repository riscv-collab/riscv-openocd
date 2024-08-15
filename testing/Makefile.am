# SPDX-License-Identifier: GPL-2.0-or-later

if DUMMY
TESTS = \
	test-jtag-execute-command.cfg \
	test-irscan-command.cfg \
	test-drscan-command.cfg
endif

TEST_EXTENSIONS = .cfg
CFG_LOG_COMPILER = $(top_builddir)/src/openocd
AM_CFG_LOG_FLAGS = -f $(abs_srcdir)/utils.tcl -f
