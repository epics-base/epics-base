#!/bin/sh
# epics/release chmod.sh,v 1.1.1.1 1995/08/15 03:15:26 epicss Exp
#       Author: Roger A. Cole (LANL)
#       Date:   08-20-91
#

# set execute permission for shell scripts in this directory (this avoids
# the need to manually maintain a list of file names)

# the name of this file is magic for ~epics/release/sccsGet.  Whenever that
# that script sees a file named "chmod.sh" it executes the file.

L=""
for F in `find * \( -type d -prune \) -o -type f -print`; do
    if [ "`sed -n -e '/^\#\!/p' -e 1q $F`" ]; then
	L="$L $F"
    fi
done
if [ "$L" ]; then
    chmod +x $L
fi
