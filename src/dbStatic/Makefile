#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

INC += alarm.h
INC += alarmString.h
INC += dbBase.h
INC += dbFldTypes.h
INC += dbStaticLib.h
INC += dbStaticPvt.h
INC += link.h
INC += special.h
INC += guigroup.h
INC += devSup.h
INC += drvSup.h
INC += recSup.h
INC += dbStaticIocRegister.h

LIBSRCS += dbStaticLib.c
LIBSRCS += dbYacc.c
LIBSRCS += dbPvdLib.c

dbStaticHost_SRCS += dbStaticNoRun.c 
dbStaticIoc_SRCS += dbStaticRun.c
dbStaticIoc_SRCS += dbStaticIocRegister.c

LIBRARY_HOST += dbStaticHost
LIBRARY_IOC += dbStaticIoc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=dbStaticIoc
OBJLIB_SRCS = $(LIBSRCS) $(dbStaticIoc_SRCS)
endif

dbStaticHost_LIBS = Com
dbStaticIoc_LIBS = Com

dbStaticHost_RCS = dbStaticHost.rc
dbStaticIoc_RCS = dbStaticIoc.rc

PROD_LIBS := dbStaticHost Com
PROD_HOST = dbReadTest dbExpand dbToMenuH dbToRecordtypeH

dbReadTest_SRCS = dbReadTest.c
dbExpand_SRCS = dbExpand.c
dbToMenuH_SRCS = dbToMenuH.c
dbToRecordtypeH_SRCS = dbToRecordtypeH.c

include $(TOP)/configure/RULES

# Extra rule since dbLexRoutines.c is included in dbYacc.c
dbYacc.c: dbLex.c ../dbLexRoutines.c

clean::
	@$(RM) dbLex.c dbYacc.c 

#	EOF Makefile.Host for base/src/dbStatic
