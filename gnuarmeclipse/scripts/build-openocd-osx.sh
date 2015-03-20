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

# ----- Prepare MacPorts environment -----

export PATH=/opt/local/bin:/opt/local/sbin:$PATH
port version >/dev/null
if [ $? != 0 ]
then
  echo "Mandatory MacPorts not found, quit."
  exit 1
fi

# ----- Parse actions and command line options -----

ACTION_CLEAN=""
ACTION_GIT=""
TARGET_BITS="64"

while [ $# -gt 0 ]
do
  if [ "$1" == "clean" ]
  then
    ACTION_CLEAN="$1"
  elif [ "$1" == "pull" ]
  then
    ACTION_GIT="$1"
  elif [ "$1" == "checkout-dev" ]
  then
    ACTION_GIT="$1"
  elif [ "$1" == "checkout-stable" ]
  then
    ACTION_GIT="$1"
  else
    echo "Unknown action/option $1"
    exit 1
  fi

  shift
done

# ----- Externally configurable variables -----

# The folder where the entire build procedure will run.
# If you prefer to build in a separate folder, define it before invoking
# the script.
if [ -d /media/Work ]
then
  OPENOCD_WORK_FOLDER=${OPENOCD_WORK_FOLDER:-"/media/Work/openocd"}
else
  OPENOCD_WORK_FOLDER=${OPENOCD_WORK_FOLDER:-${HOME}/Work/openocd}
fi

# The folder where OpenOCD will be installed.
INSTALL_FOLDER=${INSTALL_FOLDER:-"/Applications/GNU ARM Eclipse/OpenOCD"}

# PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-""}
PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR:-""}
DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH:-""}

MAKE_JOBS=${MAKE_JOBS:-"-j8"}

# ----- Local variables -----

# For updates, please check the corresponding pages.

# http://www.intra2net.com/en/developer/libftdi/download.php
LIBFTDI="libftdi1-1.2"

# https://sourceforge.net/projects/libusb/files/libusb-compat-0.1/
LIBUSB0="libusb-compat-0.1.5"

# https://sourceforge.net/projects/libusb/files/libusb-1.0/
LIBUSB1="libusb-1.0.19"

# https://github.com/signal11/hidapi/downloads
HIDAPI="hidapi-0.7.0"

HIDAPI_TARGET="mac"
HIDAPI_OBJECT="hid.o"

OPENOCD_TARGET="osx"

OPENOCD_GIT_FOLDER="${OPENOCD_WORK_FOLDER}/gnuarmeclipse-openocd.git"
OPENOCD_DOWNLOAD_FOLDER="${OPENOCD_WORK_FOLDER}/download"
OPENOCD_BUILD_FOLDER="${OPENOCD_WORK_FOLDER}/build/${OPENOCD_TARGET}"
OPENOCD_INSTALL_FOLDER="${OPENOCD_WORK_FOLDER}/install/${OPENOCD_TARGET}"
OPENOCD_OUTPUT="${OPENOCD_WORK_FOLDER}/output"

WGET="wget"
WGET_OUT="-O"

# ----- Test if some tools are present -----

echo
echo "Test tools..."
echo
gcc --version
git --version
automake --version
cmake --version

# Process actions.

if [ "${ACTION_CLEAN}" == "clean" ]
then
  # Remove most build and temporary folders
  echo
  echo "Remove most build folders..."

  rm -rf "${OPENOCD_BUILD_FOLDER}"
  rm -rf "${OPENOCD_INSTALL_FOLDER}"
  rm -rf "${OPENOCD_WORK_FOLDER}/${LIBFTDI}"
  rm -rf "${OPENOCD_WORK_FOLDER}/${LIBUSB0}"
  rm -rf "${OPENOCD_WORK_FOLDER}/${LIBUSB1}"
  rm -rf "${OPENOCD_WORK_FOLDER}/${HIDAPI}"

  echo
  echo "Clean completed. Proceed with a regular build."
  exit 0
fi

if [ "${ACTION_GIT}" == "pull" ]
then
  if [ -d "${OPENOCD_GIT_FOLDER}" ]
  then
    echo
    if [ "${USER}" == "ilg" ]
    then
      echo "Enter SourceForge password for git pull"
    fi
    cd "${OPENOCD_GIT_FOLDER}"
    git pull
    git submodule update

    rm -rf "${OPENOCD_BUILD_FOLDER}/openocd"

    # Prepare autotools.
    echo
    echo "bootstrap..."

    cd "${OPENOCD_GIT_FOLDER}"
    ./bootstrap

    echo
    echo "Pull completed. Proceed with a regular build."
    exit 0
  else
	echo "No git folder."
    exit 1
  fi
