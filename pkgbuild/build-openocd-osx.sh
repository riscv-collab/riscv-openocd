#!/bin/bash

# Script to build the OS X version of OpenOCD.

# Prerequisites:
#
# export PATH=/opt/local/bin:/opt/local/sbin:$PATH
#
# sudo port install libtool automake autoconf pkgconfig libusb [libusb-compat]
# sudo port install texinfo texlive

# DEBUG="y"

WORK=/Users/$(whoami)/Work/openocd
mkdir -p "${WORK}"

OUTFILE_VERSION="0.8.0"
OPENOCD_TARGET="osx"

OPENOCD_GIT_FOLDER="${WORK}/gnuarmeclipse-openocd.git"
OPENOCD_DOWNLOAD_FOLDER="${WORK}/download"
OPENOCD_BUILD_FOLDER="${WORK}/build/${OPENOCD_TARGET}"
OPENOCD_INSTALL_FOLDER="${WORK}/install/${OPENOCD_TARGET}"
OPENOCD_PKG_FOLDER="${WORK}/pkg_root"

HIDAPI_FOLDER="hidapi-0.7.0"
HIDAPI_ARCHIVE="${HIDAPI_FOLDER}.zip"

PKGBUILD_RELATIVE_INSTALL="Applications/GNU ARM Eclipse/OpenOCD"
INSTALL_FOLDER=/${PKGBUILD_RELATIVE_INSTALL}

# http://www.signal11.us/oss/hidapi/
# https://github.com/downloads/signal11/hidapi/hidapi-0.7.0.zip

if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  curl -L https://github.com/downloads/signal11/hidapi/${HIDAPI_ARCHIVE} -o ${HIDAPI_ARCHIVE}
fi

if [ ! -f "${WORK}/${HIDAPI_FOLDER}/mac/libhid.a" ]
then
  cd "${WORK}"
  unzip "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}"

  cd "${WORK}/${HIDAPI_FOLDER}/mac"
  make

  # Create the library, the make procedure cretes only the object file.
  ar -r  "libhid.a"  hid.o
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

export HIDAPI_CFLAGS="-I${WORK}/${HIDAPI_FOLDER}/hidapi"
export HIDAPI_LIBS="-L${WORK}/${HIDAPI_FOLDER}/mac -lhid"
if [ "${DEBUG}" == "y" ]
then
  export CFLAGS="-g"
fi
export LIBS="-framework IOKit -framework CoreFoundation"

cd "${OPENOCD_BUILD_FOLDER}"
LDFLAGS=-L/opt/local/lib CPPFLAGS=-I/opt/local/include \
"${OPENOCD_GIT_FOLDER}/configure" \
--prefix=""  \
--enable-aice \
--disable-amtjtagaccel \
--enable-armjtagew \
--enable-cmsis-dap \
--enable-ftdi \
--disable-gw16012 \
--enable-jlink \
--disable-jtag_vpi \
--enable-opendous \
--enable-openjtag_ftdi \
--enable-osbdm \
--enable-legacy-ft2232_libftdi \
--disable-parport \
--disable-parport-ppdev \
--disable-parport-giveio \
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
make bindir="bin" pkgdatadir="" all pdf html
if [ "${DEBUG}" != "y" ]
then
  strip src/openocd
fi

mkdir -p ${OPENOCD_PKG_FOLDER}/bin
mkdir -p ${OPENOCD_PKG_FOLDER}/scripts
mkdir -p ${OPENOCD_PKG_FOLDER}/doc
mkdir -p ${OPENOCD_PKG_FOLDER}/license/openocd
mkdir -p ${OPENOCD_PKG_FOLDER}/license/hidapi
mkdir -p ${OPENOCD_PKG_FOLDER}/info

cp "${OPENOCD_BUILD_FOLDER}/src/openocd" "${OPENOCD_PKG_FOLDER}/bin"

cp -r "${OPENOCD_GIT_FOLDER}/tcl/"* "${OPENOCD_PKG_FOLDER}/scripts"

cp "${OPENOCD_BUILD_FOLDER}/doc/openocd.pdf" "${OPENOCD_PKG_FOLDER}/doc"
cp -r "${OPENOCD_BUILD_FOLDER}/doc/openocd.html" "${OPENOCD_PKG_FOLDER}/doc"

cp -r "${OPENOCD_GIT_FOLDER}/pkgbuild/"* "${OPENOCD_PKG_FOLDER}/info"

cp "${OPENOCD_GIT_FOLDER}/AUTHORS" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/COPYING" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/"NEW* "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/README" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/README.OSX" "${OPENOCD_PKG_FOLDER}/license/openocd"

cp "${WORK}/hidapi-0.7.0/AUTHORS.txt" "${OPENOCD_PKG_FOLDER}/license/hidapi"
cp "${WORK}/hidapi-0.7.0/"LICENSE* "${OPENOCD_PKG_FOLDER}/license/hidapi"
cp "${WORK}/hidapi-0.7.0/README.txt" "${OPENOCD_PKG_FOLDER}/license/hidapi"


mkdir -p "${WORK}/output"

NDATE=$(date -u +%Y%m%d%H%M)
INSTALLER=${WORK}/output/gnuarmeclipse-openocd-osx-${OUTFILE_VERSION}-${NDATE}.pkg

cd "${WORK}"

pkgbuild --identifier ilg.gnuarmeclipse.openocd \
--root "${OPENOCD_PKG_FOLDER}" \
--version "${OUTFILE_VERSION}" \
--install-location "${PKGBUILD_RELATIVE_INSTALL}" \
"${INSTALLER}"
