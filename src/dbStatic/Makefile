
TOP=../..

include $(TOP)/configure/CONFIG

#       includes to install from this sub-project
#
INC += alarm.h alarmString.h dbBase.h dbFldTypes.h
INC += dbStaticLib.h link.h special.h guigroup.h

#       Sources to compile & put into lib:
#
Db_SRCS := dbStaticLib.c dbYacc.c dbPvdLib.c dbStaticNoRun.c 

#       Library to build:
#
LIBRARY_DEFAULT := Db

#       Products to build:
#
PROD_DEFAULT := dbReadTest dbExpand dbToMenuH dbToRecordtypeH
PROD_vxWorks := -nil-

#       libs needed by PROD:
#
PROD_LIBS := Db Com

dbStaticLib_SRCS = \
	dbYacc.c\
	dbPvdLib.c\
	dbStaticRun.c\
	dbStaticLib.c

LIBRARY_vxWorks = dbStaticLib

include $(TOP)/configure/RULES_BUILD

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc$(OBJ): dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

#	EOF Makefile.Host for base/src/dbStatic
