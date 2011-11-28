#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP = ../..
include $(TOP)/configure/CONFIG

LIBCOM = $(TOP)/src/libCom

# Command-line input support
epicsReadline_CFLAGS += -DEPICS_COMMANDLINE_LIBRARY=EPICS_COMMANDLINE_LIBRARY_$(COMMANDLINE_LIBRARY)
epicsReadline_INCLUDES += $(INCLUDES_$(COMMANDLINE_LIBRARY))

#POSIX thread priority scheduling flag
THREAD_CPPFLAGS_NO += -DDONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING
osdThread_CPPFLAGS += $(THREAD_CPPFLAGS_$(USE_POSIX_THREAD_PRIORITY_SCHEDULING))

#epicsVersion is created by this Makefile
INC += epicsVersion.h

SRC_DIRS += $(LIBCOM)/bucketLib
INC += bucketLib.h
SRCS += bucketLib.c

SRC_DIRS += $(LIBCOM)/ring
#following needed for locating epicsRingPointer.h and epicsRingBytes.h
INC += epicsRingPointer.h
INC += epicsRingBytes.h
SRCS += epicsRingPointer.cpp
SRCS += epicsRingBytes.c

SRC_DIRS += $(LIBCOM)/calc
INC += postfix.h
SRCS += postfix.c
SRCS += calcPerform.c

SRC_DIRS += $(LIBCOM)/cvtFast
INC += cvtFast.h
SRCS += cvtFast.c

# From cppStd
SRC_DIRS += $(LIBCOM)/cppStd
INC += epicsAlgorithm.h
INC += epicsExcept.h
INC += epicsMemory.h

# From cxxTemplates
SRC_DIRS += $(LIBCOM)/cxxTemplates
INC += resourceLib.h
INC += tsDLList.h
INC += tsSLList.h
INC += tsMinMax.h
INC += tsBTree.h
INC += tsFreeList.h
INC += epicsSingleton.h
INC += epicsGuard.h
INC += epicsOnce.h
SRCS += resourceLib.cpp
SRCS += epicsOnce.cpp
SRCS += epicsSingletonMutex.cpp

SRC_DIRS += $(LIBCOM)/dbmf
INC += dbmf.h
SRCS += dbmf.c

SRC_DIRS += $(LIBCOM)/ellLib
INC += ellLib.h
SRCS += ellLib.c

SRC_DIRS += $(LIBCOM)/env
INC += envDefs.h
SRCS += envSubr.c
SRCS += envData.c

SRC_DIRS += $(LIBCOM)/error
INC += epicsPrint.h
INC += errMdef.h
INC += errSymTbl.h
INC += errlog.h
INC += error.h
SRCS += errlog.c
SRCS += errSymLib.c
SRCS += errSymTbl.c

SRC_DIRS += $(LIBCOM)/fdmgr
INC += fdManager.h
INC += fdmgr.h
SRCS += fdmgr.cpp
SRCS += fdManager.cpp

SRC_DIRS += $(LIBCOM)/freeList
INC += freeList.h
SRCS += freeListLib.c
HTMLS += freeList/freeList.html

SRC_DIRS += $(LIBCOM)/gpHash
INC += gpHash.h
SRCS += gpHashLib.c
HTMLS += gpHash/gpHash.html

SRC_DIRS += $(LIBCOM)/iocsh
INC += iocsh.h
INC += registry.h
INC += libComRegister.h
SRCS += iocsh.cpp
SRCS += registry.c
SRCS += libComRegister.c

SRC_DIRS += $(LIBCOM)/logClient
INC += iocLog.h
INC += logClient.h
SRCS += iocLog.c
SRCS += logClient.c

SRC_DIRS += $(LIBCOM)/macLib
INC += macLib.h
SRCS += macCore.c
SRCS += macEnv.c
SRCS += macUtil.c

SRC_DIRS += $(LIBCOM)/misc
INC += adjustment.h
INC += cantProceed.h
INC += dbDefs.h
INC += epicsConvert.h
INC += epicsExit.h
INC += epicsStdlib.h
INC += epicsString.h
INC += epicsTypes.h
INC += shareLib.h
INC += epicsExport.h
INC += unixFileName.h
INC += locationException.h
INC += ipAddrToAsciiAsynchronous.h
INC += compilerDependencies.h
INC += epicsUnitTest.h
INC += testMain.h
SRCS += aToIPAddr.c
SRCS += adjustment.c
SRCS += cantProceed.c
SRCS += epicsConvert.c
SRCS += epicsExit.c
SRCS += epicsStdlib.c
SRCS += epicsString.c
SRCS += truncateFile.c
SRCS += ipAddrToAsciiAsynchronous.cpp
SRCS += epicsUnitTest.c

# From osi
SRC_DIRS += $(LIBCOM)/osi
INC += osiFileName.h
INC += osiSock.h
INC += osdSock.h
INC += epicsInterrupt.h
INC += osdInterrupt.h

INC += epicsMutex.h
INC += osdMutex.h
INC += epicsEvent.h
INC += osdEvent.h
INC += epicsMath.h
INC += osdMessageQueue.h
INC += osdStrtod.h

INC += epicsAssert.h
INC += epicsFindSymbol.h
INC += osiPoolStatus.h
INC += osdPoolStatus.h
INC += osdThread.h

INC += epicsThread.h
INC += epicsTime.h
INC += epicsGeneralTime.h
INC += osdTime.h
INC += generalTimeSup.h
INC += osiClockTime.h
INC += epicsSignal.h
INC += osiProcess.h
INC += osiUnistd.h
INC += osiWireFormat.h
INC += osdWireFormat.h
INC += osdWireConfig.h
INC += epicsEndian.h
INC += epicsReadline.h
INC += epicsMessageQueue.h
INC += epicsStdio.h
INC += epicsStdioRedirect.h
INC += epicsGetopt.h