fi

if [ "${ACTION_GIT}" == "checkout-dev" ]
then
  if [ -d "${OPENOCD_GIT_FOLDER}" ]
  then
    echo
    if [ "${USER}" == "ilg" ]
    then
      echo "Enter SourceForge password for git pull"
    fi
    cd "${OPENOCD_GIT_FOLDER}"
    git pull
    git checkout gnuarmeclipse-dev
    git submodule update

    rm -rf "${OPENOCD_BUILD_FOLDER}/openocd"

    # Prepare autotools.
    echo
    echo "bootstrap..."

    cd "${OPENOCD_GIT_FOLDER}"
    ./bootstrap

    echo
    echo "Pull completed. Proceed with a regular build."
    exit 0
  else
	echo "No git folder."
    exit 1
  fi
fi

if [ "${ACTION_GIT}" == "checkout-stable" ]
then
  if [ -d "${OPENOCD_GIT_FOLDER}" ]
  then
    echo
    if [ "${USER}" == "ilg" ]
    then
      echo "Enter SourceForge password for git pull"
    fi
    cd "${OPENOCD_GIT_FOLDER}"
    git pull
    git checkout gnuarmeclipse
    git submodule update

    rm -rf "${OPENOCD_BUILD_FOLDER}/openocd"

    # Prepare autotools.
    echo
    echo "bootstrap..."

    cd "${OPENOCD_GIT_FOLDER}"
    ./bootstrap

    echo
    echo "Pull completed. Proceed with a regular build."
    exit 0
  else
	echo "No git folder."
    exit 1
  fi
fi

# ----- Begin of common part --------------------------------------------------

# Create the work folder.
mkdir -p "${OPENOCD_WORK_FOLDER}"

# ----- Get the GNU ARM Eclipse OpenOCD git repository -----

# The custom OpenOCD branch is available from the dedicated Git repository
# which is part of the GNU ARM Eclipse project hosted on SourceForge.
# Generally this branch follows the official OpenOCD master branch, 
# with updates after every OpenOCD public release.

if [ ! -d "${OPENOCD_GIT_FOLDER}" ]
then
  cd "${OPENOCD_WORK_FOLDER}"

  if [ "${USER}" == "ilg" ]
  then
    # Shortcut for ilg, who has full access to the repo.
    echo
    echo "Enter SourceForge password for git clone"
    git clone ssh://ilg-ul@git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  else
    # For regular read/only access, use the git url.
    git clone http://git.code.sf.net/p/gnuarmeclipse/openocd gnuarmeclipse-openocd.git
  fi

  # Change to the gnuarmeclipse branch. On subsequent runs use "git pull".
  cd "${OPENOCD_GIT_FOLDER}"
  git checkout gnuarmeclipse
  git submodule update

  # ---- Prepare autotools -----
  echo
  echo "bootstrap..."

  cd "${OPENOCD_GIT_FOLDER}"
  ./bootstrap
fi

# Get the current Git branch name, to know if we are building the stable or
# the development release.
cd "${OPENOCD_GIT_FOLDER}"
OPENOCD_GIT_HEAD=$(git symbolic-ref -q --short HEAD)

# ----- Build the USB libraries -----

# Both USB libraries are available from a single project LIBUSB
# 	http://www.libusb.info
# with source files ready to download from SourceForge
# 	https://sourceforge.net/projects/libusb/files

# Download the new USB library.
LIBUSB1_ARCHIVE="${LIBUSB1}.tar.bz2"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"

  cd "${OPENOCD_DOWNLOAD_FOLDER}"
  "${WGET}" "http://sourceforge.net/projects/libusb/files/libusb-1.0/${LIBUSB1}/${LIBUSB1_ARCHIVE}" \
  "${WGET_OUT}" "${LIBUSB1_ARCHIVE}"
fi

# Unpack the new USB library.
if [ ! -d "${OPENOCD_WORK_FOLDER}/${LIBUSB1}" ]
then
  cd "${OPENOCD_WORK_FOLDER}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB1_ARCHIVE}"
fi

