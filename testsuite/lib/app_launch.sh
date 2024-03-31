#/bin/sh

APP="$1"
VALGRIND="$2"
APP_LOG="$3"
shift
shift
shift

STREAMS_DIR="streams"
STDERR_STREAM="${STREAMS_DIR}/stderr_${APP_LOG}"
STDOUT_STREAM="${STREAMS_DIR}/stdout_${APP_LOG}"
mkdir -p "${STREAMS_DIR}"

exec 2>"${STDERR_STREAM}"
if [ -z "${VALGRIND}" ]
then
  exec "${APP}" "$@" >"${STDOUT_STREAM}"
else
  exec "${VALGRIND}" "${APP}" "$@" >"${STDOUT_STREAM}"
fi
