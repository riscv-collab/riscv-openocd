#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

API_KEY=$1

REPO_PATH=$(realpath "$(dirname "$0")/../..")
COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse --short HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR=${ARTIFACTORY_DIR:-openocd_test_reports}
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"

function check_build_status {
  STAND_NAME=$1
  EXPECTED_PATH="${ARTIFACTORY_BASEPATH}/${STAND_NAME}_${COMMIT}_success.txt"
  echo "trying to get $EXPECTED_PATH"
  echo "$(curl -H "X-JFrog-Art-Api:$API_KEY" "${EXPECTED_PATH}")" | grep "Great Success"
}

check_build_status zalman
check_build_status twin_server
