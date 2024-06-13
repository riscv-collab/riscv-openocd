# SPDX-License-Identifier: GPL-2.0-or-later

if ELCT_SUPPORT
noinst_LTLIBRARIES += %D%/libelct.la
endif

%C%_libelct_la_CPPFLAGS = $(AM_CPPFLAGS)

%C%_libelct_la_SOURCES = \
	%D%/common_hw.h \
	%D%/common_hw.c \
	%D%/elct_cmd.h \
	%D%/elct_cmd.c \
	%D%/axi_hw.h \
	%D%/axi_hw.c \
	%D%/axi_cmd.h \
	%D%/axi_cmd.c \
	%D%/bps_cmd.h \
	%D%/bps_cmd.c \
	%D%/ila_hw.h \
	%D%/ila_hw.c \
	%D%/ila_cmd.h \
	%D%/ila_cmd.c \
	%D%/ila_vcd.h \
	%D%/ila_vcd.c

%C%_libelct_la_CPPFLAGS += -DELCT_DDR_ILA_WORKAROUND

if SYNTACORE_RELEASE
%C%_libelct_la_CPPFLAGS += -DRELSTR=\"-`head -1 $(top_srcdir)/__sc_version.txt`\"
else
if RELEASE
%C%_libelct_la_CPPFLAGS += -DRELSTR=\"\"
else
%C%_libelct_la_CPPFLAGS += -DRELSTR=\"`$(top_srcdir)/guess-rev.sh $(top_srcdir)`\"
endif
endif

if HAVE_JANSSON
%C%_libelct_la_CPPFLAGS += $(JANSSON_CFLAGS)
%C%_libelct_la_LIBADD = $(JANSSON_LIBS)
%C%_libelct_la_SOURCES += %D%/ila_json.h %D%/ila_json.c
endif
