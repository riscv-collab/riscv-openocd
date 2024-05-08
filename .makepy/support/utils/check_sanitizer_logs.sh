#!/usr/bin/env bash
#
set -o errexit
set -o nounset

TEST_DIRECTORY="$1"
TD_LEN=$(($(echo -n "${TEST_DIRECTORY}/" | wc -c)+1))

UBSAN_ERRS_FILE="${TEST_DIRECTORY}/ubsan.errs"
VALGRIND_ERRS_FILE="${TEST_DIRECTORY}/valgrind.errs"

find "${TEST_DIRECTORY}" -name "*.log" -type f -exec  grep --text -H -R "runtime error" {} \; \
  | grep jim.c -v \
  | sort \
  | cut -c${TD_LEN}- >"${UBSAN_ERRS_FILE}"
UBSAN_COUNT=$(cat "${UBSAN_ERRS_FILE}" | wc -l)

find "${TEST_DIRECTORY}" -name "*.log" -type f -exec  grep --text -H -R "== ERROR SUMMARY:" {} \; \
  | grep " 0 errors from" -v \
  | sort \
  | cut -c${TD_LEN}- >"${VALGRIND_ERRS_FILE}"
VALGRIND_COUNT=$(cat "${VALGRIND_ERRS_FILE}" | wc -l)


RETURN_CODE=0
if [ "$UBSAN_COUNT" -ne 0 ] ;
then
  RETURN_CODE=1
  echo "ERROR: $UBSAN_COUNT unexpected sanitizer errors detected!"
  cat "${UBSAN_ERRS_FILE}"
else
  echo "SUCCESS: no unexpected sanitizer errors detected"
fi

if [ "$VALGRIND_COUNT" -ne 0 ] ;
then
  RETURN_CODE=1
  echo "ERROR: $VALGRIND_COUNT unexpected valgrind errors detected!"
  cat "${VALGRIND_ERRS_FILE}"
else
  echo "SUCCESS: no unexpected valgrind errors detected"
fi

exit ${RETURN_CODE}