INC += devLib.h
INC += devLibVME.h
INC += devLibVMEImpl.h
INC += osdVME.h

SRCS += epicsThread.cpp
SRCS += epicsMutex.cpp
SRCS += epicsEvent.cpp
SRCS += epicsTime.cpp
SRCS += epicsMessageQueue.cpp
SRCS += epicsMath.cpp

SRCS += epicsGeneralTime.c
SRCS += osiClockTime.c

SRCS += osdSock.c
SRCS += osdSockAddrReuse.cpp
SRCS += osiSock.c
SRCS += systemCallIntMech.cpp
SRCS += epicsSocketConvertErrnoToString.cpp
SRCS += osdAssert.c
SRCS += osdFindSymbol.c
SRCS += osdInterrupt.c
SRCS += osdPoolStatus.c
SRCS += osdSignal.cpp
SRCS += osdEnv.c
SRCS += epicsReadline.c
SRCS += epicsTempFile.cpp
SRCS += epicsStdio.c
SRCS += osdStdio.c

SRCS += osdThread.c
SRCS += osdMutex.c
SRCS += osdEvent.c
SRCS += osdTime.cpp
SRCS += osdProcess.c
SRCS += osdNetIntf.c
SRCS += osdMessageQueue.c

SRCS += devLibVME.c
SRCS += devLibVMEOSD.c

SRC_DIRS += $(LIBCOM)/taskwd
INC += taskwd.h
SRCS += taskwd.c

SRC_DIRS += $(LIBCOM)/timer
INC += epicsTimer.h
SRCS += epicsTimer.cpp
SRCS += timer.cpp
SRCS += timerQueue.cpp
SRCS += timerQueueActive.cpp
SRCS += timerQueueActiveMgr.cpp
SRCS += timerQueuePassive.cpp

#tsDefs contains R3.13 compatibility tsStamp code
SRC_DIRS += $(LIBCOM)/tsDefs
INC += tsDefs.h
SRCS += tsDefs.c

# Time providers, in osi
SRCS_vxWorks += osiNTPTime.c
SRCS_RTEMS += osiNTPTime.c

# These files are in osi/os/vxWorks
# Special reboot hook
SRCS_vxWorks += atReboot.cpp
# For old vxWorks applications
INC_vxWorks += camacLib.h
INC_vxWorks += epicsDynLink.h
INC_vxWorks += module_types.h
INC_vxWorks += task_params.h
SRCS_vxWorks += epicsDynLink.c
SRCS_vxWorks += veclist.c
SRCS_vxWorks += logMsgToErrlog.cpp

#This forces the vxWorks compatibility stuff to be loaded
OBJS_vxWorks = vxComLibrary

# These files are in osi/os/WIN32
SRCS_WIN32 += epicsGetopt.c 
SRCS_WIN32 += setThreadName.cpp
#SRCS_WIN32 += dllmain.cpp
SRCS_WIN32 += forceBadAllocException.cpp

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY=Com

Com_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

Com_RCS = Com.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=Com
OBJLIB_SRCS = $(SRCS)
endif

#   libs needed for PROD and TESTPRODUCT
PROD_LIBS = Com
PROD_SYS_LIBS_WIN32 = ws2_32 advapi32

#	for bldErrSymTbl:
#
ERR_S_FILES += $(TOP)/src/libCom/osi/devLib.h
ERR_S_FILES += $(TOP)/src/as/asLib.h
ERR_S_FILES += $(TOP)/src/db/dbAccessDefs.h
ERR_S_FILES += $(TOP)/src/dbStatic/devSup.h
ERR_S_FILES += $(TOP)/src/dbStatic/drvSup.h
ERR_S_FILES += $(TOP)/src/dbStatic/recSup.h
ERR_S_FILES += $(TOP)/src/dbStatic/dbStaticLib.h
ERR_S_FILES += $(LIBCOM)/error/errMdef.h
ERR_S_FILES += $(TOP)/src/cas/generic/casdef.h
ERR_S_FILES += $(TOP)/src/gdd/gddAppFuncTable.h

include $(TOP)/configure/RULES

osdAssert$(OBJ): $(COMMON_DIR)/epicsVersion.h
epicsTime$(OBJ): $(COMMON_DIR)/epicsVersion.h
osdNetIntf$(OBJ): $(COMMON_DIR)/epicsVersion.h
osdSock$(OBJ): $(COMMON_DIR)/epicsVersion.h

envData.c: $(LIBCOM)/env/envDefs.h $(LIBCOM)/env/bldEnvData.pl \
	$(CONFIG)/CONFIG_ENV $(CONFIG)/CONFIG_SITE_ENV
	$(PERL) $(LIBCOM)/env/bldEnvData.pl $(CONFIG)

errSymTbl.c: $(ERR_S_FILES) $(LIBCOM)/error/makeStatTbl.pl
	$(PERL) $(LIBCOM)/error/makeStatTbl.pl  $(ERR_S_FILES)

$(COMMON_DIR)/epicsVersion.h: $(CONFIG)/CONFIG_BASE_VERSION $(CONFIG)/CONFIG_SITE
	$(PERL) $(LIBCOM)/misc/makeEpicsVersion.pl $(CONFIG)/CONFIG_BASE_VERSION $(@D) $(EPICS_SITE_VERSION)

clean::
	@$(RM) errSymTbl.c envData.c

