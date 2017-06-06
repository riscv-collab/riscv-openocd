# The default target
.PHONY: all
all:

BINDIR := bin
OBJDIR := obj
SRCDIR := src

UBUNTU ?= x86_64-linux-gnu
WINDOWS ?= i686-w64-mingw32

# FIXME: Detect the native platform
NATIVE ?= $(UBUNTU)

OBJ_NATIVE := $(OBJDIR)/$(NATIVE)
OBJ_UBUNTU := $(OBJDIR)/$(UBUNTU)
OBJ_WIN64  := $(OBJDIR)/$(WINDOWS)

SRC_RGT := $(SRCDIR)/riscv-gnu-toolchain
SRC_EXPAT := $(SRCDIR)/libexpat/expat
SRC_ZLIB := $(SRCDIR)/zlib

# The version that will be appended to the various tool builds.
VERSION ?= $(shell cd $(SRC_RGT); git describe --tags | sed s/^v//g)

# The actual output of this repository is a set of tarballs.
.PHONY: windows
windows: $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-$(WINDOWS).tar.gz
windows: $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-$(WINDOWS).src.tar.gz
.PHONY: ubuntu
ubuntu: $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-$(UBUNTU).tar.gz
ubuntu: $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-$(UBUNTU).src.tar.gz

# FIXME: Check to see if the Windows tools should be built based on the
# presence of the Windows cross compiler.
all: windows
all: ubuntu

# Some special riscv-gnu-toolchain configure flags for specific targets.
x86_64-linux-gnu-rgt-configue    :=
i686-w64-mingw32-rgt-configure   := --without-system-zlib

# There's enough % rules that make starts blowing intermediate files away.
.SECONDARY:

# Builds riscv-gnu-toolchain for various targets.
$(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-%.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-%.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/install -c riscv64-unknown-elf-gcc-$(VERSION)-$($@_TARGET) | gzip > $(abspath $@)

$(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-%.src.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv64-unknown-elf-gcc-$(VERSION)-%.src.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/build -c . | gzip > $(abspath $@)

$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp: \
		$(OBJDIR)/%/stamps/expat/install.stamp \
		$(OBJDIR)/%/build/riscv-gnu-toolchain/stamp
	$(eval $@_TARGET := $(patsubst $(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp,%,$@))
	$(eval $@_BUILD := $(patsubst %/stamps/riscv-gnu-toolchain/install.stamp,%/build/riscv-gnu-toolchain,$@))
	$(eval $@_INSTALL := $(patsubst %/stamps/riscv-gnu-toolchain/install.stamp,%/install/riscv64-unknown-elf-gcc-$(VERSION)-$($@_TARGET),$@))
	mkdir -p $($@_BUILD)
	cd $($@_BUILD); ./configure --prefix=$(abspath $($@_INSTALL)) --with-host=$($@_TARGET) $($($@_TARGET)-rgt-configure)
	$(MAKE) PATH="$(abspath $(OBJ_NATIVE)/install/riscv64-unknown-elf-gcc-$(VERSION)-$(NATIVE)/bin:$(PATH))" -C $($@_BUILD)
	mkdir -p $(dir $@)
	date > $@

$(OBJDIR)/%/build/riscv-gnu-toolchain/stamp:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)
	cp -r $(SRC_RGT)/* $(dir $@)
	cd $(dir $@)/riscv-gcc; ./contrib/download_prerequisites
	date > $@

# The Windows build requires the native toolchain.  The dependency is enforced
# here, PATH allows the tools to get access.
$(OBJ_WIN64)/stamps/riscv-gnu-toolchain/install.stamp: \
	$(OBJ_NATIVE)/stamps/riscv-gnu-toolchain/install.stamp

# OpenOCD requires a GDB that's been build with expat support so it can read
# the target XML files.
$(OBJDIR)/%/stamps/expat/install.stamp: \
		$(OBJDIR)/%/build/expat/configure
	$(eval $@_TARGET := $(patsubst $(OBJDIR)/%/stamps/expat/install.stamp,%,$@))
	$(eval $@_BUILD := $(patsubst %/stamps/expat/install.stamp,%/build/expat,$@))
	$(eval $@_INSTALL := $(patsubst %/stamps/expat/install.stamp,%/install,$@))
	mkdir -p $($@_BUILD)
	cd $($@_BUILD); ./configure --prefix=$(abspath $($@_INSTALL)) --host=$($@_TARGET)
	$(MAKE) -C $($@_BUILD) buildlib
	$(MAKE) -C $($@_BUILD) installlib
	mkdir -p $(dir $@)
	date > $@

$(OBJDIR)/%/build/expat/configure:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)
	cp -r $(SRC_EXPAT)/* $(dir $@)
	cd $(dir $@); ./buildconf.sh
	touch -c $@

# Targets that don't build anything
.PHONY: clean
clean::
	rm -rf $(OBJDIR) $(BINDIR)
