
TOP=../..

include $(TOP)/configure/CONFIG

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

LIBSRCS += cacChannel.cpp
LIBSRCS += cacChannelIO.cpp
LIBSRCS += cacNotify.cpp
LIBSRCS += cacNotifyIO.cpp
LIBSRCS += cacServiceList.cpp
LIBSRCS += access.cpp
LIBSRCS += recvProcessThread.cpp
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
LIBSRCS += netReadCopyIO.cpp
LIBSRCS += netReadNotifyIO.cpp
LIBSRCS += netWriteNotifyIO.cpp
LIBSRCS += netSubscription.cpp
LIBSRCS += cac.cpp
LIBSRCS += tcpSendWatchdog.cpp
LIBSRCS += tcpRecvWatchdog.cpp
LIBSRCS += bhe.cpp
LIBSRCS += oldChannel.cpp
LIBSRCS += oldSubscription.cpp
LIBSRCS += getCallback.cpp
LIBSRCS += putCallback.cpp
LIBSRCS += syncgrp.cpp
LIBSRCS += CASG.cpp
LIBSRCS += syncGroupNotify.cpp
LIBSRCS += localHostName.cpp
LIBSRCS += comQueRecv.cpp
LIBSRCS += comQueSend.cpp
LIBSRCS += cacPrivateListOfIO.cpp
LIBSRCS += hostNameCache.cpp
LIBSRCS += ioCounter.cpp
LIBSRCS += msgForMultiplyDefinedPV.cpp

LIBRARY=ca

#   only for WIN32:
#   build LIBRARY as a DLL, use the specified def file
LIBTYPE := SHARED
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
PROD += caRepeater

PROD_DEFAULT += catime acctst caConnTest
catime_SRCS = catimeMain.c catime.c 
acctst_SRCS = acctstMain.c acctst.c 
caConnTest_SRCS = caConnTestMain.cpp caConnTest.cpp 

PROD_vxWorks = catime acctst caConnTest

include $(TOP)/configure/RULES


