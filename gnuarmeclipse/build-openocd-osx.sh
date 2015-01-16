#!/bin/bash

# Script to build the OS X version of OpenOCD. It produces an install package
# that expands in "/Applications/GNU ARM Eclipse/OpenOCD".

# Prerequisites:
#
# MacPorts with the following ports installed:
#
# sudo port install libtool automake autoconf pkgconfig wget
# sudo port install cmake boost libconfuse swig-python
# sudo port install texinfo texlive

# ----- Externally configurable variables -----

# Define it externally to "y"
DEBUG=${DEBUG:-"n"}

# The folder where the entire build procedure will run.
if [ -d /media/Work ]
then
  OPENOCD_WORK=${OPENOCD_WORK:-"/media/Work/openocd"}
else
  OPENOCD_WORK=${OPENOCD_WORK:-~/Work/openocd}
fi

INSTALL_FOLDER=${INSTALL_FOLDER:-"/Applications/GNU ARM Eclipse/OpenOCD"}

NDATE=${NDATE:-$(date -u +%Y%m%d%H%M)}

export PATH=/opt/local/bin:/opt/local/sbin:$PATH
port version >/dev/null
if [ $? != 0 ]
then
  echo "Mandatory MacPorts not found, exit."
  exit 1
fi

# ----- Local variables -----

OUTFILE_VERSION="0.8.0"

# For updates, please check the corresponding pages

# http://www.intra2net.com/en/developer/libftdi/download.php
LIBFTDI="libftdi1-1.2"

# https://sourceforge.net/projects/libusb/files/libusb-compat-0.1/
LIBUSB0="libusb-compat-0.1.5"

# https://sourceforge.net/projects/libusb/files/libusb-1.0/
LIBUSB1="libusb-1.0.19"

# https://github.com/signal11/hidapi/downloads
HIDAPI="hidapi-0.7.0"

OPENOCD_TARGET="osx"
HIDAPI_TARGET="mac"
HIDAPI_OBJECT="hid.o"

OPENOCD_GIT_FOLDER="${OPENOCD_WORK}/gnuarmeclipse-openocd.git"
OPENOCD_DOWNLOAD_FOLDER="${OPENOCD_WORK}/download"
OPENOCD_BUILD_FOLDER="${OPENOCD_WORK}/build/${OPENOCD_TARGET}"
OPENOCD_INSTALL_FOLDER="${OPENOCD_WORK}/install/${OPENOCD_TARGET}"

OPENOCD_PKG_FOLDER="${OPENOCD_WORK}/pkg_root"

WGET="wget"
WGET_OUT="-O"

if [ $# > 0 ]
then
  if [ $1 == "clean" ]
  then
    rm -rf "${OPENOCD_BUILD_FOLDER}"
    rm -rf "${OPENOCD_INSTALL_FOLDER}"
    rm -rf "${OPENOCD_PKG_FOLDER}"
    rm -rf "${OPENOCD_WORK}/${LIBFTDI}"
    rm -rf "${OPENOCD_WORK}/${LIBUSB0}"
    rm -rf "${OPENOCD_WORK}/${LIBUSB1}"
    rm -rf "${OPENOCD_WORK}/${HIDAPI}"
  fi
fi

# Create the work folder.
mkdir -p "${OPENOCD_WORK}"

# http://www.libusb.info

# Download the new USB library.
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1}.tar.bz2" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" http://sourceforge.net/projects/libusb/files/libusb-1.0/${LIBUSB1}/${LIBUSB1}.tar.bz2 \
  "${WGET_OUT}" "${LIBUSB1}.tar.bz2"
fi

# Unpack the new USB library.
if [ ! -d "${OPENOCD_WORK}/${LIBUSB1}" ]
then
  cd "${OPENOCD_WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1}.tar.bz2"
fi

# Build and install the new USB library.
if [ ! -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.a" ]
then
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"

  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"
  cd "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"

  CFLAGS="-Wno-non-literal-null-conversion" \
  "${OPENOCD_WORK}/${LIBUSB1}/configure" \
  --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  make clean install
fi

# http://www.libusb.org

# Download the old USB library.
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0}.tar.bz2" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/${LIBUSB0}/${LIBUSB0}.tar.bz2 \
  "${WGET_OUT}" "${LIBUSB0}.tar.bz2"
fi

