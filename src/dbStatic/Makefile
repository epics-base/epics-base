TOP=../..

include $(TOP)/configure/CONFIG

INC += alarm.h
INC += alarmString.h
INC += dbBase.h
INC += dbFldTypes.h
INC += dbStaticLib.h
INC += link.h
INC += special.h
INC += guigroup.h
INC += devSup.h
INC += drvSup.h
INC += recSup.h

LIBSRCS += dbStaticLib.c
LIBSRCS += dbYacc.c
LIBSRCS += dbPvdLib.c
dbStaticHost_SRCS += dbStaticNoRun.c 
dbStaticIoc_SRCS += dbStaticRun.c

LIBRARY_HOST += dbStaticHost
LIBRARY_IOC += dbStaticIoc

dbStaticHost_DLL_LIBS = Com
dbStaticIoc_DLL_LIBS = Com

PROD_LIBS := dbStaticHost Com
dbStaticHost_DIR=.
PROD = dbReadTest dbExpand dbToMenuH dbToRecordtypeH

include $(TOP)/configure/RULES

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc$(OBJ): dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

#	EOF Makefile.Host for base/src/dbStatic
