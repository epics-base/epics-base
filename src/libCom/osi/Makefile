#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

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

# Time providers
SRCS += osiClockTime.c
SRCS_vxWorks += osiNTPTime.c
SRCS_RTEMS += osiNTPTime.c

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

osdEnv_CFLAGS_WIN32= -U__STDC__

SRCS += osdEnv.c

# Command-line input support
epicsReadline_CFLAGS += -DEPICS_COMMANDLINE_LIBRARY=EPICS_COMMANDLINE_LIBRARY_$(COMMANDLINE_LIBRARY)
epicsReadline_INCLUDES += $(INCLUDES_$(COMMANDLINE_LIBRARY))

SRCS += epicsReadline.c

SRCS += epicsTempFile.cpp
SRCS += epicsStdio.c
SRCS += osdStdio.c

#POSIX thread priority scheduling flag
THREAD_CPPFLAGS_NO += -DDONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING
osdThread_CPPFLAGS += $(THREAD_CPPFLAGS_$(USE_POSIX_THREAD_PRIORITY_SCHEDULING))

SRCS += osdThread.c
SRCS += osdMutex.c
SRCS += osdEvent.c
SRCS += osdTime.cpp
SRCS += osdProcess.c
SRCS += osdNetIntf.c
SRCS += osdMessageQueue.c

SRCS += devLibVME.c
SRCS += devLibVMEOSD.c

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

SRCS_WIN32 += epicsGetopt.c 
SRCS_WIN32 += setThreadName.cpp
#SRCS_WIN32 += dllmain.cpp
SRCS_WIN32 += forceBadAllocException.cpp
