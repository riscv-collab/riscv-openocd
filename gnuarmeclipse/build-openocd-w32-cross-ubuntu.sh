#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

# Script to cross build the 32-bit Windows version of OpenOCD with MinGW-w64
# on Debian.

# Prerequisites:
#
# sudo apt-get install git libtool autoconf automake autotools-dev pkg-config
# sudo apt-get install doxygen texinfo texlive dos2unix
# sudo apt-get install mingw-w64 mingw-w64-tools mingw-w64-dev

# ----- Externally configurable variables -----

# The folder where the entire build procedure will run.
# If you prefer to build in a separate folder, define it before invoking
# the script.
if [ -d /media/${USER}/Work ]
then
  OPENOCD_WORK=${OPENOCD_WORK:-"/media/${USER}/Work/openocd"}
else
  OPENOCD_WORK=${OPENOCD_WORK:-${HOME}/Work/openocd}
fi

# The UTC date part in the name of the archive. 
NDATE=${NDATE:-$(date -u +%Y%m%d%H%M)}

# ----- Local variables -----

OUTFILE_VERSION="0.8.0"

OPENOCD_TARGET="win32"

INPUT_VERSION="0.8.0"
INPUT_ZIP="openocd-${INPUT_VERSION}_x86_devkit.zip"
INPUT_ZIP_FOLDER="openocd-${INPUT_VERSION}_x86_devkit"

OPENOCD_GIT_FOLDER="${OPENOCD_WORK}/gnuarmeclipse-openocd.git"
OPENOCD_DOWNLOAD_FOLDER="${OPENOCD_WORK}/download"
OPENOCD_BUILD_FOLDER="${OPENOCD_WORK}/build/${OPENOCD_TARGET}"
OPENOCD_INSTALL_FOLDER="${OPENOCD_WORK}/install/${OPENOCD_TARGET}"
OPENOCD_OUTPUT="${OPENOCD_WORK}/output"

CROSS_COMPILE="i686-w64-mingw32"

WGET="wget"
WGET_OUT="-O"

ACTION=${1:-}

