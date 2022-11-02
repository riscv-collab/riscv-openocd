#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

PLATFORM=$1
INSTALLATION_ROOT=$2
DEPS_INSTALL=$3

cd $INSTALLATION_ROOT

case $PLATFORM in
  Linux)
    ARCHIVE=linux_openocd.tar.gz
    [ -f $ARCHIVE ] && rm $ARCHIVE
    tar -czvf $ARCHIVE openocd
    ;;

  Windows)
    ARCHIVE=windows_openocd.zip
    [ -f $ARCHIVE ] && rm $ARCHIVE
    zip -r $ARCHIVE openocd
    ;;

  *)
     echo "ERROR: unknown platform <${PLATFORM}>"
     exit 1
esac

echo $ARCHIVE
