#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/osi

INC += compilerDependencies.h
INC += compilerSpecific.h

INC += osiFileName.h
INC += osiSock.h
INC += osdSock.h
INC += epicsInterrupt.h
INC += osdInterrupt.h

INC += epicsMutex.h
INC += osdMutex.h
INC += epicsSpin.h
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
INC += epicsAtomic.h
INC += epicsAtomicDefault.h
INC += epicsAtomicOSD.h
INC += epicsAtomicCD.h
INC += epicsEndian.h
INC += epicsReadline.h
INC += epicsMessageQueue.h
INC += epicsStdio.h
INC += epicsStdioRedirect.h
INC += epicsTempFile.h
INC += epicsGetopt.h
INC += epicsStackTrace.h

INC += devLib.h
INC += devLibVME.h
INC += devLibVMEImpl.h
INC += osdVME.h
INC += epicsMMIO.h
INC += epicsMMIODef.h

Com_SRCS += epicsThread.cpp
Com_SRCS += epicsMutex.cpp
Com_SRCS += epicsEvent.cpp
Com_SRCS += epicsTime.cpp
Com_SRCS += epicsMessageQueue.cpp
Com_SRCS += epicsMath.cpp
Com_SRCS += epicsAtomicOSD.cpp

Com_SRCS += epicsGeneralTime.c

# Time providers
Com_SRCS += osiClockTime.c
Com_SRCS_vxWorks += osiNTPTime.c
Com_SRCS_RTEMS += osiNTPTime.c

ifeq ($(OS_CLASS),vxWorks)
osdTime.o: USR_CXXFLAGS += -DBUILD_TIME=$(shell perl -e 'print time')
endif

Com_SRCS += osdSock.c
Com_SRCS += osdSockAddrReuse.cpp
Com_SRCS += osiSock.c
Com_SRCS += systemCallIntMech.cpp
Com_SRCS += epicsSocketConvertErrnoToString.cpp
Com_SRCS += osdAssert.c
Com_SRCS += osdFindSymbol.c
Com_SRCS += osdInterrupt.c
Com_SRCS += osdPoolStatus.c
Com_SRCS += osdSignal.cpp

Com_SRCS += osdEnv.c

# Command-line input support
epicsReadline_CFLAGS += -DEPICS_COMMANDLINE_LIBRARY=EPICS_COMMANDLINE_LIBRARY_$(COMMANDLINE_LIBRARY)
epicsReadline_INCLUDES += $(INCLUDES_$(COMMANDLINE_LIBRARY))

Com_SRCS += epicsReadline.c

Com_SRCS += epicsTempFile.cpp
Com_SRCS += epicsStdio.c
Com_SRCS += osdStdio.c

#POSIX thread priority scheduling flag
THREAD_CPPFLAGS_NO += -DDONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING
osdThread_CPPFLAGS += $(THREAD_CPPFLAGS_$(USE_POSIX_THREAD_PRIORITY_SCHEDULING))
osdSpin_CPPFLAGS += $(THREAD_CPPFLAGS_$(USE_POSIX_THREAD_PRIORITY_SCHEDULING))

Com_SRCS += osdThread.c
Com_SRCS += osdThreadExtra.c
Com_SRCS += osdThreadHooks.c
Com_SRCS += osdMutex.c
Com_SRCS += osdSpin.c
Com_SRCS += osdEvent.c
Com_SRCS += osdTime.cpp
Com_SRCS += osdProcess.c
Com_SRCS += osdNetIntf.c
Com_SRCS += osdMessageQueue.c

Com_SRCS += devLibVME.c
Com_SRCS += devLibVMEOSD.c

Com_SRCS_vxWorks += atReboot.cpp

# For old vxWorks applications
INC_vxWorks += camacLib.h
INC_vxWorks += epicsDynLink.h
INC_vxWorks += module_types.h
INC_vxWorks += task_params.h

Com_SRCS_vxWorks += epicsDynLink.c
Com_SRCS_vxWorks += veclist.c
Com_SRCS_vxWorks += logMsgToErrlog.cpp
Com_SRCS_vxWorks += strtoll.c
Com_SRCS_vxWorks += strtoull.c

#This forces the vxWorks compatibility stuff to be loaded
OBJS_vxWorks = vxComLibrary

INC_WIN32 += epicsAtomicMS.h

Com_SRCS_WIN32 += epicsGetopt.c 
Com_SRCS_WIN32 += setThreadName.cpp
#Com_SRCS_WIN32 += dllmain.cpp
Com_SRCS_WIN32 += forceBadAllocException.cpp

#Stack trace support
Com_SRCS += epicsStackTrace.c
Com_SRCS += osdBackTrace.cpp
Com_SRCS += osdFindAddr.c
