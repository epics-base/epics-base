#!/bin/sh
set -e -x

die() {
  echo "$1" >&2
  exit 1
}


if [ -f /etc/hosts ]
then
    echo "==== /etc/hosts"
    cat /etc/hosts
    echo "===="
fi
