#!/bin/bash

# Script to cross build the GNU/Linux version of OpenOCD on Debian.

# Prerequisites:
#
# sudo apt-get install git doxygen libtool autoconf automake texinfo texlive \
# autotools-dev pkg-config cmake libusb-1.0.0-dev libudev-dev \
# g++ libboost1.49-dev swig2.0 python2.7-dev libconfuse-dev

if [ -d /media/Work ]
then
  WORK="/media/Work/openocd"
else
  WORK=~/Work/openocd
fi

OUTFILE_VERSION="0.8.0"
OPENOCD_TARGET="linux"

INSTALL_ROOT="/opt/gnuarmeclipse"

# For updates, please check the corresponding pages

# http://www.intra2net.com/en/developer/libftdi/download.php
LIBFTDI="libftdi1-1.2"

# https://sourceforge.net/projects/libusb/files/libusb-compat-0.1/
LIBUSB0="libusb-compat-0.1.5"

# https://sourceforge.net/projects/libusb/files/libusb-1.0/
LIBUSB1="libusb-1.0.19"

# https://github.com/signal11/hidapi/downloads
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

if [ !  \( -f "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.a" -o \
           -f "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64/libftdi1.a" \)  ]
then
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"

  rm -rf "${WORK}/${LIBFTDI}/build/linux"
  mkdir -p "${WORK}/${LIBFTDI}/build/linux"
  cd "${WORK}/${LIBFTDI}/build/linux"
  cmake -DCMAKE_INSTALL_PREFIX="${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}" ../..
  make clean
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
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  rm -rf "${WORK}/${LIBUSB0}/build/linux"
  mkdir -p "${WORK}/${LIBUSB0}/build/linux"
  cd "${WORK}/${LIBUSB0}/build/linux"
  "${WORK}/${LIBUSB0}/configure" --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  make clean
  make 
  make install
fi

# http://www.libusb.info

if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1}.tar.bz2" ]
then
  mkdir -p ${OPENOCD_DOWNLOAD_FOLDER}
  cd ${OPENOCD_DOWNLOAD_FOLDER}
  wget http://sourceforge.net/projects/libusb/files/libusb-1.0/${LIBUSB1}/${LIBUSB1}.tar.bz2
fi

if [ ! -d "${WORK}/${LIBUSB1}" ]
then
  cd "${WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1}.tar.bz2"
fi

if [ ! -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.a" ]
then
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"

  rm -rf "${WORK}/${LIBUSB1}/build/linux"
  mkdir -p "${WORK}/${LIBUSB1}/build/linux"
  cd "${WORK}/${LIBUSB1}/build/linux"
  "${WORK}/${LIBUSB1}/configure" --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  make clean
  make 
  make install
fi

# http://www.signal11.us/oss/hidapi/

HIDAPI_ARCHIVE="${HIDAPI}.zip"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}" ]
then
  mkdir -p ${OPENOCD_DOWNLOAD_FOLDER}
  cd "${OPENOCD_DOWNLOAD_FOLDER}"
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
  make clean
  make LDFLAGS="-lpthread"
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

# On some machines libftdi ends in lib64, so we refer both lib & lib64

LIBFTDI_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/include/libftdi1" \
LIBUSB0_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/include" \
LIBUSB1_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/include/libusb-1.0" \
HIDAPI_CFLAGS="-I${WORK}/${HIDAPI}/hidapi" \
\
LIBFTDI_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib -L${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib -lftdi1" \
LIBUSB0_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib -lusb" \
LIBUSB1_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib -lusb-1.0" \
HIDAPI_LIBS="-L${WORK}/${HIDAPI}/linux -lhid" \
\
LDFLAGS='-Wl,-rpath=\$$ORIGIN -lpthread' \
LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib" \
\
"${OPENOCD_GIT_FOLDER}/configure" \
--prefix="${INSTALL_ROOT}/openocd"  \
--datarootdir="${INSTALL_ROOT}" \
--infodir="${INSTALL_ROOT}/openocd/info"  \
--localedir="${INSTALL_ROOT}/openocd/locale"  \
--mandir="${INSTALL_ROOT}/openocd/man"  \
--docdir="${INSTALL_ROOT}/openocd/doc"  \
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
make clean
make bindir="bin" pkgdatadir= all pdf html

if [ -f src/openocd ]
then
  strip src/openocd

  sudo rm -rf "${INSTALL_ROOT}/openocd"

  sudo make install install-pdf install-html install-man

  if [ -d "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64" ]
  then
    sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin"
  else
    sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin"
  fi
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2"
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libftdi1.so.2.2.0" "${INSTALL_ROOT}/openocd/bin/libftdi1.so"

  sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin"
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4"
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-0.1.so.4.4.4" "${INSTALL_ROOT}/openocd/bin/libusb.so"

  sudo /usr/bin/install -c -m 644 "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.so.0.1.0" "${INSTALL_ROOT}/openocd/bin"
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-1.0.so.0.1.0" "${INSTALL_ROOT}/openocd/bin/libusb-1.0.so.0"
  sudo ln -s "${INSTALL_ROOT}/openocd/bin/libusb-1.0.so.0.1.0" "${INSTALL_ROOT}/openocd/bin/libusb-1.0.so"

  readelf -d "${INSTALL_ROOT}/openocd/bin/openocd"

  echo
  "${INSTALL_ROOT}/openocd/bin/openocd" --version

  echo
  echo "Done."
fi

