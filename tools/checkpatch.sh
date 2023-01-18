#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

since=${1:-HEAD^}
git format-patch -M --stdout $since | tools/scripts/checkpatch.pl -
