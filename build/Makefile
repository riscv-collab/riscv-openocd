SYSROOT=/opt/winroot
PKG_CONFIG=x86_64-w64-mingw32-pkg-config
PKG_CONFIG_PATH=$(SYSROOT)/lib/pkgconfig
PKG_CONFIG_LIBDIR=$(SYSROOT)/lib/pkgconfig

.PHONY: dep all

all: $(SYSROOT)/openocd.zip

dep:
	sudo apt install binutils-mingw-w64-x86-64 g++-mingw-w64-x86-64 gcc-mingw-w64-base gcc-mingw-w64-x86-64 \
		gfortran-mingw-w64-x86-64 mingw-w64-common mingw-w64-x86-64-dev gdb-mingw-w64 gdb-mingw-w64-target \
		pkg-config mingw-w64-tools autotools-dev automake autoconf m4 libtool texinfo unzip cmake zip

libusb-1.0.23.tar.bz2:
	wget https://github.com/libusb/libusb/releases/download/v1.0.23/libusb-1.0.23.tar.bz2

libusb-compat-0.1.7.tar.bz2:
	wget https://github.com/libusb/libusb-compat-0.1/releases/download/v0.1.7/libusb-compat-0.1.7.tar.bz2

libftdi1-1.4.tar.bz2:
	wget https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2

hidapi-0.9.0.tar.gz:
	wget https://github.com/libusb/hidapi/archive/hidapi-0.9.0.tar.gz

openocd-0.10.0.zip:
	wget https://nchc.dl.sourceforge.net/project/openocd/openocd/0.10.0/openocd-0.10.0.zip

# libusb Download & Build
$(SYSROOT)/bin/libusb-1.0.dll: libusb-1.0.23.tar.bz2
	tar jxvf libusb-1.0.23.tar.bz2
	cd libusb-1.0.23 && ./configure --host=x86_64-w64-mingw32 --prefix=$(SYSROOT)
	cd libusb-1.0.23 && make -j4 && make install

# libusb-compat Download & Build
$(SYSROOT)/bin/libusb-0-1-4.dll: libusb-compat-0.1.7.tar.bz2
	tar jxvf libusb-compat-0.1.7.tar.bz2
	cd libusb-compat-0.1.7 && \
		PKG_CONFIG=x86_64-w64-mingw32-pkg-config PKG_CONFIG_PATH=$(SYSROOT)/lib/pkgconfig PKG_CONFIG_LIBDIR=$(SYSROOT)/lib/pkgconfig \
		./configure --host=x86_64-w64-mingw32 --prefix=$(SYSROOT)
	cd libusb-compat-0.1.7 && \
		PKG_CONFIG=x86_64-w64-mingw32-pkg-config PKG_CONFIG_PATH=$(SYSROOT)/lib/pkgconfig PKG_CONFIG_LIBDIR=$(SYSROOT)/lib/pkgconfig \
		make -j4 && make install

# libftdi Download & Build
$(SYSROOT)/bin/libftdi1.dll: libftdi1-1.4.tar.bz2
	tar jxvf libftdi1-1.4.tar.bz2
	mkdir -p libftdi1-1.4/build
	cd libftdi1-1.4/build && cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-x86_64-w64-mingw32.cmake -DCMAKE_INSTALL_PREFIX=$(SYSROOT) \
		-DLIBUSB_INCLUDE_DIR=$(SYSROOT)/include/libusb-1.0 \
		-DLIBUSB_LIBRARIES=$(SYSROOT)/lib/libusb-1.0.a ..
	cd libftdi1-1.4/build && make -j4 && make install

# hidapi Download & Build
$(SYSROOT)/bin/libhidapi-0.dll: hidapi-0.9.0.tar.gz
	tar zxvf hidapi-0.9.0.tar.gz
	cd hidapi-hidapi-0.9.0 && ./bootstrap
	cd hidapi-hidapi-0.9.0 && ./configure --host=x86_64-w64-mingw32 --prefix=$(SYSROOT)
	cd hidapi-hidapi-0.9.0 && make -j4 && make install

# jimtcl Download & Build
$(SYSROOT)/bin/jimsh.exe: openocd-0.10.0.zip
	unzip openocd-0.10.0.zip
	cd openocd-0.10.0/jimtcl && ./configure --host=x86_64-w64-mingw32 --prefix=$(SYSROOT)
	cd openocd-0.10.0/jimtcl && make -j4 && make install

# Build openocd
$(SYSROOT)/bin/openocd.exe:	$(SYSROOT)/bin/libusb-1.0.dll $(SYSROOT)/bin/libusb-0-1-4.dll $(SYSROOT)/bin/libftdi1.dll $(SYSROOT)/bin/libhidapi-0.dll $(SYSROOT)/bin/jimsh.exe
	cd .. && SKIP_SUBMODULE=1 ./bootstrap
	mkdir -p build
	cd build && \
	  PKG_CONFIG=x86_64-w64-mingw32-pkg-config PKG_CONFIG_PATH=$(SYSROOT)/lib/pkgconfig PKG_CONFIG_LIBDIR=$(SYSROOT)/lib/pkgconfig \
	  CFLAGS=-I$(SYSROOT)/include ../../configure --host=x86_64-w64-mingw32 --prefix=$(SYSROOT) --disable-internal-jimtcl --disable-internal-libjaylink \
		--enable-jlink=no --disable-doxygen-html --enable-doxygen-pdf=no --enable-rshim=no --enable-stlink=yes \
		--enable-ti-icdi=no --enable-ulink=no --enable-usb-blaster-2=no --enable-ft232r=no --enable-vsllink=no --enable-xds110=no \
		--enable-osbdm=no --enable-opendous=no --enable-aice=no --enable-usbprog=no --enable-rlink=no --enable-armjtagew=yes \
		--enable-cmsis-dap=yes --enable-nulink=no --enable-kitprog=no --enable-usb-blaster=no \
		--enable-openjtag=no --enable-linuxgpiod=no --enable-jlink=no --enable-parport=no --enable-amtjtagaccel=no \
		--enable-zy1000-master=no --enable-zy1000=no \
		--enable-bcm2835gpio=no --enable-imx_gpio=no --enable-ep93xx=no --enable-at91rm9200=no --enable-presto=no \
		--enable-gw16012=no  --enable-buspirate=no --enable-sysfsgpio=no --enable-xlnx-pcie-xvc=no --enable-remote-bitbang=no \
		--enable-branding='S-Link Phoenix SDF'
	cd build && \
	  PKG_CONFIG=x86_64-w64-mingw32-pkg-config PKG_CONFIG_PATH=$(SYSROOT)/lib/pkgconfig PKG_CONFIG_LIBDIR=$(SYSROOT)/lib/pkgconfig \
	  make -j4 && make install

$(SYSROOT)/openocd.zip: $(SYSROOT)/bin/openocd.exe
	cd $(SYSROOT) && zip -r openocd.zip bin/lib* bin/openocd.exe share/info share/man share/openocd
