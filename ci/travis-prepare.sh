#!/bin/sh
set -e -x

die() {
  echo "$1" >&2
  exit 1
}

if [ -f /etc/hosts ]
then
    # The travis-ci "bionic" image throws us a curveball in /etc/hosts
    # by including two entries for localhost.  The first for 127.0.1.1
    # which causes epicsSockResolveTest to fail.
    #  cat /etc/hosts
    #  ...
    #  127.0.1.1 localhost localhost ip4-loopback
    #  127.0.0.1 localhost nettuno travis vagrant travis-job-....

    sudo sed -i -e '/^127\.0\.1\.1/ s|localhost\s*||g' /etc/hosts

    echo "==== /etc/hosts"
    cat /etc/hosts
    echo "===="
fi
