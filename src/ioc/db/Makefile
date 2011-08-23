#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/db

INC += callback.h
INC += dbAccess.h
INC += dbAccessDefs.h
INC += dbCa.h
INC += dbAddr.h
INC += dbBkpt.h
INC += dbConvert.h
INC += dbConvertFast.h
INC += dbEvent.h
INC += dbLock.h
INC += dbNotify.h
INC += dbScan.h
INC += dbTest.h
INC += dbCaTest.h
INC += db_test.h
INC += db_field_log.h
INC += initHooks.h
INC += recGbl.h
INC += dbIocRegister.h
# The following go away what old database access goes away
INC += db_access_routines.h
INC += db_convert.h

DBDINC += menuAlarmSevr
DBDINC += menuAlarmStat
DBDINC += menuCompress
DBDINC += menuFtype
DBDINC += menuIvoa
DBDINC += menuOmsl
DBDINC += menuPini
DBDINC += menuPriority
DBDINC += menuScan
DBDINC += menuYesNo
DBDINC += menuSimm

DBDINC += dbCommon
DBD+=  menuGlobal.dbd

dbCore_SRCS += dbLock.c
dbCore_SRCS += dbAccess.c
dbCore_SRCS += dbBkpt.c
dbCore_SRCS += dbConvert.c
dbCore_SRCS += dbFastLinkConv.c
dbCore_SRCS += dbNotify.c
dbCore_SRCS += dbScan.c
dbCore_SRCS += dbEvent.c
dbCore_SRCS += dbTest.c
dbCore_SRCS += db_access.c
dbCore_SRCS += db_test.c
dbCore_SRCS += recGbl.c
dbCore_SRCS += callback.c
dbCore_SRCS += dbCa.c
dbCore_SRCS += dbCaTest.c
dbCore_SRCS += initHooks.c
dbCore_SRCS += cvtBpt.c
dbCore_SRCS += dbContext.cpp
dbCore_SRCS += dbChannelIO.cpp
dbCore_SRCS += dbSubscriptionIO.cpp
dbCore_SRCS += dbPutNotifyBlocker.cpp
dbCore_SRCS += dbContextReadNotifyCache.cpp
dbCore_SRCS += templateInstances.cpp
dbCore_SRCS += dbIocRegister.c

