#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

LOGS_PATH=$1
BUILD_ID=$2
API_KEY=$3

REPO_PATH=$(realpath "$(dirname "$0")/../..")
COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse --short HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR=${ARTIFACTORY_DIR:-openocd_build_dependencies/test_logs}
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"

ARCHIVE_NAME=${BUILD_ID:-${COMMIT}_test}.tar.xz
ARTIFACTORY_PATH="$ARTIFACTORY_BASEPATH/$ARCHIVE_NAME"

LOGS_PARENT=$(dirname "$LOGS_PATH")
LOGS_DIR=$(basename "$LOGS_PATH")

CPU_COUNT=$(grep -c ^processor /proc/cpuinfo)
XZ_OPT="-9 -T$CPU_COUNT" tar -cJf "$ARCHIVE_NAME" -C "$LOGS_PARENT" "$LOGS_DIR"
echo "Uploading \"$ARCHIVE_NAME\" to \"$ARTIFACTORY_PATH\"..."
curl  -H "X-JFrog-Art-Api:$API_KEY" -T "$ARCHIVE_NAME" "$ARTIFACTORY_PATH"
rm "$ARCHIVE_NAME"

