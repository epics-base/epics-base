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

# For R3.13 compatibility only
OBJLIB_vxWorks=dbStaticIoc
OBJLIB_SRCS = $(LIBSRCS) $(dbStaticIoc_SRCS)

dbStaticHost_DLL_LIBS = Com
dbStaticIoc_DLL_LIBS = Com

PROD_LIBS := dbStaticHost Com
dbStaticHost_DIR=.
PROD_HOST = dbReadTest dbExpand dbToMenuH dbToRecordtypeH
dbReadTest_SRCS = dbReadTest.c
dbExpand_SRCS = dbExpand.c
dbToMenuH_SRCS = dbToMenuH.c
dbToRecordtypeH_SRCS = dbToRecordtypeH.c

dbStaticHost_RCS_WIN32 = dbStaticHost.rc
dbStaticIoc_RCS_WIN32 = dbStaticIoc.rc

include $(TOP)/configure/RULES

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc.c: dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

inc depends : $(LIBTARGETS) $(INSTALL_PROD)

#	EOF Makefile.Host for base/src/dbStatic
