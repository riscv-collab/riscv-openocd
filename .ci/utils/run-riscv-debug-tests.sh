#!/usr/bin/env bash

#set -o errexit
#set -o pipefail
#set -o nounset

RESULT="SUCCESS"

mkdir -p ${LOGS}/${TGT}

${ROOT}/gdbserver.py ${ROOT}/targets/RISC-V/${TGT}.py  \
  --logs "${LOGS}/${TGT}" \
  --print-failures             \
  --gcc "${GCC}"               \
  --gdb "${GDB}"               \
  --sim_cmd "${SIM}"           \
  --server_cmd "${OCD}" | tee "${LOGS}/${TGT}.log"

TEST_STATUS=${PIPESTATUS[0]}
if [[ $TEST_STATUS -ne 0 ]]; then
  RESULT="FAILURE"
fi


if [ "$RESULT" == "FAILURE" ]; then
  echo ""
  echo "failed platforms: ${failures[@]}"
  exit 1
fi
