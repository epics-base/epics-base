#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
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
# The following go away what old database access goes away
INC += db_access_routines.h
INC += db_convert.h

DBDINC += menuAlarmSevr
DBDINC += menuAlarmStat
DBDINC += menuArrType
DBDINC += menuCompress
DBDINC += menuFtype
DBDINC += menuIvoa
DBDINC += menuOmsl
DBDINC += menuPriority
DBDINC += menuScan
DBDINC += menuYesNo
DBDINC += menuSimm

INC += dbCommon.h

DBD+=  dbCommon.dbd
DBD+=  menuGlobal.dbd

LIBSRCS += dbLock.c
LIBSRCS += dbAccess.c
LIBSRCS += dbBkpt.c
LIBSRCS += dbConvert.c
LIBSRCS += dbFastLinkConv.c
LIBSRCS += dbNotify.c
LIBSRCS += dbScan.c
LIBSRCS += dbEvent.c
LIBSRCS += dbTest.c
LIBSRCS += db_access.c
LIBSRCS += db_test.c
LIBSRCS += recGbl.c
LIBSRCS += callback.c
LIBSRCS += dbCa.c
LIBSRCS += dbCaTest.c
LIBSRCS += initHooks.c
LIBSRCS += cvtBpt.c
LIBSRCS += dbContext.cpp
LIBSRCS += dbChannelIO.cpp
LIBSRCS += dbSubscriptionIO.cpp
LIBSRCS += dbPutNotifyBlocker.cpp
LIBSRCS += dbContextReadNotifyCache.cpp
LIBSRCS += templateInstances.cpp

LIBRARY_IOC = dbIoc
dbIoc_LIBS = dbStaticIoc ca Com

dbIoc_RCS_WIN32 = dbIoc.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=dbIoc
OBJLIB_SRCS = $(LIBSRCS)
endif

include $(TOP)/configure/RULES

$(COMMON_DIR)/dbCommon.h: ../dbCommonRecord.dbd ../dbCommon.dbd
	@$(RM) $@$(DEP)
	@-$(MKMF) -m $@$(DEP) ..  $@ $<
	$(RM) $@
	$(DBTORECORDTYPEH) -I .. $< $@

inc: $(INSTALL_DBD)/dbCommon.dbd

