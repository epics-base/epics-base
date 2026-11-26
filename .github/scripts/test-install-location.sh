#!/bin/bash
#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
#
# Build Base and downstream modules (instantiated from the makeBaseApp
# "example" template) with different ways of setting INSTALL_LOCATION,
# then check the library paths embedded in the installed executables and
# shared libraries (RPATH/RUNPATH).
#
# Usage: test-install-location.sh <base-method> <rpath-mode> <work-dir>
#
# <base-method> selects how the settings are given for the Base build:
#   file     - appended to configure/CONFIG_SITE (which gets installed)
#   cmdline  - on the make command line
#   both     - in configure/CONFIG_SITE with decoy values *and* on the
#              make command line with the real values (command line wins)
#
# <rpath-mode> selects which settings are used:
#   install  - INSTALL_LOCATION (with the default LINKER_USE_RPATH=YES),
#              the RPATH points into INSTALL_LOCATION
#   final    - INSTALL_LOCATION and FINAL_LOCATION; after building, the
#              install tree is moved from INSTALL_LOCATION (staging) to
#              FINAL_LOCATION, and everything is checked there
#   origin   - INSTALL_LOCATION and LINKER_USE_RPATH=ORIGIN; the Base
#              install tree is also relocated to check that it still works
#
# Against the installed Base (EPICS_BASE in configure/RELEASE pointing to
# the installed copy), downstream modules are built with each of
#   none     - the module sets nothing (installs into its TOP)
#   file     - settings in the module's configure/CONFIG_SITE
#   cmdline  - settings on the make command line
#   both     - decoys in the file, real values on the command line
# always using install locations that differ from Base's.
#
# Must be run from the top of a Base source tree without the PVA
# submodules checked out (minimal Base build). Linux (ELF) only.
# configure/CONFIG_SITE is modified during the run and restored at exit.

set -e -o pipefail

BASE_METHOD=$1
MODE=$2
WORK=$3

usage() {
    echo "Usage: $0 <file|cmdline|both> <install|final|origin> <work-dir>" >&2
    exit 2
}
[ -n "$WORK" ] || usage
case "$BASE_METHOD" in file|cmdline|both) ;; *) usage ;; esac
case "$MODE" in install|final|origin) ;; *) usage ;; esac

BASE_SRC=$(pwd)
if [ ! -f "$BASE_SRC/configure/CONFIG_BASE" ]; then
    echo "Must be run from the top of an EPICS Base source tree" >&2
    exit 2
fi
if [ -f "$BASE_SRC/modules/pvData/Makefile" ]; then
    echo "PVA submodules are checked out; this test wants a minimal Base" >&2
    exit 2
fi

mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd -P)

EPICS_HOST_ARCH=$(perl "$BASE_SRC/src/tools/EpicsHostArch.pl")
export EPICS_HOST_ARCH
ARCH=$EPICS_HOST_ARCH
MAKE="make -j$(nproc)"
APP_NAME=demo

cp "$BASE_SRC/configure/CONFIG_SITE" "$WORK/CONFIG_SITE.orig"
trap 'cp "$WORK/CONFIG_SITE.orig" "$BASE_SRC/configure/CONFIG_SITE"' EXIT

FAILED=0
fail() { echo "FAIL: $*"; FAILED=$((FAILED+1)); }
pass() { echo "ok: $*"; }

# configure <top> <method> <prefix>
# Sets up configure/CONFIG_SITE of <top> and the global array MAKEARGS for
# building with <method>. Installs into $WORK/<prefix>-install, which is
# finally deployed to $WORK/<prefix>-final in "final" mode.
# Decoy values (method "both") use $WORK/<prefix>-decoy-*.
configure() {
    local top=$1 method=$2 prefix=$3
    local inst=$WORK/$prefix-install fin=$WORK/$prefix-final
    local real=() decoy=()
    real+=("INSTALL_LOCATION=$inst")
    decoy+=("INSTALL_LOCATION=$WORK/$prefix-decoy-install")
    case "$MODE" in
    final)
        real+=("FINAL_LOCATION=$fin")
        decoy+=("FINAL_LOCATION=$WORK/$prefix-decoy-final")
        ;;
    origin)
        real+=("LINKER_USE_RPATH=ORIGIN")
        decoy+=("LINKER_USE_RPATH=ORIGIN")
        ;;
    esac
    MAKEARGS=()
    case "$method" in
    file)
        printf '%s\n' "" "# Added by $(basename "$0")" "${real[@]}" \
            >> "$top/configure/CONFIG_SITE" ;;
    cmdline)
        MAKEARGS=("${real[@]}") ;;
    both)
        printf '%s\n' "" "# Added by $(basename "$0")" "${decoy[@]}" \
            >> "$top/configure/CONFIG_SITE"
        MAKEARGS=("${real[@]}") ;;
    esac
    echo "--- configure/CONFIG_SITE additions:"
    sed -n '/^# Added by/,$p' "$top/configure/CONFIG_SITE"
    echo "--- make command line: ${MAKEARGS[*]}"
}

