#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP := ../../..
CAS := $(TOP)/src/cas
SRC := $(CAS)/generic
CA := $(CAS)/../ca
IOSRC := $(CAS)/io/bsdSocket
STSRC := $(SRC)/st

include $(TOP)/configure/CONFIG

SRC_DIRS += $(SRC)
SRC_DIRS += $(IOSRC)
SRC_DIRS += $(STSRC)
SRC_DIRS += $(CA)

#USR_CXXFLAGS = $(USR_CFLAGS)

INC += casdef.h
INC += casEventMask.h
INC += caNetAddr.h

LIBSRCS += caServer.cc
LIBSRCS += caServerI.cc
LIBSRCS += casCoreClient.cc
LIBSRCS += casDGClient.cc
LIBSRCS += casStrmClient.cc
LIBSRCS += casPV.cc
LIBSRCS += casPVI.cc
LIBSRCS += casChannel.cc
LIBSRCS += casChannelI.cc
LIBSRCS += casAsyncIOI.cc
LIBSRCS += casAsyncReadIO.cc
LIBSRCS += casAsyncReadIOI.cc
LIBSRCS += casAsyncWriteIO.cc
LIBSRCS += casAsyncWriteIOI.cc
LIBSRCS += casAsyncPVExistIO.cc
LIBSRCS += casAsyncPVExistIOI.cc
LIBSRCS += casAsyncPVAttachIO.cc
LIBSRCS += casAsyncPVAttachIOI.cc
LIBSRCS += casEventSys.cc
LIBSRCS += casMonitor.cc
LIBSRCS += casMonEvent.cc
LIBSRCS += inBuf.cc
LIBSRCS += outBuf.cc
LIBSRCS += casCtx.cc 
LIBSRCS += casEventMask.cc
LIBSRCS += ioBlocked.cc
LIBSRCS += casBufferFactory.cc
LIBSRCS += pvExistReturn.cc
LIBSRCS += pvAttachReturn.cc
LIBSRCS += caNetAddr.cc
LIBSRCS += beaconTimer.cc
LIBSRCS += beaconAnomalyGovernor.cc
LIBSRCS += clientBufMemoryManager.cpp
LIBSRCS += chanIntfForPV.cc
LIBSRCS += channelDestroyEvent.cpp

LIBSRCS += casIntfOS.cc
LIBSRCS += casDGIntfOS.cc
LIBSRCS += casStreamOS.cc

LIBSRCS += caServerIO.cc
LIBSRCS += casIntfIO.cc
LIBSRCS += casDGIntfIO.cc
LIBSRCS += casStreamIO.cc
LIBSRCS += ipIgnoreEntry.cc

# There is a bug in some vxWorks compilers that these work around:
ifeq ($(VX_GNU_VERSION), 4.1.2)
  casStreamOS_CXXFLAGS_vxWorks-ppc604_altivec = -O0
  casStreamOS_CXXFLAGS_vxWorks-ppc604_long = -O0
  casStreamOS_CXXFLAGS_vxWorks-ppc604 = -O0
endif

LIBSRCS_vxWorks += templateInstances.cpp

LIBRARY = cas
cas_LIBS = ca gdd Com
cas_SYS_LIBS_WIN32 = ws2_32
cas_RCS = cas.rc

include $(TOP)/configure/RULES

clean::
	@$(RM) Templates.DB

