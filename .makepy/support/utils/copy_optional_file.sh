#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset

FILE="$1"
DEST="$2"

if [ -z "${FILE}" ]; then
  exit 0
fi

echo "copying $FILE to $DEST..."
cp "${FILE}" "${DEST}"
echo "done"
