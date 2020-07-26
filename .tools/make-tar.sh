#!/bin/bash
#
# Make tar for git repo w/ one level of sub modules.
#
set -e

die() {
  echo "$1" >&2
  exit 1
}

maybedie() {
  if [ "$DEVMODE" ]; then
    echo "Warning: $1" >&2
  else
    echo "Error: $1" >&2
    exit 1
  fi
}

usage() {
   cat <<EOF >&2
usage: $0 [-v] [-s] <rev> [<outfile> [<prefix>]]

  <rev> may be any git revision spec. (tag, branch, or commit id).

  If provided, <outfile> must end with ".tar", ".tar.gz" or ".tar.bz2".
  If <outfile> is omitted, "base-<rev>.tar.gz" will be used.
  If provided, <prefix> must end with "/".  If <prefix> is omitted,
  the default is "base-<rev>/".

  Options:

    -v   Enable verbose prints
    -d   Enable permissive developer mode
EOF
   exit 1
}

export DEVMODE=

while getopts "vd" OPT
do
    case "$OPT" in
    v) set -x;;
    d) DEVMODE=1;;
    ?) echo "Unknown option"
       usage;;
    esac
done
shift $(($OPTIND - 1))

TOPREV="$1"
FINALTAR="$2"
PREFIX="${3:-}"

[ "$TOPREV" ] || usage

case "$FINALTAR" in
"")
  TAROPT=-z
  FINALTAR="base-$TOPREV.tar.gz"
  ;;
*.tar)
  TAROPT=""
  ;;
*.tar.gz)
  TAROPT=-z
  ;;
*.tar.bz2)
  TAROPT=-j
  ;;
*)
  die "outfile must end with '.tar.gz' or '.tar.bz2'"
  ;;
esac

case "$PREFIX" in
"")
  PREFIX="base-$TOPREV/"
  ;;
*/)
  ;;
*)
  die "Prefix must end with '/'"
  ;;
esac

# Check for both <tag> and R<tag>
if ! [ `git tag -l $TOPREV` ]
then
  if [ `git tag -l R$TOPREV` ]
  then
    TOPREV="R$TOPREV"
  else
    maybedie "No tags exist '$TOPREV' or 'R$TOPREV'"
  fi
fi

# temporary directory w/ automatic cleanup
TDIR=`mktemp -d`
trap 'rm -rf "$TDIR"' EXIT INT QUIT TERM

mkdir "$TDIR"/tar

echo "Exporting revision $TOPREV as $FINALTAR with prefix $PREFIX"

# Use git-archive to copy files at the given revision into our temp. dir.
# Respects 'export-exclude' in .gitattributes files.

git archive --prefix=$PREFIX $TOPREV | tar -C "$TDIR"/tar -x

# use ls-tree instead of submodule-foreach to capture submodule revision associated with supermodule rev.
#
# sub-modules appear in tree as eg.:
#  160000 commit c3a6cfcf0dad4a4eeecf59b474710d06ff3eb68a  modules/ca
git ls-tree -r $TOPREV | \
  awk '/^[0-9]+ commit / && $4 != ".ci" {print $3, $4}' | \
while read HASH MODDIR
do
    echo "Visiting $HASH $MODDIR"
    if [ -e $MODDIR/.git ]
    then
        git -C $MODDIR archive --prefix=${PREFIX}${MODDIR}/ $HASH | tar -C "$TDIR"/tar -x
    else
        maybedie "  Submodule not checked out."
    fi
done

# make a list of files copied and filter out undesirables

(cd "$TDIR"/tar && find . -mindepth 1 -not -type d) > "$TDIR"/list.1

# Remove leading ./ from filenames
sed -i -e 's|^\./||' "$TDIR"/list.1

# Exclude files
sed \
  -e '/\/\.ci\//d' \
  -e '/\/\.ci-local\//d' \
  -e '/\/\.tools\//d' \
  -e '/\/jenkins\//d' \
  -e '/\/\.git/d' \
  -e '/\/\.hgtags$/d' \
  -e '/\/\.cproject$/d' \
  -e '/\/\.project$/d' \
  -e '/\/\.travis\.yml$/d' \
  -e '/\/\.appveyor\.yml$/d' \
  -e '/\/\.readthedocs\.yml$/d' \
  "$TDIR"/list.1 > "$TDIR"/list.2

if ! diff -U 0 "$TDIR"/list.1 "$TDIR"/list.2
then
    echo "Excluding the files shown above"
fi

# Use the filtered list to build the final tar
tar -c $TAROPT -C "$TDIR"/tar -T "$TDIR"/list.2 -f "$FINALTAR"

echo "Wrote $FINALTAR"

tar -t $TAROPT -f "$FINALTAR" > "$TDIR"/list.3

# make sure we haven't picked up anything extra
if ! diff -u "$TDIR"/list.2 "$TDIR"/list.3
then
    die "Oops! Tarfile diff against plan shown above"
fi
