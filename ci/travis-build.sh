#!/bin/sh
set -e -x

die() {
  echo "$1" >&2
  exit 1
}

ticker() {
  while true
  do
    sleep 60
    date -R
    [ -r "$1" ] && tail -n10 "$1"
  done
}

CACHEKEY=1

EPICS_HOST_ARCH=`sh startup/EpicsHostArch`

[ -e configure/os/CONFIG_SITE.Common.linux-x86 ] || die "Wrong location: $PWD"

case "$CMPLR" in
clang)
  echo "Host compiler is clang"
  cat << EOF >> configure/os/CONFIG_SITE.Common.$EPICS_HOST_ARCH
GNU         = NO
CMPLR_CLASS = clang
CC          = clang
CCC         = clang++
EOF
  ;;
*) echo "Host compiler is default";;
esac

if [ "$STATIC" = "YES" ]
then
  echo "Build static libraries/executables"
  cat << EOF >> configure/CONFIG_SITE
SHARED_LIBRARIES=NO
STATIC_BUILD=YES
EOF
fi

# requires wine and g++-mingw-w64-i686
if [ "$WINE" = "32" ]
then
  echo "Cross mingw32"
  sed -i -e '/CMPLR_PREFIX/d' configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
  cat << EOF >> configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
CMPLR_PREFIX=i686-w64-mingw32-
EOF
  cat << EOF >> configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=win32-x86-mingw
EOF
fi

# set RTEMS to eg. "4.9" or "4.10"
# requires qemu, bison, flex, texinfo, install-info
if [ -n "$RTEMS" ]
then
  echo "Cross RTEMS${RTEMS} for pc386"
  install -d /home/travis/.cache
  curl -L "https://github.com/mdavidsaver/rsb/releases/download/travis-20160306-2/rtems${RTEMS}-i386-trusty-20190306-2.tar.gz" \
  | tar -C /home/travis/.cache -xj

  sed -i -e '/^RTEMS_VERSION/d' -e '/^RTEMS_BASE/d' configure/os/CONFIG_SITE.Common.RTEMS
  cat << EOF >> configure/os/CONFIG_SITE.Common.RTEMS
RTEMS_VERSION=$RTEMS
RTEMS_BASE=/home/travis/.cache/rtems${RTEMS}-i386
EOF
  cat << EOF >> configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=RTEMS-pc386
EOF

  # find local qemu-system-i386
  export PATH="$HOME/.cache/qemu/usr/bin:$PATH"
  echo -n "Using QEMU: "
  type qemu-system-i386 || echo "Missing qemu"
  EXTRA=RTEMS_QEMU_FIXUPS=YES
fi

make -j2 $EXTRA

if [ "$TEST" != "NO" ]
then
   make tapfiles
   find . -name '*.tap' -print0 | xargs -0 -n1 prove -e cat -f
fi
