#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

# Script to build the OS X version of OpenOCD. It produces an install package
# that expands in "/Applications/GNU ARM Eclipse/OpenOCD".

# Prerequisites:
#
# MacPorts with the following ports installed:
#
# sudo port install libtool automake autoconf pkgconfig wget
# sudo port install cmake boost libconfuse swig-python
# sudo port install texinfo texlive

# Prepare MacPorts environment.

export PATH=/opt/local/bin:/opt/local/sbin:$PATH
port version >/dev/null
if [ $? != 0 ]
then
  echo "Mandatory MacPorts not found, quit."
  exit 1
fi

# ----- Externally configurable variables -----

# Define it externally to "y"
DEBUG=${DEBUG:-"n"}

# The folder where the entire build procedure will run.
# If you prefer to build in a separate folder, define it before invoking
# the script.
if [ -d /media/Work ]
then
  OPENOCD_WORK=${OPENOCD_WORK:-"/media/Work/openocd"}
else
  OPENOCD_WORK=${OPENOCD_WORK:-${HOME}/Work/openocd}
fi

# The UTC date part in the name of the archive.
NDATE=${NDATE:-$(date -u +%Y%m%d%H%M)}

# The folder where OpenOCD will be installed.
INSTALL_FOLDER=${INSTALL_FOLDER:-"/Applications/GNU ARM Eclipse/OpenOCD"}

PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-""}
DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH:-""}

# ----- Local variables -----

OUTFILE_VERSION="0.8.0"

# For updates, please check the corresponding pages.

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
OPENOCD_OUTPUT="${OPENOCD_WORK}/output"

WGET="wget"
WGET_OUT="-O"

ACTION=${1:-}

