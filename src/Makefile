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
#  .02  08-16-93	rcz	moving most of these pre-builds back to the
# object area Imakefile and adding DependTarget($(EARLYSRC))
#
#  	...
#

all: bld_errSym dlsource eddsource 

bld_errSym:
	cd libUnix; 	sh blderrSymTbl
	cd libvxWorks; 	sh blderrSymTbl VXLIST

dlsource:
	cd opi/dl ; /bin/sh build.it

eddsource:
	cd opi/edd ; /bin/sh build.it


