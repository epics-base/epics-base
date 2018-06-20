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
INC += dbAddr.h
INC += dbBkpt.h
INC += dbCa.h
INC += dbChannel.h
INC += dbConstLink.h
INC += dbConvert.h
INC += dbConvertFast.h
INC += dbConvertJSON.h
INC += dbDbLink.h
INC += dbExtractArray.h
INC += dbEvent.h
INC += dbJLink.h
INC += dbLink.h
INC += dbLock.h
INC += dbNotify.h
INC += dbScan.h
INC += dbServer.h
INC += dbTest.h
INC += dbCaTest.h
INC += db_test.h
INC += db_field_log.h
INC += initHooks.h
INC += recGbl.h
INC += dbIocRegister.h
INC += chfPlugin.h
INC += dbState.h
INC += db_access_routines.h
INC += db_convert.h
INC += dbUnitTest.h

# Generate menuGlobal.dbd, not really by concatenation, see RULES
DBDCAT += menuGlobal.dbd
menuGlobal_DBD += menuAlarmSevr.dbd
menuGlobal_DBD += menuAlarmStat.dbd
menuGlobal_DBD += menuFtype.dbd
menuGlobal_DBD += menuIvoa.dbd
menuGlobal_DBD += menuOmsl.dbd
menuGlobal_DBD += menuPini.dbd
menuGlobal_DBD += menuPost.dbd
menuGlobal_DBD += menuPriority.dbd
menuGlobal_DBD += menuYesNo.dbd
menuGlobal_DBD += menuSimm.dbd

DBDINC += $(basename $(menuGlobal_DBD))
DBDINC += menuScan
DBDINC += dbCommon

dbMenusPod = $(notdir $(wildcard ../db/menu*.dbd.pod))
HTMLS += $(patsubst %.dbd.pod,%.html,$(menusPod))

dbCore_SRCS += dbLock.c
dbCore_SRCS += dbAccess.c
dbCore_SRCS += dbBkpt.c
dbCore_SRCS += dbChannel.c
dbCore_SRCS += dbConstLink.c
dbCore_SRCS += dbConvert.c
dbCore_SRCS += dbConvertJSON.c
dbCore_SRCS += dbDbLink.c
dbCore_SRCS += dbFastLinkConv.c
dbCore_SRCS += dbExtractArray.c
dbCore_SRCS += dbJLink.c
dbCore_SRCS += dbLink.c
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
dbCore_SRCS += dbIocRegister.c
dbCore_SRCS += chfPlugin.c
dbCore_SRCS += dbState.c
dbCore_SRCS += dbUnitTest.c
dbCore_SRCS += dbServer.c