# Build and install the new USB library.
if [ ! \( -d "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}" \) -o \
     ! \( -f "${OPENOCD_INSTALL_FOLDER}/lib/libusb-1.0.a" -o \
          -f "${OPENOCD_INSTALL_FOLDER}/lib64/libusb-1.0.a" \) ]
then
  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"

  mkdir -p "${OPENOCD_INSTALL_FOLDER}"

  cd "${OPENOCD_BUILD_FOLDER}/${LIBUSB1}"
  # Configure
  CFLAGS="-Wno-non-literal-null-conversion -m${TARGET_BITS}" \
  "${OPENOCD_WORK_FOLDER}/${LIBUSB1}/configure" \
  --prefix="${OPENOCD_INSTALL_FOLDER}"

  # Build
  make ${MAKE_JOBS} clean install
fi

# http://www.libusb.org

# Download the old USB library.
LIBUSB0_ARCHIVE="${LIBUSB0}.tar.bz2"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"

  cd "${OPENOCD_DOWNLOAD_FOLDER}"
  "${WGET}" "http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/${LIBUSB0}/${LIBUSB0_ARCHIVE}" \
  "${WGET_OUT}" "${LIBUSB0_ARCHIVE}"
fi

# Unpack the old USB library.
if [ ! -d "${OPENOCD_WORK_FOLDER}/${LIBUSB0}" ]
then
  cd "${OPENOCD_WORK_FOLDER}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBUSB0_ARCHIVE}"
fi

# Build and install the old USB library.
if [ ! \( -d "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}" \) -o \
     ! \( -f "${OPENOCD_INSTALL_FOLDER}/lib/libusb.a" -o \
          -f "${OPENOCD_INSTALL_FOLDER}/lib64/libusb.a" \) ]
then
  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"

  mkdir -p "${OPENOCD_INSTALL_FOLDER}"

  cd "${OPENOCD_BUILD_FOLDER}/${LIBUSB0}"
  # Configure
  CFLAGS="-m${TARGET_BITS}" \
  \
  PKG_CONFIG_LIBDIR=\
"${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/lib64/pkgconfig" \
  \
  "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/configure" \
  --prefix="${OPENOCD_INSTALL_FOLDER}"

  # Build
  make ${MAKE_JOBS} clean install
fi

# ----- Build the FTDI library -----

# There are two versions of the FDDI library; we recommend using the 
# open source one, available from intra2net.
#	http://www.intra2net.com/en/developer/libftdi/

# Download the FTDI library.
LIBFTDI_ARCHIVE="${LIBFTDI}.tar.bz2"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"

  cd "${OPENOCD_DOWNLOAD_FOLDER}"
  "${WGET}" "http://www.intra2net.com/en/developer/libftdi/download/${LIBFTDI_ARCHIVE}" \
  "${WGET_OUT}" "${LIBFTDI_ARCHIVE}"
fi

# Unpack the FTDI library.
if [ ! -d "${OPENOCD_WORK_FOLDER}/${LIBFTDI}" ]
then
  cd "${OPENOCD_WORK_FOLDER}"
  tar -xjvf "${OPENOCD_DOWNLOAD_FOLDER}/${LIBFTDI_ARCHIVE}"

  cd "${OPENOCD_WORK_FOLDER}/${LIBFTDI}"
  # Patch to prevent the use of system libraries and force the use of local ones.
  patch -p0 < "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/patches/${LIBFTDI}-cmake-FindUSB1.patch"
fi

# Build and install the FTDI library.
if [ ! \( -d "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}" \) -o \
     ! \( -f "${OPENOCD_INSTALL_FOLDER}/lib/libftdi1.a" -o \
           -f "${OPENOCD_INSTALL_FOLDER}/lib64/libftdi1.a" \)  ]
then
  rm -rf "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"

  mkdir -p "${OPENOCD_INSTALL_FOLDER}"

  echo
  echo "cmake libftdi..."

  cd "${OPENOCD_BUILD_FOLDER}/${LIBFTDI}"
  # cmake
  CFLAGS="-m${TARGET_BITS}" \
  \
  PKG_CONFIG_LIBDIR=\
