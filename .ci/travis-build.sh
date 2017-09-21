#!/bin/sh
set -e -x

make -j2

if [ "$TEST" != "NO" ]
then
  make tapfiles
  find . -name '*.tap' -print0 | xargs -0 -n1 prove -e cat -f
fi