# Unpack the old USB library.
if [ ! -d "${OPENOCD_WORK}/${LIBUSB0}" ]
then
  cd "${OPENOCD_WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0}.tar.bz2"
fi

# Build and install the old USB library.
if [ ! -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb.a" ]
then
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"

  # Configure
  PKG_CONFIG_PATH="${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":"${PKG_CONFIG_PATH}" \
  \
  "${OPENOCD_WORK}/${LIBUSB0}/configure" \
  --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  # Build
  make clean install
fi

# http://www.intra2net.com/en/developer/libftdi

# Download the FTDI library.
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI}.tar.bz2" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" http://www.intra2net.com/en/developer/libftdi/download/${LIBFTDI}.tar.bz2 \
  "${WGET_OUT}" "${LIBFTDI}.tar.bz2"
fi

# Unpack the FTDI library.
if [ ! -d "${OPENOCD_WORK}/${LIBFTDI}" ]
then
  cd "${OPENOCD_WORK}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI}.tar.bz2"
fi

# Build and install the FTDI library.
if [ !  \( -f "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.a" -o \
           -f "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64/libftdi1.a" \)  ]
then
  rm -rf "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"

  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"

  # Configure
  PKG_CONFIG_PATH="${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":"${PKG_CONFIG_PATH}" \
  \
  cmake \
  -DCMAKE_INSTALL_PREFIX="${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}" \
  "${OPENOCD_WORK}/${LIBFTDI}"

  # Build
  make clean install
fi

# http://www.signal11.us/oss/hidapi/

# Download the HDI library.
HIDAPI_ARCHIVE="${HIDAPI}.zip"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" https://github.com/downloads/signal11/hidapi/${HIDAPI_ARCHIVE} \
  "${WGET_OUT}" "${HIDAPI_ARCHIVE}"
fi

# Unpack the HDI library.
if [ ! -d "${OPENOCD_WORK}/${HIDAPI}" ]
then
  cd "${OPENOCD_WORK}"
  unzip "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}"
fi

# Build the new HDI library.
if [ ! -f "${OPENOCD_WORK}/${HIDAPI}/${HIDAPI_TARGET}/${HIDAPI_OBJECT}" ]
then
  cd "${OPENOCD_WORK}/${HIDAPI}/${HIDAPI_TARGET}"
  make clean
  make LDFLAGS="-lpthread"

  # Make just compiles the file. Create the archive.
  # No dynamic/shared libs involved.
  ar -r  "libhid.a" "${HIDAPI_OBJECT}"
fi

# Get the GNU ARM Eclipse OpenOCD git repository.
if [ ! -d "${OPENOCD_GIT_FOLDER}" ]
then
  cd "${OPENOCD_WORK}"

  if [ "$(whoami)" == "ilg" ]
  then
  # ilg has full access to the repo.
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

mkdir -p "${OPENOCD_BUILD_FOLDER}/openocd"

# On subsequent runs, clear it to always force a configure.
if [ -f "${OPENOCD_BUILD_FOLDER}/openocd/config.h" ]
then
  cd "${OPENOCD_BUILD_FOLDER}/openocd"

  make distclean
fi

# -----------------------------------------------------------------------------

if [ "${DEBUG}" == "y" ]
then
  export CFLAGS="-g"
fi
export

# Configure OpenOCD. Use (more or less) the same options as Freddie Chopin.

mkdir -p "${OPENOCD_BUILD_FOLDER}/openocd"
cd "${OPENOCD_BUILD_FOLDER}/openocd"

# All variables below are passed on the command line before 'configure'.
# Be sure all these lines end in '\' to ensure lines are concatenated.
# On some machines libftdi ends in lib64, so we refer both lib & lib64
HIDAPI_CFLAGS="-I${OPENOCD_WORK}/${HIDAPI}/hidapi" \
HIDAPI_LIBS="-L${OPENOCD_WORK}/${HIDAPI}/${HIDAPI_TARGET} -lhid" \
\
LDFLAGS="-L/opt/local/lib" \
CPPFLAGS="-I/opt/local/include" \
LIBS="-framework IOKit -framework CoreFoundation" \
PKG_CONFIG_PATH="${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64/pkgconfig":\
"${PKG_CONFIG_PATH}" \
DYLD_LIBRARY_PATH="${DYLD_LIBRARY_PATH}":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib" \
\
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

