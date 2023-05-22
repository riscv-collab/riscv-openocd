#!/bin/bash

set -x -e
URL_BASE="ftp://""${NAS_USR}":"${NAS_PSW}"@"${BITSTREAM_FTP_STORAGE}"

BITSTREAM_ARCH="$1"
if [[ -z $BITSTREAM_ARCH ]]
then
  echo "no arguments is passed, listing storage..."
  lftp -e "ls ; exit " "$URL_BASE"
  exit 0
fi

TMPDIR=$(mktemp -d --tmpdir=$(pwd))
echo $TMPDIR

pushd "$TMPDIR"

lftp -e "get $1 ; exit " "$URL_BASE"

echo "got file, trying to unarchive"

if [[ "$1" == *.rar ]]
then
  unrar x "$1"
else
  tar -xvf "$1"
fi

# yes, yes, I know. we should use "-print0 here". I don't care.
FOUND_BITSTREAMS=$(find $(pwd) -type f -name "arty*top.bit")
FOUND_FILES_COUNT=$(echo -n "$FOUND_BITSTREAMS" | wc -l)

if [[ "$FOUND_FILES_COUNT" -ne 0 ]]
then
  echo "ERROR: could not uniquely identify bitstream"
  exit -1
fi

popd

cp $FOUND_BITSTREAMS "$BITSTREAM_ARCH".bitstream.bit
