#!/bin/sh
set -e -x
# Usage: commit-gh <sub-directory-prefix> <files...>
#
# Creates a commit containing only the files in the sub-directory provided as an argument
#
# Does not disturb the working copy or index

prefix="$1"
shift

# Commit to this branch
BRANCH=refs/heads/gh-pages

# Use the main branch description as the gh-pages commit message
MSG=`git describe --tags --always`

# Scratch space
TDIR=`mktemp -d -p $PWD`

# Automatic cleanup of scratch space
trap 'rm -rf $TDIR' INT TERM QUIT EXIT

export GIT_INDEX_FILE="$TDIR/index"

# Add listed files to a new (empty) index
git update-index --add "$@"

# Write the index into the repo, get tree hash
TREE=`git write-tree --prefix="$prefix"`

echo "TREE $TREE"
git cat-file -p $TREE

# Create a commit with our new tree
# Reference current branch head as parent (if any)
CMT=`git commit-tree -m "$MSG" $TREE`

echo "COMMIT $CMT"
git cat-file -p $CMT

# Update the branch with the new commit tree hash
git update-ref $BRANCH $CMT

echo "Done"
