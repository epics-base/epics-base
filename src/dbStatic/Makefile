TOP=../..

include $(TOP)/configure/CONFIG

#       includes to install from this sub-project
#
INC += alarm.h
INC += alarmString.h
INC += dbBase.h
INC += dbFldTypes.h
INC += dbStaticLib.h
INC += link.h
INC += special.h
INC += guigroup.h

#       Sources to compile & put into lib:
#
LIBSRCS += dbStaticLib.c
LIBSRCS += dbYacc.c
LIBSRCS += dbPvdLib.c
dbStaticHost_SRCS += dbStaticNoRun.c 
dbStaticIoc_SRCS += dbStaticRun.c


#       Library to build:
#
LIBRARY_HOST += dbStaticHost
LIBRARY_IOC += dbStaticIoc

DLL_LIBS = Com

#       Products to build:
#
PROD = dbReadTest dbExpand dbToMenuH dbToRecordtypeH

#       libs needed by PROD:
#
PROD_LIBS := dbStaticHost Com
Db_DIR=.

include $(TOP)/configure/RULES

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc$(OBJ): dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

#	EOF Makefile.Host for base/src/dbStatic
