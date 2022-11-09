#!/usr/bin/env bash

#set -o errexit
#set -o pipefail
#set -o nounset

platforms=(
  spike32 \
  spike32-2 \
  spike32-2-hwthread \
  spike64 \
  spike64-2 \
  spike64-2-hwthread \
)

failures=()

RESULT="SUCCESS"
for platform in ${platforms[@]}
do
  mkdir -p ${LOGS}/${platform}

  ${ROOT}/gdbserver.py ${ROOT}/targets/RISC-V/${platform}.py  \
    --logs "${LOGS}/${platform}" \
    --print-failures             \
    --gcc "${GCC}"               \
    --gdb "${GDB}"               \
    --sim_cmd "${SIM}"           \
    --server_cmd "${OCD}" | tee "${LOGS}/${platform}.log"

  TEST_STATUS=${PIPESTATUS[0]}
  if [[ $TEST_STATUS -ne 0 ]]; then
    failures+=("${platform}")
    RESULT="FAILURE"
  fi
done


if [ "$RESULT" == "FAILURE" ]; then
  echo ""
  echo "failed platforms: ${failures[@]}"
  exit 1
fi
