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

#       includes to install from this sub-project
#
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

LIB_SRCS += dbLock.c
LIB_SRCS += dbAccess.c
LIB_SRCS += dbBkpt.c
LIB_SRCS += dbConvert.c
LIB_SRCS += dbFastLinkConv.c
LIB_SRCS += dbNotify.c
LIB_SRCS += dbScan.c
LIB_SRCS += dbEvent.c
LIB_SRCS += dbTest.c
LIB_SRCS += db_access.c
LIB_SRCS += db_test.c
LIB_SRCS += recGbl.c
LIB_SRCS += callback.c
LIB_SRCS += dbCa.c
LIB_SRCS += dbCaTest.c
LIB_SRCS += initHooks.c
LIB_SRCS += cvtBpt.c
LIB_SRCS += dbContext.cpp
LIB_SRCS += dbChannelIO.cpp
LIB_SRCS += dbSubscriptionIO.cpp
LIB_SRCS += dbPutNotifyBlocker.cpp
LIB_SRCS += dbContextReadNotifyCache.cpp
LIB_SRCS += templateInstances.cpp
LIB_SRCS += dbIocRegister.c

LIBRARY_IOC = dbIoc
dbIoc_LIBS = dbStaticIoc ca Com

dbIoc_RCS = dbIoc.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=dbIoc
OBJLIB_SRCS = $(LIB_SRCS)
endif

include $(TOP)/configure/RULES

dbCommon.h$(DEP): ../dbCommonRecord.dbd ../dbCommon.dbd
	@$(RM) $@
	@-$(MKMF) -m $@ ..  $(COMMON_DIR)/dbCommon.h $<

$(COMMON_DIR)/dbCommon.h: ../dbCommonRecord.dbd ../dbCommon.dbd
	$(RM) $@
	$(DBTORECORDTYPEH) -I .. $< $@

