#! /bin/sh
# createSoftLinks.sh - create needed symbolic links
# share/src/libUnix $Id$

/bin/rm -f blderrSymTbl
/bin/rm -f errInc.c
/bin/rm -f makeStatTbl

ln -s ../misc/blderrSymTbl blderrSymTbl
ln -s ../misc/errInc.c errInc.c
ln -s ../../script/makeStatTbl makeStatTbl
