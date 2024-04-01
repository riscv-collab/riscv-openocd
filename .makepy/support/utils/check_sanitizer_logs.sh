#!/usr/bin/env bash
#
set -o errexit
set -o nounset

TEST_DIRECTORY="$1"
UBSAN_COUNT=$(find "$TEST_DIRECTORY" -name "*.log" -type f -exec  grep --text -R "runtime error" {} \; | grep jim.c -v | wc -l)

if [ "$UBSAN_COUNT" -ne 0 ] ;
then
  echo "ERROR: $UBSAN_COUNT unexpected sanitizer errors detected!"
  exit 1
fi

echo "SUCCESS: no unexpected sanitizer errors detected"

VALGRIND_COUNT=$(find "$TEST_DIRECTORY" -name "*.log" -type f -exec  grep --text -R "== ERROR SUMMARY:" {} \; | grep "0 errors from" -v | wc -l)

if [ "$VALGRIND_COUNT" -ne 0 ] ;
then
  echo "ERROR: $VALGRIND_COUNT unexpected valgrind errors detected!"
  exit 1
fi

echo "SUCCESS: no unexpected valgrind errors detected"
