#! /bin/sh
#   $Id$
# 	Author: Robert Zieman (ANL)
# 	Date:	6/03/91
# 
# 	Experimental Physics and Industrial Control System (EPICS)
# 
# 	Copyright 1991, the Regents of the University of California,
# 	and the University of Chicago Board of Governors.
# 
# 	This software was produced under  U.S. Government contracts:
# 	(W-7405-ENG-36) at the Los Alamos National Laboratory,
# 	and (W-31-109-ENG-38) at Argonne National Laboratory.
# 
# 	Initial development by:
# 		The Controls and Automation Group (AT-8)
# 		Ground Test Accelerator
# 		Accelerator Technology Division
# 		Los Alamos National Laboratory
# 
# 	Co-developed with
# 		The Controls and Computing Group
# 		Accelerator Systems Division
# 		Advanced Photon Source
# 		Argonne National Laboratory
# 
#  Modification Log:
#  -----------------
#  .01	mm-dd-yy	iii	Comment
#  .02	mm-dd-yy	iii	Comment
#  	...
#

# Tailored script to create local softlinks releative to this directory
# the name createSoftLinks.sh is a key and must not be changed. If the
# release tool sccsGet finds a file by the name of (createSoftLinks.sh),
# after it has retrieved any out-of-date SCCS files, it invokes the
# script to create the defined softlinks

/bin/rm -f blderrSymTbl
/bin/rm -f errInc.c
/bin/rm -f makeStatTbl

ln -s ../misc/blderrSymTbl blderrSymTbl
ln -s ../../epicsH/errInc.c errInc.c
ln -s ../misc/makeStatTbl makeStatTbl

