#!/bin/sh
# epics/release $Id$
#       Author: Roger A. Cole (LANL)
#       Date:   08-20-91
#
#       Experimental Physics and Industrial Control System (EPICS)
#
#       Copyright 1991, the Regents of the University of California,
#       and the University of Chicago Board of Governors.
#
#       This software was produced under  U.S. Government contracts:
#       (W-7405-ENG-36) at the Los Alamos National Laboratory,
#       and (W-31-109-ENG-38) at Argonne National Laboratory.
#
#       Initial development by:
#               The Controls and Automation Group (AT-8)
#               Ground Test Accelerator
#               Accelerator Technology Division
#               Los Alamos National Laboratory
#
#       Co-developed with
#               The Controls and Computing Group
#               Accelerator Systems Division
#               Advanced Photon Source
#               Argonne National Laboratory
#
#  Modification Log:
#  -----------------
#  .01  08-20-91        rac	initial version

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
