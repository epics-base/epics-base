
TOP=../..

include $(TOP)/configure/CONFIG

#       includes to install from this sub-project
#
INC += alarm.h alarmString.h dbBase.h dbFldTypes.h
INC += dbStaticLib.h link.h special.h guigroup.h

#       Sources to compile & put into lib:
#
LIBSRCS := dbStaticLib.c dbYacc.c dbPvdLib.c dbStaticNoRun.c 

#       Library to build:
#
LIBRARY := Db

#       Products to build:
#
PROD := dbReadTest dbExpand dbToMenuH dbToRecordtypeH

#       libs needed by PROD:
#
PROD_LIBS := Db Com Osi 

#====================================================

dbStaticLib_SRCS = \
	dbYacc.c\
	dbPvdLib.c\
	dbStaticRun.c\
	dbStaticLib.c

IOC_LIBRARY_vxWorks = dbStaticLib

#====================================================
include $(TOP)/configure/RULES_BUILD

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc$(OBJ): dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

#	EOF Makefile.Host for base/src/dbStatic
