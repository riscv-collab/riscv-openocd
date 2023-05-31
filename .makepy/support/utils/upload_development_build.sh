#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

REPO_PATH=$(realpath "$(dirname "$0")/../../..")

COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse --short=8 HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR="tools-gitlab-artifacts/openocd/development_builds/$COMMIT"
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"
ARTIFACT_NAME=$(basename "$1")
ARTIFACTORY_PATH="$ARTIFACTORY_BASEPATH/$ARTIFACT_NAME"
BINARY_TO_UPLOAD="$1"
ARTIFACTORY_API_KEY="$2"

set -x
echo "Uploading \"$BINARY_TO_UPLOAD\" to \"$ARTIFACTORY_PATH\"..."
curl -H "X-JFrog-Art-Api:$ARTIFACTORY_API_KEY" -T "$BINARY_TO_UPLOAD" "$ARTIFACTORY_PATH"
