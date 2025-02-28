#!/usr/bin/env bash

set -e
set -u
set -x

mkdir -p "${DESTDIR}"
"$@" DESTDIR="$(realpath ${DESTDIR})"
