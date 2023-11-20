#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset

RUN_BEFORE_HOUR="$1"

re='^[0-9]+$'
if ! [[ $RUN_BEFORE_HOUR =~ $re ]] ; then
  echo "the specified hour $RUN_BEFORE_HOUR is not a number, skipping checks"
  exit 0
fi

CURRENT_HOUR=$(env TZ=Europe/Moscow date +"%H")

echo "current hour: ${CURRENT_HOUR}"
if [ $CURRENT_HOUR -ge $RUN_BEFORE_HOUR ]; then
  echo "current hour ${CURRENT_HOUR} is greater than ${RUN_BEFORE_HOUR}"
  exit 1
fi

exit 0
