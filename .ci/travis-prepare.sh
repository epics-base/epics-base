#!/bin/sh
set -e -x

CURDIR="$PWD"

QDIR="$HOME/.cache/qemu"

if [ -n "$RTEMS" -a "$TEST" = "YES" ]
then
  git clone --quiet --branch vme --depth 10 https://github.com/mdavidsaver/qemu.git "$HOME/.build/qemu"
  cd "$HOME/.build/qemu"

  HEAD=`git log -n1 --pretty=format:%H`
  echo "HEAD revision $HEAD"

  [ -e "$HOME/.cache/qemu/built" ] && BUILT=`cat "$HOME/.cache/qemu/built"`
  echo "Cached revision $BUILT"

  if [ "$HEAD" != "$BUILT" ]
  then
    echo "Building QEMU"
    git submodule --quiet update --init

    install -d "$HOME/.build/qemu/build"
    cd         "$HOME/.build/qemu/build"

    "$HOME/.build/qemu/configure" --prefix="$HOME/.cache/qemu/usr" --target-list=i386-softmmu --disable-werror
    make -j2
    make install

    echo "$HEAD" > "$HOME/.cache/qemu/built"
  fi
fi

cd "$CURDIR"

cat << EOF > configure/RELEASE.local
EPICS_BASE=$HOME/.source/epics-base
EOF

install -d "$HOME/.source"
cd "$HOME/.source"

git clone --quiet --depth 5 --branch core/"${BRCORE:-master}" https://github.com/${REPOBASE:-epics-base}/epics-base.git epics-base
(cd epics-base && git log -n1 )

EPICS_HOST_ARCH=`sh epics-base/startup/EpicsHostArch`

# requires wine and g++-mingw-w64-i686
if [ "$WINE" = "32" ]
then
  echo "Cross mingw32"
  sed -i -e '/CMPLR_PREFIX/d' epics-base/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
CMPLR_PREFIX=i686-w64-mingw32-
EOF
  cat << EOF >> epics-base/configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=win32-x86-mingw
EOF
fi

if [ "$STATIC" = "YES" ]
then
  echo "Build static libraries/executables"
  cat << EOF >> epics-base/configure/CONFIG_SITE
SHARED_LIBRARIES=NO
STATIC_BUILD=YES
EOF
fi

case "$CMPLR" in
clang)
  echo "Host compiler is clang"
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.Common.$EPICS_HOST_ARCH
GNU         = NO
CMPLR_CLASS = clang
CC          = clang
CCC         = clang++
EOF

  # hack
  sed -i -e 's/CMPLR_CLASS = gcc/CMPLR_CLASS = clang/' epics-base/configure/CONFIG.gnuCommon

  clang --version
  ;;
*)
  echo "Host compiler is default"
  gcc --version
  ;;
esac

cat <<EOF >> epics-base/configure/CONFIG_SITE
USR_CPPFLAGS += $USR_CPPFLAGS
USR_CFLAGS += $USR_CFLAGS
USR_CXXFLAGS += $USR_CXXFLAGS
EOF

# set RTEMS to eg. "4.9" or "4.10"
# requires qemu, bison, flex, texinfo, install-info
if [ -n "$RTEMS" ]
then
  echo "Cross RTEMS${RTEMS} for pc386"
  install -d /home/travis/.cache
  curl -L "https://github.com/mdavidsaver/rsb/releases/download/travis-20160306-2/rtems${RTEMS}-i386-trusty-20190306-2.tar.gz" \
  | tar -C /home/travis/.cache -xj

  sed -i -e '/^RTEMS_VERSION/d' -e '/^RTEMS_BASE/d' epics-base/configure/os/CONFIG_SITE.Common.RTEMS
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.Common.RTEMS
RTEMS_VERSION=$RTEMS
RTEMS_BASE=/home/travis/.cache/rtems${RTEMS}-i386
EOF
  cat << EOF >> epics-base/configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=RTEMS-pc386
EOF

  # find local qemu-system-i386
  export PATH="$HOME/.cache/qemu/usr/bin:$PATH"
  echo -n "Using QEMU: "
  type qemu-system-i386 || echo "Missing qemu"
  EXTRA=RTEMS_QEMU_FIXUPS=YES
fi

make -j2 -C epics-base $EXTRA
