#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

LOGS_PATH=$1
STAND_ID=$2
API_KEY=$3

REPO_PATH=$(realpath "$(dirname "$0")/../..")
COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse --short=8 HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR=${ARTIFACTORY_DIR:-openocd_test_reports}
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"

ARCHIVE_NAME="${STAND_ID}_${COMMIT}_success.txt"
ARTIFACTORY_PATH="$ARTIFACTORY_BASEPATH/$ARCHIVE_NAME"

echo "Great Success!" > $ARCHIVE_NAME
echo "Uploading \"$ARCHIVE_NAME\" to \"$ARTIFACTORY_PATH\"..."
curl  -H "X-JFrog-Art-Api:$API_KEY" -T "$ARCHIVE_NAME" "$ARTIFACTORY_PATH"
rm "$ARCHIVE_NAME"

