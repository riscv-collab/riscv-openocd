#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o xtrace

NPROC=$1

BUILDDIR=$2
SCRIPTDIR=`dirname $0`
SRCDIR=`realpath --canonicalize-missing --relative-to ${BUILDDIR} ${SCRIPTDIR}/../../..`

./bootstrap nosubmodule
mkdir -p ${BUILDDIR}
cd ${BUILDDIR}
${SRCDIR}/configure --enable-dummy
make -j ${NPROC}
make check
