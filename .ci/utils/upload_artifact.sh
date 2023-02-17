#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

REPO_PATH=$(realpath "$(dirname "$0")/../..")

COMMIT=$(git --git-dir="$REPO_PATH/.git" --work-tree "$REPO_PATH" rev-parse --short HEAD)
ARTIFACTORY_URL="http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR=${ARTIFACTORY_DIR:-tools-gitlab-artifacts/openocd/development_builds/$COMMIT}
ARTIFACTORY_BASEPATH="$ARTIFACTORY_URL/$ARTIFACTORY_DIR"
NAME=$(basename "$1")
ARCHIVE_NAME=$NAME.tar.gz
SKIP_ARCHIVATION=${SKIP_ARCHIVATION:-false}
ARTIFACT_NAME=$([[ "$SKIP_ARCHIVATION" != "true" ]] && echo "$ARCHIVE_NAME" || echo "$NAME")
ARTIFACTORY_PATH="$ARTIFACTORY_BASEPATH/$ARTIFACT_NAME"
ARTIFACT_PATH=$([[ "$SKIP_ARCHIVATION" != "true" ]] && echo "$ARCHIVE_NAME" || echo "$1")

[[ "$SKIP_ARCHIVATION" == "true" ]] || tar -czvf "$ARCHIVE_NAME" "$@"
echo "Uploading \"$ARTIFACT_PATH\" to \"$ARTIFACTORY_PATH\"..."
curl -H "X-JFrog-Art-Api:$ARTIFACTORY_API_KEY" -T "$ARTIFACT_PATH" "$ARTIFACTORY_PATH"
[[ "$SKIP_ARCHIVATION" == "true" ]] || rm "$ARCHIVE_NAME"

