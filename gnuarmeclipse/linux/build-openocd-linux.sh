#!/bin/bash

# Script to cross build the GNU/Linux version of OpenOCD on Debian.

# Prerequisites:
#
# sudo apt-get install git doxygen libtool autoconf automake texinfo texlive \
# autotools-dev pkg-config  mingw-w64 mingw-w64-tools mingw-w64-dev

WORK="/media/Work/openocd"

OUTFILE_VERSION="0.8.0"
OPENOCD_TARGET="linux"

INSTALL_ROOT="/opt/gnuarmeclipse"

LIBFTDI="libftdi1-1.2"
LIBUSB0="libusb-compat-0.1.4"
HIDAPI="hidapi-0.7.0"

OPENOCD_GIT_FOLDER="${WORK}/gnuarmeclipse-openocd.git"
OPENOCD_DOWNLOAD_FOLDER="${WORK}/download"
OPENOCD_BUILD_FOLDER="${WORK}/build/${OPENOCD_TARGET}"
OPENOCD_INSTALL_FOLDER="${WORK}/install/${OPENOCD_TARGET}"

mkdir -p "${WORK}"

# http://www.intra2net.com/en/developer/libftdi

if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI}.tar.bz2" ]
then
  mkdir -p ${OPENOCD_DOWNLOAD_FOLDER}
  cd ${OPENOCD_DOWNLOAD_FOLDER}
  wget http://www.intra2net.com/en/developer/libftdi/download/${LIBFTDI}.tar.bz2
fi

if [ ! -d "${WORK}/${LIBFTDI}" ]
then
  cd "${WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI}.tar.bz2"
fi

if [ ! -f "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.a" ]
then
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"

  mkdir -p "${WORK}/${LIBFTDI}/build/linux"
  cd "${WORK}/${LIBFTDI}/build/linux"
  cmake -DCMAKE_INSTALL_PREFIX="${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}" ../..
  make 
  make install
fi

# http://www.libusb.org

if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0}.tar.bz2" ]
then
  mkdir -p ${OPENOCD_DOWNLOAD_FOLDER}
  cd ${OPENOCD_DOWNLOAD_FOLDER}
  wget http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/${LIBUSB0}/${LIBUSB0}.tar.bz2
fi

if [ ! -d "${WORK}/${LIBUSB0}" ]
then
  cd "${WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0}.tar.bz2"
fi

if [ ! -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb.a" ]
then
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  mkdir -p "${WORK}/${LIBUSB0}/build/linux"
  cd "${WORK}/${LIBUSB0}/build/linux"
  "${WORK}/${LIBUSB0}/configure" --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  make 
  make install
fi

# http://www.signal11.us/oss/hidapi/

HIDAPI_ARCHIVE="${HIDAPI}.zip"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}" ]
then
  mkdir -p ${OPENOCD_DOWNLOAD_FOLDER}
  cd "${OPENOCD_DOWNLOAD_FOLDER}"
  HIDAPI_ARCHIVE="${HIDAPI}.zip"
  wget https://github.com/downloads/signal11/hidapi/${HIDAPI_ARCHIVE}
fi

if [ ! -d "${WORK}/${HIDAPI}" ]
then
  cd "${WORK}"
  unzip "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}"
fi

if [ ! -f "${WORK}/${HIDAPI}/libhid.a" ]
then
  cd "${WORK}/${HIDAPI}/linux"
  make
  ar -r  "libhid.a"  hid-libusb.o
fi

if [ ! -d "${OPENOCD_GIT_FOLDER}" ]
then
  cd "${WORK}"

  if [ "$(whoami)" == "ilg" ]
  then
    git clone ssh://ilg-ul@git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  else
    git clone http://git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  fi

  # Change to the gnuarmeclipse branch.
  cd "${OPENOCD_GIT_FOLDER}"
  git checkout gnuarmeclipse

  # Prepare autotools.
  cd "${OPENOCD_GIT_FOLDER}"
  ./bootstrap
fi


mkdir -p "${OPENOCD_BUILD_FOLDER}"

if [ -f "${OPENOCD_BUILD_FOLDER}/config.h" ]
then
  cd "${OPENOCD_BUILD_FOLDER}"
  make distclean
fi

cd "${OPENOCD_BUILD_FOLDER}"
LIBFTDI_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/include/libftdi1" \
LIBUSB0_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/include" \
LIBFTDI_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib -lftdi1" \
LIBUSB0_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib -lusb" \
HIDAPI_CFLAGS="-I${WORK}/${HIDAPI}/hidapi" \
HIDAPI_LIBS="-L${WORK}/${HIDAPI}/linux -lhid" \
LDFLAGS='-Wl,-rpath=\$$ORIGIN' \
LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib":"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib" \
"${OPENOCD_GIT_FOLDER}/configure" \
--prefix="${INSTALL_ROOT}/openocd"  \
--datarootdir="${INSTALL_ROOT}" \
--infodir="${INSTALL_ROOT}/openocd/info"  \
--localedir="${INSTALL_ROOT}/openocd/locale"  \
--mandir="${INSTALL_ROOT}/openocd/man"  \
--enable-aice \
--enable-amtjtagaccel \
--enable-armjtagew \
--enable-cmsis-dap \
--enable-ftdi \
--enable-gw16012 \
--enable-jlink \
--enable-jtag_vpi \
--enable-opendous \
--enable-openjtag_ftdi \
--enable-osbdm \
--enable-legacy-ft2232_libftdi \
--enable-parport \
--disable-parport-ppdev \
--enable-parport-giveio \
--enable-presto_libftdi \
--enable-remote-bitbang \
--enable-rlink \
--enable-stlink \
--enable-ti-icdi \
--enable-ulink \
--enable-usb-blaster-2 \
--enable-usb_blaster_libftdi \
--enable-usbprog \
--enable-vsllink

cd "${OPENOCD_BUILD_FOLDER}"
make bindir="bin" pkgdatadir= all pdf html
strip src/openocd

sudo make install

sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin"
sudo ln -s "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2"
sudo ln -s "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin/libftdi1.so"
sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin"
sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4"
sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin/libusb.so"