if [ $# -gt 0 ]
then
  if [ "${ACTION}" == "clean" ]
  then
    # Remove most build and temporary folders
    rm -rfv "${OPENOCD_BUILD_FOLDER}"
    rm -rfv "${OPENOCD_INSTALL_FOLDER}"

    # exit 0
    # Continue with build
  fi
fi

# Test if various tools are present
${CROSS_COMPILE}-gcc --version
unix2dos --version
git --version
makensis -VERSION

# Create the work folder.
mkdir -p "${OPENOCD_WORK}"

# To simplify the build, we do not build the libraries, but use them form 
# the open source picusb related project, which includes a version of OpenOCD.
# https://sourceforge.net/projects/picusb/
# From this archive the binary DLLs will be directly copied to the setup, 
# and the headers and libraries will be referred during the build.
if [ ! -f "${OPENOCD_DOWNLOAD_FOLDER}/${INPUT_ZIP}" ]
then
  mkdir -p "${OPENOCD_DOWNLOAD_FOLDER}"
  cd "${OPENOCD_DOWNLOAD_FOLDER}"

  "${WGET}" "http://sourceforge.net/projects/picusb/files/${INPUT_ZIP}" \
  "${WGET_OUT}" "${INPUT_ZIP}"
fi

if [ ! -d "${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}" ]
then
  mkdir -p "${OPENOCD_INSTALL_FOLDER}"
  cd "${OPENOCD_INSTALL_FOLDER}"
  unzip "${OPENOCD_DOWNLOAD_FOLDER}/${INPUT_ZIP}"
fi

# Get the GNU ARM Eclipse OpenOCD git repository.

# The custom OpenOCD branch is available from the dedicated Git repository
# which is part of the GNU ARM Eclipse project hosted on SourceForge.
# Generally this branch follows the official OpenOCD master branch, 
# with updates after every OpenOCD public release.

if [ ! -d "${OPENOCD_GIT_FOLDER}" ]
then
  cd "${OPENOCD_WORK}"

  if [ "${USER}" == "ilg" ]
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

# Configure OpenOCD. Use the same options as Freddie Chopin.

cd "${OPENOCD_BUILD_FOLDER}/openocd"

OUTPUT_DIR="${OPENOCD_BUILD_FOLDER}" \
PKG_CONFIG_PREFIX="${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit" \
\
LIBFTDI_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/include/libftdi1" \
LIBUSB1_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/include/libusb-1.0" \
LIBUSB0_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/include/libusb-win32" \
\
LIBFTDI_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/lib -lftdi1" \
LIBUSB1_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/lib -lusb-1.0" \
LIBUSB0_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/lib -lusb" \
HIDAPI_CFLAGS="-I${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/include/hidapi" \
HIDAPI_LIBS="-L${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/lib -lhidapi" \
\
PKG_CONFIG_PATH="${PKG_CONFIG_PREFIX}/lib/pkgconfig" \
\
"${OPENOCD_GIT_FOLDER}/configure" \
--build="$(uname -m)-linux-gnu" \
--host="${CROSS_COMPILE}" \
--prefix="${OPENOCD_INSTALL_FOLDER}/openocd"  \
--datarootdir="${OPENOCD_INSTALL_FOLDER}" \
--infodir="${OPENOCD_INSTALL_FOLDER}/openocd/info"  \
--localedir="${OPENOCD_INSTALL_FOLDER}/openocd/locale"  \
--mandir="${OPENOCD_INSTALL_FOLDER}/openocd/man"  \
--docdir="${OPENOCD_INSTALL_FOLDER}/openocd/doc"  \
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

# Do a clean build, with documentation.

# The bindir and pkgdatadir are required to configure bin and scripts folders
# at the same level in the hierarchy.
cd "${OPENOCD_BUILD_FOLDER}/openocd"
make bindir="bin" pkgdatadir="" clean all pdf html
${CROSS_COMPILE}-strip src/openocd.exe

# Always clear the destination folder, to have a consistent package.
rm -rfv "${OPENOCD_INSTALL_FOLDER}/openocd"

# Install, including documentation.
cd "${OPENOCD_BUILD_FOLDER}/openocd"
make install install-pdf install-html

# Copy DLLs to the install bin folder.
cp "${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/bin/"*.dll \
"${OPENOCD_INSTALL_FOLDER}/openocd/bin"

# Copy license files
mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/license"
cp -r "${OPENOCD_INSTALL_FOLDER}/${INPUT_ZIP_FOLDER}/dev_kit/license/"* \
  "${OPENOCD_INSTALL_FOLDER}/openocd/license"

find "${OPENOCD_INSTALL_FOLDER}/openocd/license" -type f \
-exec unix2dos {} \;

# Copy the GNU ARM Eclipse info files.
mkdir -p "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse"
cp "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/build-openocd-w32-cross-debian.sh" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse"
unix2dos "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/build-openocd-w32-cross-debian.sh"
cp "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/INFO-w32.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/INFO.txt"
unix2dos "${OPENOCD_INSTALL_FOLDER}/openocd/INFO.txt"
cp "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/BUILD-w32.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/BUILD.txt"
unix2dos "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/BUILD.txt"
cp "${OPENOCD_GIT_FOLDER}/gnuarmeclipse/CHANGES.txt" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/"
unix2dos "${OPENOCD_INSTALL_FOLDER}/openocd/gnuarmeclipse/CHANGES.txt"

# Not passed as it, used by makensis for the MUI_PAGE_LICENSE; must be DOS.
cp "${OPENOCD_GIT_FOLDER}/COPYING" \
  "${OPENOCD_INSTALL_FOLDER}/openocd/COPYING"
unix2dos "${OPENOCD_INSTALL_FOLDER}/openocd/COPYING"

# Create the distribution setup.

mkdir -p "${OPENOCD_OUTPUT}"

NSIS_FOLDER="${OPENOCD_GIT_FOLDER}/gnuarmeclipse/nsis"
NSIS_FILE="${NSIS_FOLDER}/gnuarmeclipse-openocd.nsi"

OPENOCD_SETUP="${OPENOCD_OUTPUT}/gnuarmeclipse-openocd-${OPENOCD_TARGET}-${OUTFILE_VERSION}-${NDATE}-setup.exe"

cd "${OPENOCD_BUILD_FOLDER}"
echo
makensis -V4 -NOCD \
-DINSTALL_FOLDER="${OPENOCD_INSTALL_FOLDER}/openocd" \
-DNSIS_FOLDER="${NSIS_FOLDER}" \
-DOUTFILE="${OPENOCD_SETUP}" \
"${NSIS_FILE}"
RESULT="$?"

echo
if [ "${RESULT}" == "0" ]
then
  echo "Build completed."
else
  echo "Build failed."
fi

exit 0
