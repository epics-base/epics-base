#!/bin/sh
set -e -x

cat << EOF > configure/RELEASE.local
EPICS_BASE=$HOME/.source/epics-base
EOF

install -d "$HOME/.source"
cd "$HOME/.source"

add_base_module()
{
MODULE=$1
BRANCH=$2
( cd epics-base/modules && \
git clone --quiet --depth 5 --branch "$MODULE"/"$BRANCH" https://github.com/${REPOBASE:-epics-base}/epics-base.git "$MODULE" && \
cd "$MODULE" && \
git log -n1 )
}

git clone --quiet --depth 5 --branch core/"$BRCORE" https://github.com/${REPOBASE:-epics-base}/epics-base.git epics-base
( cd epics-base && git log -n1 )
add_base_module libcom "$BRLIBCOM"
add_base_module ca "$BRCA"

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

make -j2 -C epics-base
