
TOP=../..

include $(TOP)/configure/CONFIG

HTMLS += CAref.html

#
#	includes to install from this subproject
#
INC += cadef.h
INC += caerr.h
INC += caeventmask.h
INC += caProto.h
INC += db_access.h
INC += addrList.h
INC += cacIO.h
INC += caDiagnostics.h

LIBSRCS += cacChannel.cpp
LIBSRCS += cacChannelNotify.cpp
LIBSRCS += cacNotify.cpp
LIBSRCS += cacReadNotify.cpp
LIBSRCS += cacWriteNotify.cpp
LIBSRCS += cacStateNotify.cpp
LIBSRCS += cacServiceList.cpp
LIBSRCS += access.cpp
LIBSRCS += iocinf.cpp
LIBSRCS += convert.cpp
LIBSRCS += test_event.cpp
LIBSRCS += repeater.cpp
LIBSRCS += searchTimer.cpp
LIBSRCS += repeaterSubscribeTimer.cpp
LIBSRCS += tcpiiu.cpp
LIBSRCS += udpiiu.cpp
LIBSRCS += netiiu.cpp
LIBSRCS += nciu.cpp
LIBSRCS += baseNMIU.cpp
LIBSRCS += netReadNotifyIO.cpp
LIBSRCS += netWriteNotifyIO.cpp
LIBSRCS += netSubscription.cpp
LIBSRCS += cac.cpp
LIBSRCS += tcpSendWatchdog.cpp
LIBSRCS += tcpRecvWatchdog.cpp
LIBSRCS += bhe.cpp
LIBSRCS += oldCAC.cpp
LIBSRCS += oldChannelNotify.cpp
LIBSRCS += oldSubscription.cpp
LIBSRCS += getCallback.cpp
LIBSRCS += getCopy.cpp
LIBSRCS += putCallback.cpp
LIBSRCS += syncgrp.cpp
LIBSRCS += CASG.cpp
LIBSRCS += syncGroupNotify.cpp
LIBSRCS += syncGroupReadNotify.cpp
LIBSRCS += syncGroupWriteNotify.cpp
LIBSRCS += localHostName.cpp
LIBSRCS += comQueRecv.cpp
LIBSRCS += comQueSend.cpp
LIBSRCS += comBuf.cpp
LIBSRCS += hostNameCache.cpp
LIBSRCS += msgForMultiplyDefinedPV.cpp
LIBSRCS += limboiiu.cpp

LIBRARY=ca

# For R3.13 compatibility only
OBJLIB_vxWorks=ca
OBJLIB_SRCS = $(LIBSRCS)

#   only for WIN32:
SYS_DLL_LIBS = ws2_32
DLL_LIBS = Com

#   libs needed for PROD and TESTPRODUCT
PROD_LIBS = ca Com 
# needed when its an object library build
SYS_PROD_LIBS_WIN32 = ws2_32 advapi32 user32

#	libs needed to build [TEST]PRODUCT

ca_DIR = .
Com_DIR = $(INSTALL_LIB)

caRepeater_SRCS = caRepeater.cpp
PROD_HOST += caRepeater

PROD_HOST += catime acctst caConnTest casw caEventRate
OBJS_IOC += catime acctst caConnTest casw caEventRate
catime_SRCS = catimeMain.c catime.c 
acctst_SRCS = acctstMain.c acctst.c 
caEventRate_SRCS = caEventRateMain.cpp caEventRate.cpp
casw_SRCS = casw.cpp
caConnTest_SRCS = caConnTestMain.cpp caConnTest.cpp 

include $(TOP)/configure/RULES

