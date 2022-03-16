#!/bin/bash

CHECKOUT_DIR=`pwd`/tools/riscv-openocd-ci/work/riscv-tests

# Fail on first error.
set -e

# Echo commands.
set -o xtrace

# Checkout riscv-tests.
mkdir -p "$CHECKOUT_DIR"
cd "$CHECKOUT_DIR"
git clone --recursive https://github.com/riscv/riscv-tests .

# Run the debug tests.
# Do not stop even on a failed test.
# Use slightly more jobs than CPUs. Observed that this still speeds up the testing.
cd debug
JOBS=$(($(nproc) + 2))
make -k -j$JOBS all || true
