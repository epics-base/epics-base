#! /bin/sh
# chmod.sh
# share/src/sequencer / $Id$
# Tailored script to change permissions on files in this directory
# the name chmod.sh is a key and must not be changed. If the release tool
# getLatestDelta finds a file by the name of (chmod.sh), after it has retrieved
# any out-of-date SCCS files, it invokes the script to add execute permissions.

    /bin/chmod +x makeSeqVersion makeVersion
    /bin/chmod u+w Version
