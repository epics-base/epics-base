
TOP=../..

include $(TOP)/configure/CONFIG

CMPLR = STRICT

USR_CFLAGS = -D_NO_PROTO 

#	includes to install from this sub-project
#
INC += cantProceed.h
INC += cvtFast.h
INC += ellLib.h
INC += envDefs.h
INC += epicsAssert.h
INC += errlog.h
INC += epicsPrint.h
INC += errMdef.h
INC += error.h
INC += fdmgr.h
INC += freeList.h
INC += gpHash.h
INC += memDebugLib.h
INC += tsDefs.h
INC += bucketLib.h
INC += pal.h
INC += fdManager.h
INC += osiTime.h
INC += osiTimer.h
INC += macLib.h
INC += sigPipeIgnore.h
INC += dbmf.h
INC += epicsString.h
INC += truncateFile.h
INC += adjustment.h
INC += postfix.h
INC += sCalcPostfix.h
INC += osiSleep.h
INC += bsdSocketResource.h

#	For WIN32 we supply getopt as part of libCom:
INC_WIN32 := getopt.h

####################################
#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#
LIBSRCS += cantProceed.c
LIBSRCS += bucketLib.c
LIBSRCS += cvtFast.c
LIBSRCS += ellLib.c
LIBSRCS += envSubr.c
LIBSRCS += envData.c
LIBSRCS += errlogUNIX.c
LIBSRCS += errSymLib.c
LIBSRCS += errSymTbl.c
LIBSRCS += fdmgr.c
LIBSRCS += freeListLib.c
LIBSRCS += gpHashLib.c
LIBSRCS += memDebugLib.c
LIBSRCS += postfix.c
LIBSRCS += calcPerform.c
LIBSRCS += sCalcPostfix.c
LIBSRCS += sCalcPerform.c
LIBSRCS += realpath.c
LIBSRCS += tsSubr.c
LIBSRCS += assertUNIX.c
LIBSRCS += macCore.c
LIBSRCS += macUtil.c
LIBSRCS += sigPipeIgnore.c
LIBSRCS += dbmf.c
LIBSRCS += epicsString.c
LIBSRCS += truncateFile.c
LIBSRCS += aToIPAddr.c
LIBSRCS += adjustment.c
LIBSRCS += bsdSockResource.c
LIBSRCS += osiSleep.c
LIBSRCS += bsdSockResCom.c

#
# if CPLUSPLUS isnt empty then include C++ src codes
#	Note: After including/excluding files here,
#	e.g. the C++ stuff,
#	please check Com.def!
#
LIBSRCS += $(patsubst %,osiTime.cc,$(strip $(CPLUSPLUS)))
LIBSRCS += $(patsubst %,osdTime.cc,$(strip $(CPLUSPLUS)))
LIBSRCS += $(patsubst %,osiTimer.cc,$(strip $(CPLUSPLUS)))
LIBSRCS += $(patsubst %,fdManager.cc,$(strip $(CPLUSPLUS)))

#	WIN32 has no getopt, we add it to the Com lib,
#	special initialisation is done in winmain.c
LIBSRCS_WIN32 := getopt.c getLastWSAErrorAsString.c
LIBSRCS_WIN32 += $(patsubst %,dllmain.cc,$(strip $(CPLUSPLUS))) 

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY:=Com

#	only for WIN32:
#	build LIBRARY as a DLL, use the specified def file
LIBTYPE := SHARED

#	libs needed for PROD and TESTPRODUCT
PROD_LIBS := Com 
SYS_PROD_LIBS_WIN32 := ws2_32 advapi32

#USR_CFLAGS += -DTS_TEST
#TESTPROD_SRCS=tsSubr.c
#TESTPROD=testtsLib
#TESTPROD=envtest
#TESTPROD=osiTimeTest fdManagerTest

MAN3 = gpHash.3 freeList.3

#	for bldErrSymTbl:
#
ERR_S_FILES += $(TOP)/src/as/asLib.h
ERR_S_FILES += $(TOP)/src/db/dbAccess.h
ERR_S_FILES += $(TOP)/src/db/devSup.h
ERR_S_FILES += $(TOP)/src/db/drvSup.h
ERR_S_FILES += $(TOP)/src/db/recSup.h
ERR_S_FILES += $(TOP)/src/dbStatic/dbStaticLib.h
ERR_S_FILES += ../tsDefs.h
ERR_S_FILES += ../errMdef.h
ERR_S_FILES += $(TOP)/src/cas/generic/casdef.h
ERR_S_FILES += $(TOP)/src/gdd/gddAppFuncTable.h

#================================================

USR_INCLUDES_68k = -I$(TOP)/src/include/os/vxWorks

VPATH = ../os/$(OS_CLASS)

libCom_SRCS += cantProceed.c
libCom_SRCS += cvtFast.c
libCom_SRCS += gpHashLib.c
libCom_SRCS += freeListLib.c
libCom_SRCS += ellLib.c
libCom_SRCS += envSubr.c
libCom_SRCS += envData.c
libCom_SRCS += errSymLib.c
libCom_SRCS += postfix.c
libCom_SRCS += calcPerform.c
libCom_SRCS += sCalcPostfix.c
libCom_SRCS += sCalcPerform.c
libCom_SRCS += bucketLib.c
libCom_SRCS += memDebugLib.c
libCom_SRCS += tsSubr.c
libCom_SRCS += pal.c
libCom_SRCS += paldef.c
libCom_SRCS += errSymTbl.c
libCom_SRCS += errlogVX.c
libCom_SRCS += assertVX.c
libCom_SRCS += macCore.c
libCom_SRCS += macUtil.c
#libCom_SRCS += osiTimer.c
#libCom_SRCS += osdTime.c
libCom_SRCS += sigPipeIgnore.c
libCom_SRCS += dbmf.c
libCom_SRCS += epicsString.c
libCom_SRCS += aToIPAddr.c
libCom_SRCS += adjustment.c
libCom_SRCS += bsdSockResCom.c
libCom_SRCS += bsdSockResource.c
libCom_SRCS += osiSleep.c

IOC_LIBRARY_vxWorks = libCom

#================================================

include $(TOP)/configure/RULES_BUILD

#	The real dependecies seem to confuse GNUmake:
#	envData.c is rebuild every time...
#envData.c: ../envDefs.h $(CONFIG)/CONFIG_ENV $(CONFIG)/CONFIG_SITE_ENV
envData.c: ../envDefs.h ../bldEnvData.pl
	$(PERL) ../bldEnvData.pl $(CONFIG)

errSymTbl.c: $(ERR_S_FILES)
	$(PERL) ../makeStatTbl.pl  $(ERR_S_FILES)

clean::
	@$(RM) errSymTbl.c envData.c

#	EOF Makefile.Host  for  base/src/libCom

