TOP = ../..
include $(TOP)/configure/CONFIG

LIBCOM = $(TOP)/src/libComNew


# From osi
SRC_DIRS = $(LIBCOM)/osi


INC += osiInterrupt.h
INC += osiRing.h
INC += osiSem.h

INC += bsdSocketResource.h
INC += epicsAssert.h
INC += osiFindGlobalSymbol.h
INC += osiPoolStatus.h
INC += osiThread.h
INC += osiTime.h
INC += sigPipeIgnore.h
INC += tsStamp.h

SRCS += bsdSockResource.c
SRCS += bsdSockResCom.c
SRCS += assert.c
SRCS += osiFindGlobalSymbol.c
SRCS += osiInterrupt.c
SRCS += osiPoolStatus.c
SRCS += osiRing.c
SRCS += osiSem.c
SRCS += osiThread.c
SRCS += osdTime.cpp
SRCS += osiTime.cpp
SRCS += sigPipeIgnore.c
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
#SRCS += fdmgr.cpp
#SRCS += fdManager.cpp

SRC_DIRS += $(LIBCOM)/freeList
INC += freeList.h
SRCS += freeListLib.c

SRC_DIRS += $(LIBCOM)/gpHash
INC += gpHash.h
SRCS += gpHashLib.c

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

#why is this here ?
USR_CFLAGS = -D_NO_PROTO 

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIB=Com


#	for bldErrSymTbl:
#
ERR_S_FILES += $(TOP)/src/include/asLib.h
ERR_S_FILES += $(TOP)/src/db/dbAccess.h
ERR_S_FILES += $(TOP)/src/include/devSup.h
ERR_S_FILES += $(TOP)/src/include/drvSup.h
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

clean::
	@$(RM) errSymTbl.c envData.c

