#!/bin/bash

TOOLCHAIN_URL="https://buildbot.embecosm.com/job/riscv32-gcc-ubuntu1804"
TOOLCHAIN_URL+="/25/artifact/riscv32-embecosm-gcc-ubuntu1804-20201108.tar.gz"
ARCHIVE_NAME=${TOOLCHAIN_URL##*/}
DOWNLOAD_DIR=`pwd`/tools/riscv-openocd-ci/work
INSTALL_DIR=`pwd`/tools/riscv-openocd-ci/work/install

# Fail on first error.
set -e

# Echo commands.
set -o xtrace

# Download the toolchain.
# Use a pre-built toolchain binaries provided by Embecosm: https://buildbot.embecosm.com/
mkdir -p "$DOWNLOAD_DIR"
cd "$DOWNLOAD_DIR"
wget --progress dot:mega "$TOOLCHAIN_URL"

# Extract
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"
tar xvf "$DOWNLOAD_DIR/$ARCHIVE_NAME" --strip-components=1

# Make symlinks: riscv64-* --> riscv32-*
cd "$INSTALL_DIR/bin"
find . -name 'riscv32-*' | while read F; do ln -s $F $(echo $F | sed -e 's/riscv32/riscv64/'); done

# Check that the compiler runs
./riscv64-unknown-elf-gcc --version
