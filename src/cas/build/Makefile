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

# sometimes it's good to have different C/C++ flags, not now:
USR_CPPFLAGS = -DcaNetAddrSock
#USR_CXXFLAGS = $(USR_CFLAGS)

INC += casdef.h
INC += casEventMask.h
INC += casInternal.h
INC += caNetAddr.h

LIBSRCS += caServer.cc
LIBSRCS += caServerI.cc
LIBSRCS += casCoreClient.cc
LIBSRCS += casClient.cc
LIBSRCS += casDGClient.cc
LIBSRCS += casStrmClient.cc
LIBSRCS += casPV.cc
LIBSRCS += casPVI.cc
LIBSRCS += casChannel.cc
LIBSRCS += casChannelI.cc
LIBSRCS += casPVListChan.cc
LIBSRCS += casClientMon.cc
LIBSRCS += casChanDelEv.cc
LIBSRCS += casAsyncIOI.cc
LIBSRCS += casAsyncReadIO.cc
LIBSRCS += casAsyncWriteIO.cc
LIBSRCS += casAsyncPVExistIO.cc
LIBSRCS += casAsyncPVAttachIO.cc
LIBSRCS += casEventSys.cc
LIBSRCS += casMonitor.cc
LIBSRCS += casMonEvent.cc
LIBSRCS += inBuf.cc
LIBSRCS += outBuf.cc
LIBSRCS += casCtx.cc 
LIBSRCS += casEventMask.cc
LIBSRCS += ioBlocked.cc

LIBSRCS += caServerOS.cc
LIBSRCS += casIntfOS.cc
LIBSRCS += casDGIntfOS.cc
LIBSRCS += casStreamOS.cc

LIBSRCS += caServerIO.cc
LIBSRCS += casIntfIO.cc
LIBSRCS += casDGIntfIO.cc
LIBSRCS += casStreamIO.cc

LIBRARY = cas
DLL_LIBS = ca gdd Com
SYS_DLL_LIBS = ws2_32

#PROD_LIBS := cas ca gdd Com 
#SYS_PROD_LIBS_WIN32 := ws2_32

include $(TOP)/configure/RULES

clean::
	@$(RM) -rf Templates.DB