# Do a clean build, with documentation.
cd "${OPENOCD_BUILD_FOLDER}/openocd"

# The bindir and pkgdatadir are required to configure bin and scripts folders
# at the same level in the hierarchy.
make bindir="bin" pkgdatadir="" clean all pdf html
if [ "${DEBUG}" != "y" ]
then
  strip src/openocd
fi

# Prepare the pkgbuild root folder.
rm -rf "${OPENOCD_PKG_FOLDER}"
mkdir -p "${OPENOCD_PKG_FOLDER}/bin"
mkdir -p "${OPENOCD_PKG_FOLDER}/scripts"
mkdir -p "${OPENOCD_PKG_FOLDER}/doc"
mkdir -p "${OPENOCD_PKG_FOLDER}/license/openocd"
mkdir -p "${OPENOCD_PKG_FOLDER}/license/hidapi"
mkdir -p "${OPENOCD_PKG_FOLDER}/info"

# Make all dynlib references relative to the executable folder.

cp "${OPENOCD_BUILD_FOLDER}/openocd/src/openocd" "${OPENOCD_PKG_FOLDER}/bin"
otool -L "${OPENOCD_PKG_FOLDER}/bin/openocd"
install_name_tool -change "libftdi1.2.dylib" "@executable_path/libftdi1.2.dylib" "${OPENOCD_PKG_FOLDER}/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/openocd"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.4.dylib" "@executable_path/libusb-0.1.4.dylib" "${OPENOCD_PKG_FOLDER}/bin/openocd"
install_name_tool -change "/opt/local/lib/libusb-0.1.4.dylib" "@executable_path/libusb-0.1.4.dylib" "${OPENOCD_PKG_FOLDER}/bin/openocd"
otool -L "${OPENOCD_PKG_FOLDER}/bin/openocd"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.2.2.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"
install_name_tool -id libftdi1.2.dylib "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libftdi1.2.dylib"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.4.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"
install_name_tool -id libusb-0.1.4.dylib "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libusb-0.1.4.dylib"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"
install_name_tool -id libusb-1.0.0.dylib "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"
otool -L "${OPENOCD_PKG_FOLDER}/bin/libusb-1.0.0.dylib"

# "${OPENOCD_PKG_FOLDER}/bin/openocd" --version

cp -r "${OPENOCD_GIT_FOLDER}/tcl/"* "${OPENOCD_PKG_FOLDER}/scripts"

cp "${OPENOCD_BUILD_FOLDER}/openocd/doc/openocd.pdf" "${OPENOCD_PKG_FOLDER}/doc"
cp -r "${OPENOCD_BUILD_FOLDER}/openocd/doc/openocd.html" "${OPENOCD_PKG_FOLDER}/doc"

cp -r "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/pkgbuild/"* "${OPENOCD_PKG_FOLDER}/info"

cp "${OPENOCD_GIT_FOLDER}/AUTHORS" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/COPYING" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/"NEW* "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/README" "${OPENOCD_PKG_FOLDER}/license/openocd"
cp "${OPENOCD_GIT_FOLDER}/README.OSX" "${OPENOCD_PKG_FOLDER}/license/openocd"

cp "${OPENOCD_WORK}/hidapi-0.7.0/AUTHORS.txt" "${OPENOCD_PKG_FOLDER}/license/hidapi"
cp "${OPENOCD_WORK}/hidapi-0.7.0/"LICENSE* "${OPENOCD_PKG_FOLDER}/license/hidapi"
cp "${OPENOCD_WORK}/hidapi-0.7.0/README.txt" "${OPENOCD_PKG_FOLDER}/license/hidapi"

mkdir -p "${OPENOCD_WORK}/output"

INSTALLER=${OPENOCD_WORK}/output/gnuarmeclipse-openocd-osx-${OUTFILE_VERSION}-${NDATE}.pkg

cd "${OPENOCD_WORK}"

pkgbuild --identifier ilg.gnuarmeclipse.openocd \
--root "${OPENOCD_PKG_FOLDER}" \
--version "${OUTFILE_VERSION}" \
--install-location "${INSTALL_FOLDER:1}" \
"${INSTALLER}"

echo
"${OPENOCD_PKG_FOLDER}/bin/openocd" --version
RESULT="$?"

echo
if [ "${RESULT}" == "0" ]
then
  echo "Build completed."
else
  echo "Buld failed."
fi

