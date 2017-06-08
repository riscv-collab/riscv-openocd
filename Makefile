# The default target
.PHONY: all
all:

BINDIR := bin
OBJDIR := obj
SRCDIR := src

UBUNTU ?= x86_64-linux-gnu
WIN64  ?= x86_64-w64-mingw32

# FIXME: Detect the native platform
NATIVE ?= $(UBUNTU)

OBJ_NATIVE := $(OBJDIR)/$(NATIVE)
OBJ_UBUNTU := $(OBJDIR)/$(UBUNTU)
OBJ_WIN64  := $(OBJDIR)/$(WIN64)

SRC_RGT := $(SRCDIR)/riscv-gnu-toolchain
SRC_ROCD := $(SRCDIR)/riscv-openocd
SRC_EXPAT := $(SRCDIR)/libexpat/expat
SRC_ZLIB := $(SRCDIR)/zlib
SRC_LIBUSB := $(SRCDIR)/libusb

# The version that will be appended to the various tool builds.
RGT_VERSION ?= $(shell cd $(SRC_RGT); git describe --tags | sed s/^v//g)
ROCD_VERSION ?= $(shell cd $(SRC_ROCD); git describe --tags | sed s/^v//g)

# The actual output of this repository is a set of tarballs.
.PHONY: windows windows-openocd windows-gcc
win64: win64-openocd win64-gcc
win64-gcc: $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(WIN64).zip
win64-gcc: $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(WIN64).tar.gz
win64-gcc: $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(WIN64).src.tar.gz
win64-openocd: $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-$(WIN64).zip
win64-openocd: $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-$(WIN64).tar.gz
win64-openocd: $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-$(WIN64).src.tar.gz
.PHONY: ubuntu ubuntu-gcc ubuntu-openocd
ubuntu: ubuntu-gcc ubuntu-openocd
ubuntu-gcc: $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(UBUNTU).tar.gz
ubuntu-gcc: $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(UBUNTU).src.tar.gz
ubuntu-openocd: $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-$(UBUNTU).tar.gz
ubuntu-openocd: $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-$(UBUNTU).src.tar.gz

# FIXME: Check to see if the Windows tools should be built based on the
# presence of the Windows cross compiler.
all: win64
all: ubuntu

# Some special riscv-gnu-toolchain configure flags for specific targets.
$(WIN64)-rgt-configure   := --without-system-zlib
$(WIN64)-rocd-vars       := LIBUSB1_LIBS="-L$(abspath $(OBJ_WIN64)/install/riscv-openocd-$(ROCD_VERSION)-$(WIN64))/lib" CFLAGS="-O2"

# There's enough % rules that make starts blowing intermediate files away.
.SECONDARY:

# Builds riscv-gnu-toolchain for various targets.
$(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.zip: \
		$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.zip,%,$@))
	mkdir -p $(dir $@)
	cd $(OBJDIR)/$($@_TARGET)/install; zip -r $(abspath $@) riscv64-unknown-elf-gcc-$(RGT_VERSION)-$($@_TARGET)

$(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/install -c riscv64-unknown-elf-gcc-$(RGT_VERSION)-$($@_TARGET) | gzip > $(abspath $@)

$(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.src.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv64-unknown-elf-gcc-$(RGT_VERSION)-%.src.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/build -c . | gzip > $(abspath $@)

$(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp: \
		$(OBJDIR)/%/stamps/expat/install.stamp \
		$(OBJDIR)/%/build/riscv-gnu-toolchain/stamp
	$(eval $@_TARGET := $(patsubst $(OBJDIR)/%/stamps/riscv-gnu-toolchain/install.stamp,%,$@))
	$(eval $@_BUILD := $(patsubst %/stamps/riscv-gnu-toolchain/install.stamp,%/build/riscv-gnu-toolchain,$@))
	$(eval $@_INSTALL := $(patsubst %/stamps/riscv-gnu-toolchain/install.stamp,%/install/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$($@_TARGET),$@))
	mkdir -p $($@_BUILD)
	cd $($@_BUILD); ./configure --prefix=$(abspath $($@_INSTALL)) --with-host=$($@_TARGET) $($($@_TARGET)-rgt-configure) --enable-multilib
	$(MAKE) PATH="$(abspath $(OBJ_NATIVE)/install/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$(NATIVE)/bin:$(PATH))" -C $($@_BUILD)
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
	$(eval $@_INSTALL := $(patsubst %/stamps/expat/install.stamp,%/install/riscv64-unknown-elf-gcc-$(RGT_VERSION)-$($@_TARGET),$@))
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

# The OpenOCD builds go here
$(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.zip: \
		$(OBJDIR)/%/stamps/riscv-openocd/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.zip,%,$@))
	mkdir -p $(dir $@)
	cd $(OBJDIR)/$($@_TARGET)/install; zip -r $(abspath $@) riscv-openocd-$(ROCD_VERSION)-$($@_TARGET)

$(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-openocd/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/install -c riscv-openocd-$(ROCD_VERSION)-$($@_TARGET) | gzip > $(abspath $@)

$(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.src.tar.gz: \
		$(OBJDIR)/%/stamps/riscv-openocd/install.stamp
	$(eval $@_TARGET := $(patsubst $(BINDIR)/riscv-openocd-$(ROCD_VERSION)-%.src.tar.gz,%,$@))
	mkdir -p $(dir $@)
	tar -C $(OBJDIR)/$($@_TARGET)/build -c . | gzip > $(abspath $@)

$(OBJDIR)/%/stamps/riscv-openocd/install.stamp: \
		$(OBJDIR)/%/stamps/libusb/install.stamp \
		$(OBJDIR)/%/build/riscv-openocd/configure
	$(eval $@_TARGET := $(patsubst $(OBJDIR)/%/stamps/riscv-openocd/install.stamp,%,$@))
	$(eval $@_BUILD := $(patsubst %/stamps/riscv-openocd/install.stamp,%/build/riscv-openocd,$@))
	$(eval $@_INSTALL := $(patsubst %/stamps/riscv-openocd/install.stamp,%/install/riscv-openocd-$(ROCD_VERSION)-$($@_TARGET),$@))
	mkdir -p $($@_BUILD)
	cd $($@_BUILD); $($($@_TARGET)-rocd-vars) ./configure --prefix=$(abspath $($@_INSTALL)) --host=$($@_TARGET) --enable-remote-bitbang --disable-werror --enable-ftdi
	$(MAKE) -C $($@_BUILD)
	$(MAKE) -C $($@_BUILD) install
	mkdir -p $(dir $@)
	date > $@

$(OBJDIR)/%/build/riscv-openocd/configure:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)
	cp -r $(SRC_ROCD)/* $(dir $@)
	cd $(dir $@); autoreconf -i
	touch -c $@

# Use the host libusb unless we expect there to be none
$(OBJ_WIN64)/stamps/riscv-openocd/install.stamp: \
		$(OBJ_WIN64)/stamps/libusb/install.stamp

# OpenOCD needs libusb
$(OBJDIR)/%/stamps/libusb/install.stamp: \
		$(OBJDIR)/%/build/libusb/configure
	$(eval $@_TARGET := $(patsubst $(OBJDIR)/%/stamps/libusb/install.stamp,%,$@))
	$(eval $@_BUILD := $(patsubst %/stamps/libusb/install.stamp,%/build/libusb,$@))
	$(eval $@_INSTALL := $(patsubst %/stamps/libusb/install.stamp,%/install/riscv-openocd-$(ROCD_VERSION)-$($@_TARGET),$@))
	mkdir -p $($@_BUILD)
	cd $($@_BUILD); ./configure --prefix=$(abspath $($@_INSTALL)) --host=$($@_TARGET) --disable-udev
	$(MAKE) -C $($@_BUILD)
	$(MAKE) -C $($@_BUILD) install
	mkdir -p $(dir $@)
	date > $@

$(OBJDIR)/%/build/libusb/configure:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)
	cp -r $(SRC_LIBUSB)/* $(dir $@)
	cd $(dir $@); ./autogen.sh --disable-udev
	touch -c $@

# Targets that don't build anything
.PHONY: clean
clean::
	rm -rf $(OBJDIR) $(BINDIR)
