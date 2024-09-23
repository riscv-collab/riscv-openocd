#!/bin/bash

INPUT="$1"
OUTPUT="$2"
PACKAGE_NAME="$3"

# ${SRC_DIR}/.makepy/support/utils/make_distr.sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
OPENOCD_SOURCES=$(dirname $(dirname $(dirname "${SCRIPT_DIR}")))
OPENOCD_PATH="${INPUT}/install_openocd/openocd"
DISTRIB_LOCATION="${OUTPUT}/${PACKAGE_NAME}"
source "${INPUT}/conanrun.sh"

echo "output: ${OUTPUT}"
set -x
set -e
mkdir -p "${DISTRIB_LOCATION}"

ACCEPTANCE_TESTS_DIR="${DISTRIB_LOCATION}/acceptance_tests"
mkdir -p "${ACCEPTANCE_TESTS_DIR}"
cp -r "${OPENOCD_SOURCES}/testsuite" "${ACCEPTANCE_TESTS_DIR}"
cp -r "${OPENOCD_SOURCES}/testing/dejagnu" "${ACCEPTANCE_TESTS_DIR}/syntacore"
cp -r "${OPENOCD_PATH}"       "${DISTRIB_LOCATION}/openocd"
cp -r "${SC_DEJAGNU_PATH}"    "${DISTRIB_LOCATION}/dejagnu"
cp -r "${SC_SPIKE_PATH}"      "${DISTRIB_LOCATION}/spike"
cp -r "${SC_RISCV_GDB_PATH}"  "${DISTRIB_LOCATION}/gdb"
cp -r "${SC_GCC_PATH}"        "${DISTRIB_LOCATION}/compiler"

cat << EOF > "${DISTRIB_LOCATION}/site.exp"
variable DEPLOYMENT_ROOT [file dirname [file normalize [info script]]]

set OPENOCD_ROOT  "\${DEPLOYMENT_ROOT}/openocd"
set SPIKE_SIM     "\${DEPLOYMENT_ROOT}/spike/bin/spike"
set GDB_BIN       "\${DEPLOYMENT_ROOT}/gdb/rv64elf/bin/riscv64-unknown-elf-gdb"
set CC_FOR_TARGET "\${DEPLOYMENT_ROOT}/compiler/bin/riscv64-unknown-elf-gcc"

source "\${DEPLOYMENT_ROOT}/acceptance_tests/syntacore/site.exp"

set OPENOCD_DEBUG_ADAPTER_CONFIG ""
if {[info exists ::env(OPENOCD_DEBUG_ADAPTER_CONFIG)]} {
  set OPENOCD_DEBUG_ADAPTER_CONFIG $::env(OPENOCD_DEBUG_ADAPTER_CONFIG)
}

set OPENOCD_DEBUG_ADAPTER_SERIAL ""
if {[info exists ::env(OPENOCD_DEBUG_ADAPTER_SERIAL)]} {
  set OPENOCD_DEBUG_ADAPTER_SERIAL $::env(OPENOCD_DEBUG_ADAPTER_SERIAL)
}

set OPENOCD_DEBUG_ADAPTER_SPEED ""
if {[info exists ::env(OPENOCD_DEBUG_ADAPTER_SPEED)]} {
  set OPENOCD_DEBUG_ADAPTER_SPEED $::env(OPENOCD_DEBUG_ADAPTER_SPEED)
}

puts "OpenOCD root: \${OPENOCD_ROOT}"
puts "Spike simulator: \${SPIKE_SIM}"
puts "GDB binary: \${GDB_BIN}"
puts "Compiler: \${CC_FOR_TARGET}"
puts ""
puts "Debug Adapter Serial: \${OPENOCD_DEBUG_ADAPTER_SERIAL}"
puts "Debug Adapter Speed: \${OPENOCD_DEBUG_ADAPTER_SPEED}"
puts "Debug Adapter Configuration: \${OPENOCD_DEBUG_ADAPTER_CONFIG}"

EOF

cat << EOF > "${DISTRIB_LOCATION}/run.sh"
#!/usr/bin/env bash

SCRIPT_DIR=\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

echo "
export DEJAGNU=\"\${SCRIPT_DIR}/site.exp\"
export OPENOCD_DEBUG_ADAPTER_SERIAL=SERIAL
export OPENOCD_DEBUG_ADAPTER_CONFIG=CONFIG
export OPENOCD_DEBUG_ADAPTER_SPEED=1000
\${SCRIPT_DIR}/dejagnu/bin/runtest \\\\
  --srcdir \"\${SCRIPT_DIR}/acceptance_tests/testsuite\" \\\\
  --out-dir=test_results \\\\
  --target_board elct_mcore2_nortos \\\\
  --tool ocd
"
EOF

chmod +x "${DISTRIB_LOCATION}/run.sh"

tar -C "${OUTPUT}" -czf "${OUTPUT}/${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"