"${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/lib64/pkgconfig" \
  \
  cmake \
  -DCMAKE_INSTALL_PREFIX="${OPENOCD_INSTALL_FOLDER}" \
  -DBUILD_TESTS:BOOL=off \
  -DFTDIPP:BOOL=off \
  -DPYTHON_BINDINGS:BOOL=off \
  -DEXAMPLES:BOOL=off \
  -DDOCUMENTATION:BOOL=off \
  -DFTDI_EEPROM:BOOL=off \
  "${OPENOCD_WORK_FOLDER}/${LIBFTDI}"

  # Build
  make ${MAKE_JOBS} clean install
fi

# ----- Build the HDI library -----

# This is just a simple wrapper over libusb.
# http://www.signal11.us/oss/hidapi/

# Download the HDI library.
HIDAPI_ARCHIVE="${HIDAPI}.zip"
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" "https://github.com/downloads/signal11/hidapi/${HIDAPI_ARCHIVE}" \
  "${WGET_OUT}" "${HIDAPI_ARCHIVE}"
fi

# Unpack the HDI library.
if [ ! -d "${OPENOCD_WORK_FOLDER}/${HIDAPI}" ]
then
  cd "${OPENOCD_WORK_FOLDER}"
  unzip "${OPENOCD_DOWNLOAD_FOLDER}/${HIDAPI_ARCHIVE}"
fi

if [ ! \( -d "${OPENOCD_BUILD_FOLDER}/${HIDAPI}" \) -o \
     ! \( -f "${OPENOCD_INSTALL_FOLDER}/lib/libhid.a" \) ]
then
  rm -rf "${OPENOCD_BUILD_FOLDER}/${HIDAPI}"
  mkdir -p "${OPENOCD_BUILD_FOLDER}/${HIDAPI}"

  cp -r "${OPENOCD_WORK_FOLDER}/${HIDAPI}/"* \
    "${OPENOCD_BUILD_FOLDER}/${HIDAPI}"

  echo
  echo "make libhid..."

  cd "${OPENOCD_BUILD_FOLDER}/${HIDAPI}/${HIDAPI_TARGET}"

  CFLAGS="-m${TARGET_BITS}" \
  \
  PKG_CONFIG_LIBDIR=\
"${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/lib64/pkgconfig": \
  \
  make clean "${HIDAPI_OBJECT}"

  # Make just compiles the file. Create the archive.
  # No dynamic/shared libs involved.
  ar -r libhid.a ${HIDAPI_OBJECT}
  ranlib libhid.a

  mkdir -p "${OPENOCD_INSTALL_FOLDER}/lib"
  cp -v libhid.a \
     "${OPENOCD_INSTALL_FOLDER}/lib"

  mkdir -p "${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig"
  sed -e "s|XXX|${OPENOCD_INSTALL_FOLDER}|" \
    "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/pkgconfig/${HIDAPI}-${HIDAPI_TARGET}.pc" \
    > "${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig/hidapi.pc"

  mkdir -p "${OPENOCD_INSTALL_FOLDER}/include/hidapi"
  cp -v "${OPENOCD_WORK_FOLDER}/${HIDAPI}/hidapi/hidapi.h" \
     "${OPENOCD_INSTALL_FOLDER}/include/hidapi"

fi

# On first run, create the build folder.
mkdir -p "${OPENOCD_BUILD_FOLDER}/openocd"

# ----- End of common part ----------------------------------------------------

# Configure OpenOCD. Use (more or less) the same options as Freddie Chopin.

if [ ! \( -d "${OPENOCD_BUILD_FOLDER}/openocd" \) -o \
     ! \( -f "${OPENOCD_BUILD_FOLDER}/openocd/config.h" \) ]
then

  echo
  echo "configure..."

  mkdir -p "${OPENOCD_BUILD_FOLDER}/openocd"
  cd "${OPENOCD_BUILD_FOLDER}/openocd"

  # All variables below are passed on the command line before 'configure'.
  # Be sure all these lines end in '\' to ensure lines are concatenated.
  CPPFLAGS="-m${TARGET_BITS}" \
  \
  PKG_CONFIG_LIBDIR=\
"${OPENOCD_INSTALL_FOLDER}/lib/pkgconfig":\
"${OPENOCD_INSTALL_FOLDER}/lib64/pkgconfig" \
  \
  DYLD_LIBRARY_PATH=\
"${OPENOCD_INSTALL_FOLDER}/lib":\
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

fi

# ----- Full build, with documentation -----