if [ $# > 0 ]
then
  if [ "${ACTION}" == "clean" ]
  then
    # Remove most build and temporary folders
    rm -rfv "${OPENOCD_BUILD_FOLDER}"
    rm -rfv "${OPENOCD_INSTALL_FOLDER}"
    #rm -rfv "${OPENOCD_PKG_FOLDER}"
    rm -rfv "${OPENOCD_WORK}/${LIBFTDI}"
    rm -rfv "${OPENOCD_WORK}/${LIBUSB0}"
    rm -rfv "${OPENOCD_WORK}/${LIBUSB1}"
    rm -rfv "${OPENOCD_WORK}/${HIDAPI}"

    # exit 0
    # Continue with build
  fi
fi

# ----- Begin of common part --------------------------------------------------

# Create the work folder.
mkdir -p "${OPENOCD_WORK}"

# Build the USB libraries.

# Both USB libraries are available from a single project LIBUSB
# 	http://www.libusb.info
# with source files ready to download from SourceForge
# 	https://sourceforge.net/projects/libusb/files

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
if [ ! \( -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.a" -o \
          -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64/libusb-1.0.a" \) ]
then
  rm -rfv "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}"

  rm -rfv "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"
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
if [ ! \( -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb.a" -o \
          -f "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib64/libusb.a" \) ]
then
  rm -rfv "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  rm -rfv "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  cd "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"

  # Configure
  PKG_CONFIG_PATH=\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64/pkgconfig":\
"${PKG_CONFIG_PATH}" \
  \
  "${OPENOCD_WORK}/${LIBUSB0}/configure" \
  --prefix="${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}"

  # Build
  make clean install
fi

# Build the FTDI library.

# There are two versions of the FDDI library; we recommend using the 
# open source one, available from intra2net.
#	http://www.intra2net.com/en/developer/libftdi/

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
  rm -rfv "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}"

  rm -rfv "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  cd "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"

  # Configure
  PKG_CONFIG_PATH=\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64/pkgconfig":\
"${PKG_CONFIG_PATH}" \
  \
  cmake \
  -DCMAKE_INSTALL_PREFIX="${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}" \
  "${OPENOCD_WORK}/${LIBFTDI}"

  # Build
  make clean install
fi


# Build the HDI library.

# This is just a simple wrapper over libusb.
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
if [ ! -f "${OPENOCD_WORK}/${HIDAPI}/${HIDAPI_TARGET}/libhid.a" ]
then
  cd "${OPENOCD_WORK}/${HIDAPI}/${HIDAPI_TARGET}"

  PKG_CONFIG_PATH=\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64/pkgconfig":\
"${PKG_CONFIG_PATH}" \
  \
  make clean "${HIDAPI_OBJECT}"

  # Make just compiles the file. Create the archive.
  # No dynamic/shared libs involved.
  ar -r  "libhid.a" "${HIDAPI_OBJECT}"
fi

# Get the GNU ARM Eclipse OpenOCD git repository.

# The custom OpenOCD branch is available from the dedicated Git repository
# which is part of the GNU ARM Eclipse project hosted on SourceForge.
# Generally this branch follows the official OpenOCD master branch, 
# with updates after every OpenOCD public release.

if [ ! -d "${OPENOCD_GIT_FOLDER}" ]
then
  cd "${OPENOCD_WORK}"

  if [ "$(whoami)" == "ilg" ]
  then
    # Shortcut for ilg, who has full access to the repo.
    git clone ssh://ilg-ul@git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  else
    # For regular read/only access, use the git url.
    git clone http://git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  fi

  # Change to the gnuarmeclipse branch. On subsequent runs use "git pull".
  cd "${OPENOCD_GIT_FOLDER}"
  git checkout gnuarmeclipse

  # Prepare autotools.
  cd "${OPENOCD_GIT_FOLDER}"
  ./bootstrap
fi

# On first run, create the build folder.
mkdir -p "${OPENOCD_BUILD_FOLDER}/openocd"

# On subsequent runs, clear it to always force a configure.
if [ -f "${OPENOCD_BUILD_FOLDER}/openocd/config.h" ]
then
  cd "${OPENOCD_BUILD_FOLDER}/openocd"

  make distclean
fi

# ----- End of common part ----------------------------------------------------

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
PKG_CONFIG_PATH=\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib64/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64/pkgconfig":\
"${PKG_CONFIG_PATH}" \
DYLD_LIBRARY_PATH=\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib64":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib64":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib":\
"${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib64":\
"${DYLD_LIBRARY_PATH}" \
\
"${OPENOCD_GIT_FOLDER}/configure" \
--prefix="${OPENOCD_INSTALL_FOLDER}/openocd"  \
--datarootdir="${OPENOCD_INSTALL_FOLDER}" \
--infodir="${OPENOCD_INSTALL_FOLDER}/openocd/info"  \
--localedir="${OPENOCD_INSTALL_FOLDER}/openocd/locale"  \
--mandir="${OPENOCD_INSTALL_FOLDER}/openocd/man"  \
--docdir="${OPENOCD_INSTALL_FOLDER}/openocd/doc"  \
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

# The bindir and pkgdatadir are required to configure bin and scripts folders
# at the same level in the hierarchy.
cd "${OPENOCD_BUILD_FOLDER}/openocd"
make bindir="bin" pkgdatadir="" clean all pdf html
if [ "${DEBUG}" != "y" ]
then
  strip src/openocd
fi

# Always clear the destination folder, to have a consistent package.
rm -rfv "${OPENOCD_INSTALL_FOLDER}/openocd"

# Exhaustive install, including documentation.

cd "${OPENOCD_BUILD_FOLDER}/openocd"
make install install-pdf install-html install-man

# Copy the dynamic libraries to the same folder where the application file is.
# Post-process dynamic libraries paths to be relative to executable folder.

# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "libftdi1.2.dylib" "@executable_path/libftdi1.2.dylib" \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" "@executable_path/libusb-1.0.0.dylib" \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.4.dylib" \
"@executable_path/libusb-0.1.4.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "/opt/local/lib/libusb-0.1.4.dylib" \
"@executable_path/libusb-0.1.4.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBFTDI}/lib/libftdi1.2.2.0.dylib" \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"
install_name_tool -id libftdi1.2.dylib "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libftdi1.2.dylib"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBUSB0}/lib/libusb-0.1.4.dylib" \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"
install_name_tool -id libusb-0.1.4.dylib "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-0.1.4.dylib"

cp "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"
install_name_tool -id libusb-1.0.0.dylib "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/${LIBUSB1}/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"
install_name_tool -change "/opt/local/lib/libusb-1.0.0.dylib" \
"@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/libusb-1.0.0.dylib"

# Copy the license files.

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/openocd"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/AUTHORS" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/openocd"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/COPYING" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/openocd"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/"NEWS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/openocd"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/openocd"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${HIDAPI}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${HIDAPI}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${HIDAPI}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBFTDI}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBFTDI}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBFTDI}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBFTDI}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB1}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB1}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB1}/"NEWS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB1}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB0}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB0}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB0}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB0}/"NEWS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK}/${LIBUSB0}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"

# Copy the GNU ARM Eclipse info files.
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/INFO-osx.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/INFO.txt"
mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/BUILD-osx.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/BUILD.txt"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/CHANGES.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/build-openocd-osx.sh" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/"

# Create the distribution installer.

mkdir -p "${OPENOCD_OUTPUT}"

OPENOCD_INSTALLER=${OPENOCD_WORK}/output/gnuarmeclipse-openocd-${OPENOCD_TARGET}-${OUTFILE_VERSION}-${NDATE}.pkg

# Create the installer package, with content from the
# ${OPENOCD_INSTALL_FOLDER}/openocd folder.
# The "${INSTALL_FOLDER:1}" is a substring that skips first char.
cd "${OPENOCD_WORK}"
echo
pkgbuild --identifier ilg.gnuarmeclipse.openocd \
--root "${OPENOCD_INSTALL_FOLDER}/openocd" \
--version "${OUTFILE_VERSION}" \
--install-location "${INSTALL_FOLDER:1}" \
"${OPENOCD_INSTALLER}"

echo
ls -l "${OPENOCD_INSTALL_FOLDER}/openocd/bin"

# Check if the application starts (if all dynamic libraries are available).
echo
"${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd" --version
RESULT="$?"

echo
if [ "${RESULT}" == "0" ]
then
  echo "Build completed."
else
  echo "Buld failed."
fi

exit 0
