TOP = ../..
include $(TOP)/configure/CONFIG

LIBCOM = $(TOP)/src/libCom


# From cxxTemplates
SRC_DIRS += $(LIBCOM)/cxxTemplates

INC += resourceLib.h
INC += tsDLList.h
INC += tsSLList.h
INC += tsMinMax.h
INC += tsBTree.h

# From osi
SRC_DIRS += $(LIBCOM)/osi

INC += dbDefs.h
INC += epicsTypes.h
INC += epicsVersion.h
INC += gsd_sync_defs.h
INC += shareLib.h
#INC += createSoftLinks.sh
INC += unixFileName.h

INC += devSup.h
INC += drvSup.h
INC += caeventmask.h
INC += asLib.h
INC += registryDeviceSupport.h
INC += registryDriverSupport.h
INC += registryRecordType.h
INC += cadef.h
INC += caerr.h
INC += asDbLib.h

INC += osiFileName.h
INC += osiSock.h
INC += osdSock.h

INC += osiInterrupt.h
INC += osdInterrupt.h
INC += osiRing.h
INC += osdRing.h
INC += osiSem.h
INC += osdSem.h
INC += epicsAssert.h
INC += osiFindGlobalSymbol.h
INC += osdFindGlobalSymbol.h
INC += osiPoolStatus.h
INC += osdPoolStatus.h
INC += osiThread.h
INC += osdThread.h
INC += osiTime.h
INC += osdTime.h
INC += osiSigPipeIgnore.h
INC += osdSigPipeIgnore.h
INC += tsStamp.h
INC += osiMutex.h
INC += osiEvent.h

SRCS += osdSock.c
SRCS += osiSock.c
SRCS += osdAssert.c
SRCS += osdFindGlobalSymbol.c
SRCS += osdInterrupt.c
SRCS += osdPoolStatus.c
SRCS += osdRing.c
SRCS += osdSem.c
SRCS += osdThread.c
SRCS += osiThread.cpp
SRCS += osdTime.cpp
SRCS += osiTime.cpp
SRCS += osdSigPipeIgnore.c
#tsStamp code is part of osiTime and osdTime

SRC_DIRS += $(LIBCOM)/bucketLib
INC += bucketLib.h
SRCS += bucketLib.c

SRC_DIRS += $(LIBCOM)/calc
#following needed for locating postfixPvt.h and sCalcPostfixPvt.h
USR_CFLAGS += -I$(LIBCOM)/calc
INC += postfix.h
INC += sCalcPostfix.h
SRCS += postfix.c
SRCS += calcPerform.c
SRCS += sCalcPostfix.c
SRCS += sCalcPerform.c

SRC_DIRS += $(LIBCOM)/cvtFast
INC += cvtFast.h
SRCS += cvtFast.c

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
HTMLS += $(LIBCOM)/freeList/freeList.3.html

SRC_DIRS += $(LIBCOM)/gpHash
INC += gpHash.h
SRCS += gpHashLib.c
HTMLS += $(LIBCOM)/gpHash/gpHash.3.html

SRC_DIRS += $(LIBCOM)/macLib
INC += macLib.h
SRCS += macCore.c
SRCS += macUtil.c

SRC_DIRS += $(LIBCOM)/misc
INC += adjustment.h
INC += cantProceed.h
INC += epicsString.h
SRCS += adjustment.c
SRCS += cantProceed.c
SRCS += epicsString.c
SRCS += aToIPAddr.c
INC += truncateFile.h
SRCS += truncateFile.c
SRC_DIRS += $(LIBCOM)/logClient
INC += logClient.h
SRCS += logClient.c

SRC_DIRS += $(LIBCOM)/timer
INC += osiTimer.h
SRCS += osiTimer.cpp

#       For WIN32 we supply getopt as part of libCom:
INC_WIN32 := getopt.h
SRCS_WIN32 := getopt.c
SRCS_WIN32 += dllmain.cpp

# For RTEMS CommandInterpreter
INC_RTEMS := CommandInterpreter.h
SRCS_RTEMS := CommandInterpreter.c
HTMLS_RTEMS += $(LIBCOM)/osi/os/RTEMS/CommandInterpreter.3.html

#why is this here ?
USR_CFLAGS = -D_NO_PROTO 

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY=Com
SYS_DLL_LIBS := ws2_32


#   only for WIN32:
#   build LIBRARY as a DLL, use the specified def file
LIBTYPE := SHARED

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
ERR_S_FILES += $(LIBCOM)/misc/asLib.h
ERR_S_FILES += $(TOP)/src/db/dbAccess.h
ERR_S_FILES += $(LIBCOM)/misc/devSup.h
ERR_S_FILES += $(LIBCOM)/misc/drvSup.h
ERR_S_FILES += $(TOP)/src/db/recSup.h
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

epicsVersion.h: $(TOP)/configure/CONFIG_BASE_VERSION
	$(PERL) $(LIBCOM)/misc/makeEpicsVersion.pl $(TOP)/configure/CONFIG_BASE_VERSION

clean::
	@$(RM) errSymTbl.c envData.c

