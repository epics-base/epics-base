#!/bin/sh
set -e -x

die() {
  echo "$1" >&2
  exit 1
}

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