# The bindir and pkgdatadir are required to configure bin and scripts folders
# at the same level in the hierarchy.
cd "${OPENOCD_BUILD_FOLDER}/openocd"
make ${MAKE_JOBS} bindir="bin" pkgdatadir="" all pdf html

# Always clear the destination folder, to have a consistent package.
echo
echo "remove install..."

rm -rf "${OPENOCD_INSTALL_FOLDER}/openocd"

# ----- Full install, including documentation -----

echo
echo "make install..."

cd "${OPENOCD_BUILD_FOLDER}/openocd"
make install-strip install-pdf install-html install-man

# ----- Copy dynamic libraries to the install bin folder -----

# Post-process dynamic libraries paths to be relative to executable folder.

# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "libftdi1.2.dylib" "@executable_path/libftdi1.2.dylib" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/lib/libusb-1.0.0.dylib" \
  "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/lib/libusb-0.1.4.dylib" \
  "@executable_path/libusb-0.1.4.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/openocd"

DLIB="libftdi1.2.dylib"
cp "${OPENOCD_INSTALL_FOLDER}/lib/libftdi1.2.2.0.dylib" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
install_name_tool -id "${DLIB}" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/lib/libusb-1.0.0.dylib" \
  "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"

DLIB="libusb-0.1.4.dylib"
cp "${OPENOCD_INSTALL_FOLDER}/lib/libusb-0.1.4.dylib" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
install_name_tool -id "${DLIB}" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
install_name_tool -change "${OPENOCD_INSTALL_FOLDER}/lib/libusb-1.0.0.dylib" \
  "@executable_path/libusb-1.0.0.dylib" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"

DLIB="libusb-1.0.0.dylib"
cp "${OPENOCD_INSTALL_FOLDER}/lib/libusb-1.0.0.dylib" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
# otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
install_name_tool -id "${DLIB}" "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"
otool -L "${OPENOCD_INSTALL_FOLDER}/openocd/bin/${DLIB}"

# ----- Copy the license files -----

echo
echo "copy license files..."

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
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${HIDAPI}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${HIDAPI}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${HIDAPI}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${HIDAPI}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBFTDI}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBFTDI}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBFTDI}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBFTDI}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBFTDI}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB1}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB1}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB1}/"NEWS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB1}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB1}"

mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/"AUTHORS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/"COPYING* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/"LICENSE* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/"NEWS* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"
/usr/bin/install -c -m 644 "${OPENOCD_WORK_FOLDER}/${LIBUSB0}/"README* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license/${LIBUSB0}"

# ----- Copy the GNU ARM Eclipse info files -----

echo
echo "copy info files..."

/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/info/INFO-osx.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/INFO.txt"
mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/info/BUILD-osx.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/BUILD.txt"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/info/CHANGES.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/"
/usr/bin/install -c -m 644 "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/scripts/build-openocd-osx.sh" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/"

# ----- Create the distribution installer -----

mkdir -p "${OPENOCD_OUTPUT}"

# The UTC date part in the name of the archive.
OUTFILE_DATE=${OUTFILE_DATE:-$(date -u +%Y%m%d%H%M)}

if [ "${OPENOCD_GIT_HEAD}" == "gnuarmeclipse" ]
then
  OUTFILE_VERSION=$(cat "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/VERSION")-${OUTFILE_DATE}
elif [ "${OPENOCD_GIT_HEAD}" == "gnuarmeclipse-dev" ]
then
  OUTFILE_VERSION=$(cat "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/VERSION-dev")-${OUTFILE_DATE}-dev
fi

OPENOCD_INSTALLER="${OPENOCD_OUTPUT}/gnuarmeclipse-openocd-\
${OPENOCD_TARGET}-${OUTFILE_VERSION}.pkg"

echo
echo "create installer package..."
echo

# Create the installer package, with content from the
# ${OPENOCD_INSTALL_FOLDER}/openocd folder.
# The "${INSTALL_FOLDER:1}" is a substring that skips first char.
cd "${OPENOCD_WORK_FOLDER}"
pkgbuild --identifier ilg.gnuarmeclipse.openocd \
  --root "${OPENOCD_INSTALL_FOLDER}/openocd" \
  --version "${OUTFILE_VERSION}" \
  --install-location "${INSTALL_FOLDER:1}/${OUTFILE_VERSION}" \
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
  echo "File ${OPENOCD_INSTALLER} created."
else
  echo "Buld failed."
fi

exit 0
