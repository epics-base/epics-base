TOP = ../..
include $(TOP)/configure/CONFIG

LIBCOM = $(TOP)/src/libCom

USR_CXXFLAGS_hpux += -D_REENTRANT

#epicsVersion is created by this Makefile
INC += epicsVersion.h

SRC_DIRS += $(LIBCOM)/bucketLib
INC += bucketLib.h
SRCS += bucketLib.c

SRC_DIRS += $(LIBCOM)/ring
#following needed for locating epicsRingPointer.h and epicsRingBytes.h
INC += epicsRingPointer.h
INC += epicsRingBytes.h
INC += epicsReadline.h
INC += osdReadline.h
SRCS += epicsRingPointer.cpp
SRCS += epicsRingBytes.c
SRCS += epicsReadline.c

SRC_DIRS += $(LIBCOM)/calc
#following needed for locating postfixPvt.h and sCalcPostfixPvt.h
INC += postfix.h
INC += sCalcPostfix.h
SRCS += postfix.c
SRCS += calcPerform.c
SRCS += sCalcPostfix.c
SRCS += sCalcPerform.c

SRC_DIRS += $(LIBCOM)/cvtFast
INC += cvtFast.h
SRCS += cvtFast.c

# From cppStd
SRC_DIRS += $(LIBCOM)/cppStd
INC += epicsExcept.h
INC += epicsList.h
INC += epicsListBase.h
SRCS += epicsListBase.cpp

# From cxxTemplates
SRC_DIRS += $(LIBCOM)/cxxTemplates
INC += resourceLib.h
INC += tsDLList.h
INC += tsDLListDeprecated.h
INC += tsSLList.h
INC += tsMinMax.h
INC += tsBTree.h
INC += tsFreeList.h

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

SRC_DIRS += $(LIBCOM)/logClient
INC += logClient.h
SRCS += logClient.c

SRC_DIRS += $(LIBCOM)/macLib
INC += macLib.h
SRCS += macCore.c
SRCS += macUtil.c

SRC_DIRS += $(LIBCOM)/misc
INC += adjustment.h
INC += cantProceed.h
INC += dbDefs.h
INC += epicsString.h
INC += epicsTypes.h
INC += gsd_sync_defs.h
INC += shareLib.h
INC += truncateFile.h
INC += unixFileName.h
INC += locationException.h
INC += ipAddrToAsciiAsynchronous.h
SRCS += aToIPAddr.c
SRCS += adjustment.c
SRCS += cantProceed.c
SRCS += epicsString.c
SRCS += truncateFile.c
SRCS += ipAddrToAsciiAsynchronous.cpp

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

INC += epicsAssert.h
INC += epicsFindSymbol.h
INC += osiPoolStatus.h
INC += osdPoolStatus.h
INC += osdThread.h

INC += epicsThread.h
INC += epicsTime.h
INC += osdTime.h
INC += osiSigPipeIgnore.h
INC += osdSigPipeIgnore.h
INC += osiProcess.h
INC += osiUnistd.h
INC += osiWireFormat.h

SRCS += epicsThread.cpp
SRCS += epicsTime.cpp

SRCS += osdSock.c
SRCS += osiSock.c
SRCS += osdAssert.c
SRCS += osdFindSymbol.c
SRCS += osdInterrupt.c
SRCS += osdPoolStatus.c

SRCS += osdThread.c
SRCS += osdMutex.c
SRCS += osdEvent.c
SRCS += osdTime.cpp
SRCS += osdSigPipeIgnore.c
SRCS += osdProcess.c
SRCS += osdNetIntf.c

SRC_DIRS += $(LIBCOM)/taskwd
INC += taskwd.h
SRCS += taskwd.c

SRC_DIRS += $(LIBCOM)/timer
INC += osiTimer.h
SRCS += osiTimer.cpp
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

#       For WIN32 we supply getopt as part of libCom:
INC_WIN32 := getopt.h
SRCS_WIN32 := getopt.c
SRCS_WIN32 += dllmain.cpp

# For RTEMS startup
SRCS_RTEMS += rtems_config.c
SRCS_RTEMS += rtems_netconfig.c
SRCS_RTEMS += rtems_init.c
SRCS_RTEMS += rtems_util.c

# For vxWorks a clock
INC_vxWorks += iocClock.h
SRCS_vxWorks += iocClock.c

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY=Com
SYS_DLL_LIBS := ws2_32 advapi32 user32

#Com_RCS_WIN32 = Com.rc

#   libs needed for PROD and TESTPRODUCT
PROD_LIBS := Com
SYS_PROD_LIBS_WIN32 := ws2_32 advapi32

#USR_CFLAGS += -DTS_TEST
#TESTPROD_SRCS=tsSubr.c
#TESTPROD=testtsLib
#TESTPROD=envtest
#TESTPROD=osiTimeTest fdManagerTest

#	for bldErrSymTbl:
#
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

#	The real dependecies seem to confuse GNUmake:
#	envData.c is rebuild every time...
envData.c: $(LIBCOM)/env/envDefs.h $(LIBCOM)/env/bldEnvData.pl
	$(PERL) $(LIBCOM)/env/bldEnvData.pl $(CONFIG)

errSymTbl.c: $(ERR_S_FILES) $(LIBCOM)/error/makeStatTbl.pl
	$(PERL) $(LIBCOM)/error/makeStatTbl.pl  $(ERR_S_FILES)

$(COMMON_DIR)/epicsVersion.h: $(TOP)/configure/CONFIG_BASE_VERSION
	$(PERL) $(LIBCOM)/misc/makeEpicsVersion.pl $(TOP)/configure/CONFIG_BASE_VERSION $(@D)

clean::
	@$(RM) errSymTbl.c envData.c

depends: $(INSTALL_LIBS)