# Move a staged install tree to its final location ("final" mode)
deploy() {
    local prefix=$1
    if [ "$MODE" = final ]; then
        rm -rf "${WORK:?}/$prefix-final"
        mv "$WORK/$prefix-install" "$WORK/$prefix-final"
        echo "Deployed $WORK/$prefix-install to $WORK/$prefix-final"
    fi
}

# Where an install tree ends up being used
location() {
    if [ "$MODE" = final ]; then
        echo "$WORK/$1-final"
    else
        echo "$WORK/$1-install"
    fi
}

# check_rpath <file> <origin-root> <dir>...
# The RPATH/RUNPATH entries of <file> must resolve to exactly the given
# library <dir>s. If $ORIGIN_EXPECTED is "yes", entries for libraries
# under <origin-root> must be $ORIGIN-relative, all others absolute.
check_rpath() {
    local file=$1 root=$2; shift 2
    local name entries e resolved dir found
    name=$(basename "$file")
    if [ ! -f "$file" ]; then
        fail "$file does not exist"
        return
    fi
    entries=$(readelf -d "$file" |
        sed -n 's/.*(R\(UN\)\?PATH).*\[\(.*\)\]/\2/p' | tr ':' '\n')
    echo "RPATH/RUNPATH of $file:"
    echo "$entries" | sed 's/^/    /'
    declare -A have=()
    while read -r e; do
        [ -z "$e" ] && continue
        resolved=$(realpath -m "${e//\$ORIGIN/$(dirname "$file")}")
        have[$resolved]=$e
        found=no
        for dir in "$@"; do
            [ "$resolved" = "$dir" ] && found=yes
        done
        [ $found = yes ] || fail "$name has unexpected entry $e"
    done <<< "$entries"
    for dir in "$@"; do
        e=${have[$dir]}
        if [ -z "$e" ]; then
            fail "$name is missing $dir"
        elif [ "$ORIGIN_EXPECTED" = yes ] && [[ $dir == "$root"/* ]] &&
                [[ $e != '$ORIGIN'* ]]; then
            fail "$name has absolute entry $e, expected \$ORIGIN-relative"
        elif [[ $dir != "$root"/* || $ORIGIN_EXPECTED != yes ]] &&
                [[ $e == '$ORIGIN'* ]]; then
            fail "$name has relative entry $e, expected absolute"
        else
            pass "$name has $e"
        fi
    done
    if ldd "$file" | grep -q "not found"; then
        ldd "$file"
        fail "$name has unresolved libraries"
    else
        pass "$name resolves all libraries"
    fi
}

check_absent() {
    if [ -e "$1" ]; then
        fail "$1 exists but should not"
    else
        pass "$1 does not exist"
    fi
}

# Start the IOC with a record of the type from the support library
run_ioc() {
    local exe=$1 dbd=$2 out
    echo 'record(xxx, "demo:xxx") {}' > "$WORK/demo.db"
    if out=$(printf '%s\n' "dbLoadDatabase \"$dbd\"" \
            "${APP_NAME}_registerRecordDeviceDriver pdbbase" \
            "dbLoadRecords \"$WORK/demo.db\"" "iocInit" "dbl" "exit" |
            EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 "$exe" 2>&1); then
        echo "$out" | sed 's/^/    /'
        # dbl output may follow a prompt on the same line
        if echo "$out" | grep -q "iocRun: All initialization complete" &&
            echo "$out" | grep -q "demo:xxx\$"; then
            pass "$(basename "$exe") runs"
            return
        fi
    else
        echo "$out" | sed 's/^/    /'
    fi
    fail "$(basename "$exe") did not run"
}

echo "=== Base method: $BASE_METHOD, mode: $MODE, host arch: $ARCH"
rm -rf "$WORK"/base-* "$WORK"/app-*

echo "=== Building Base"
configure "$BASE_SRC" "$BASE_METHOD" base
(set -x; $MAKE -C "$BASE_SRC" "${MAKEARGS[@]}")
deploy base
BASE=$(location base)
BASE_LIB=$BASE/lib/$ARCH

echo "=== Checking Base"
ORIGIN_EXPECTED=no
[ "$MODE" = origin ] && ORIGIN_EXPECTED=yes
check_rpath "$BASE/bin/$ARCH/caget" "$BASE" "$BASE_LIB"
check_rpath "$BASE/bin/$ARCH/softIoc" "$BASE" "$BASE_LIB"
check_rpath "$BASE_LIB/libdbCore.so" "$BASE" "$BASE_LIB"
check_absent "$BASE_SRC/bin"
check_absent "$BASE_SRC/lib"
check_absent "$WORK/base-decoy-install"
[ "$MODE" = final ] && check_absent "$WORK/base-install"
if "$BASE/bin/$ARCH/caget" -h > /dev/null 2>&1; then
    pass "caget runs"
else
    fail "caget does not run"
fi
if diff -q "$BASE_SRC/configure/CONFIG_SITE" "$BASE/configure/CONFIG_SITE"; then
    pass "configure/CONFIG_SITE installed"
else
    fail "installed configure/CONFIG_SITE differs from the source"
fi

for APP_METHOD in none file cmdline both; do
    echo "=== Downstream module, method: $APP_METHOD"
    APP_SRC=$WORK/app-$APP_METHOD-src
    mkdir -p "$APP_SRC"
    (cd "$APP_SRC" && set -x &&
        "$BASE/bin/$ARCH/makeBaseApp.pl" -b "$BASE" -t example $APP_NAME)
    # EPICS_BASE must point to the installed copy of Base
    sed -i "s|^EPICS_BASE *=.*|EPICS_BASE = $BASE|" "$APP_SRC/configure/RELEASE"
    grep '^EPICS_BASE' "$APP_SRC/configure/RELEASE"

    if [ "$APP_METHOD" = none ]; then
        MAKEARGS=()
        APP=$APP_SRC
    else
        configure "$APP_SRC" "$APP_METHOD" "app-$APP_METHOD"
        APP=$(location "app-$APP_METHOD")
    fi
    if ! (set -x; $MAKE -C "$APP_SRC" "${MAKEARGS[@]}"); then
        fail "build of downstream module ($APP_METHOD) failed"
        continue
    fi
    [ "$APP_METHOD" != none ] && deploy "app-$APP_METHOD"
    APP_LIB=$APP/lib/$ARCH

    echo "=== Checking downstream module, method: $APP_METHOD"
    # A module that sets nothing inherits LINKER_USE_RPATH from
    # Base's installed configure/CONFIG_SITE
    ORIGIN_EXPECTED=no
    if [ "$MODE" = origin ] &&
        [ "$APP_METHOD" != none -o "$BASE_METHOD" != cmdline ]; then
        ORIGIN_EXPECTED=yes
    fi
    check_rpath "$APP/bin/$ARCH/$APP_NAME" "$APP" "$APP_LIB" "$BASE_LIB"
    check_rpath "$APP_LIB/lib${APP_NAME}Support.so" "$APP" "$APP_LIB" "$BASE_LIB"
    if [ "$APP_METHOD" != none ]; then
        check_absent "$APP_SRC/bin"
        check_absent "$APP_SRC/lib"
        check_absent "$WORK/app-$APP_METHOD-decoy-install"
        [ "$MODE" = final ] && check_absent "$WORK/app-$APP_METHOD-install"
    fi
    run_ioc "$APP/bin/$ARCH/$APP_NAME" "$APP/dbd/$APP_NAME.dbd"
done

if [ "$MODE" = origin ]; then
    echo "=== Relocating Base"
    mv "$BASE" "$BASE-moved"
    for f in bin/$ARCH/caget bin/$ARCH/softIoc lib/$ARCH/libdbCore.so; do
        if ldd "$BASE-moved/$f" | grep -q "not found"; then
            ldd "$BASE-moved/$f"
            fail "relocated $(basename "$f") has unresolved libraries"
        else
            pass "relocated $(basename "$f") resolves all libraries"
        fi
    done
    mv "$BASE-moved" "$BASE"
fi

echo "==="
if [ $FAILED -ne 0 ]; then
    echo "$FAILED check(s) FAILED"
    exit 1
fi
echo "All checks passed"
