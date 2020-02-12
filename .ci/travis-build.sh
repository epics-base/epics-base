#!/bin/sh
set -e -x

die() {
  echo "$1" >&2
  exit 1
}

CACHEKEY=1

export EPICS_HOST_ARCH=`perl src/tools/EpicsHostArch.pl`

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
if [ -n "$RTEMS" ]
then
  echo "Cross RTEMS${RTEMS} for pc386"
  curl -L "https://github.com/mdavidsaver/rsb/releases/download/20171203-${RTEMS}/i386-rtems${RTEMS}-trusty-20171203-${RTEMS}.tar.bz2" \
  | tar -C / -xmj

  sed -i -e '/^RTEMS_VERSION/d' -e '/^RTEMS_BASE/d' configure/os/CONFIG_SITE.Common.RTEMS
  cat << EOF >> configure/os/CONFIG_SITE.Common.RTEMS
RTEMS_VERSION=$RTEMS
RTEMS_BASE=$HOME/.rtems
EOF
  cat << EOF >> configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS += RTEMS-pc386-qemu
EOF

  # find local qemu-system-i386
  echo -n "Using QEMU: "
  type qemu-system-i386 || echo "Missing qemu"
fi

make -j2 RTEMS_QEMU_FIXUPS=YES CMD_CFLAGS="${CMD_CFLAGS}" CMD_CXXFLAGS="${CMD_CXXFLAGS}" CMD_LDFLAGS="${CMD_LDFLAGS}"

if [ "$TEST" != "NO" ]
then
   export EPICS_TEST_IMPRECISE_TIMING=YES
   make -j2 tapfiles
   make -s test-results
fi
