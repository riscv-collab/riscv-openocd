#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

REPO_PATH=$(realpath "$(dirname "$0")/../..")

COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse \
         --short=8 HEAD)
COMMIT_DATE=$(git --git-dir="${REPO_PATH}/.git" --work-tree "${REPO_PATH}" \
              show -s --format=%cd --date=format:%y%m%d-%H%M%S ${COMMIT})
BRANCH=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" \
              rev-parse --abbrev-ref HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR=${ARTIFACTORY_DIR:-tools-gitlab-artifacts/openocd/$BRANCH/$COMMIT_DATE"_"$COMMIT}
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"
BINARY_TO_UPLOAD="$1"

ARTIFACT_NAME=$(basename "$1")
LINUX_NAME="linux_openocd"
WINDOWS_NAME="windows_openocd"
if [[ $ARTIFACT_NAME == $LINUX_NAME.* ]] ; then
  ARTIFACT_NAME=$(echo "$ARTIFACT_NAME" | \
    sed "s/^${LINUX_NAME}/linux_openocd_g${COMMIT}_d${COMMIT_DATE}/")
elif [[ $ARTIFACT_NAME == $WINDOWS_NAME.* ]] ; then
  ARTIFACT_NAME=$(echo "$ARTIFACT_NAME" | \
    sed "s/^${WINDOWS_NAME}/windows_openocd_g${COMMIT}_d${COMMIT_DATE}/")
else
  echo "unsupported input archive specified"
  exit 1
fi

ARTIFACTORY_PATH="${ARTIFACTORY_BASEPATH}/${ARTIFACT_NAME}"

echo "COMMIT_DATE: $COMMIT_DATE"
echo "BRANCH_NAME: ${BRANCH}"
echo "ARTIFACT_NAME: ${ARTIFACT_NAME}"

echo "Uploading \"$BINARY_TO_UPLOAD\" to \"$ARTIFACTORY_PATH\"..."
curl -H "X-JFrog-Art-Api:$ARTIFACTORY_API_KEY" -T "$BINARY_TO_UPLOAD" "$ARTIFACTORY_PATH"

