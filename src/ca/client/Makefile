#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../../..

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

LIBSRCS += cac.cpp
LIBSRCS += cacChannel.cpp
LIBSRCS += cacChannelNotify.cpp
LIBSRCS += cacContextNotify.cpp
LIBSRCS += cacReadNotify.cpp
LIBSRCS += cacWriteNotify.cpp
LIBSRCS += cacStateNotify.cpp
LIBSRCS += access.cpp
LIBSRCS += iocinf.cpp
LIBSRCS += convert.cpp
LIBSRCS += test_event.cpp
LIBSRCS += repeater.cpp
LIBSRCS += searchTimer.cpp
LIBSRCS += disconnectGovernorTimer.cpp
LIBSRCS += repeaterSubscribeTimer.cpp
LIBSRCS += baseNMIU.cpp
LIBSRCS += nciu.cpp
LIBSRCS += netiiu.cpp
LIBSRCS += udpiiu.cpp
LIBSRCS += tcpiiu.cpp
LIBSRCS += noopiiu.cpp
LIBSRCS += netReadNotifyIO.cpp
LIBSRCS += netWriteNotifyIO.cpp
LIBSRCS += netSubscription.cpp
LIBSRCS += tcpSendWatchdog.cpp
LIBSRCS += tcpRecvWatchdog.cpp
LIBSRCS += bhe.cpp
LIBSRCS += ca_client_context.cpp
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

LIBRARY=ca

ca_RCS = ca.rc

ca_LIBS = Com

ca_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

#   libs needed for PROD and TESTPRODUCT
PROD_LIBS = ca Com 
# needed when its an object library build
PROD_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

PROD_DEFAULT += caRepeater catime acctst caConnTest casw caEventRate
PROD_vxWorks = -nil-
PROD_RTEMS = -nil-
PROD_iOS = -nil-

OBJS_vxWorks = catime acctst caConnTest casw caEventRate acctstRegister

caRepeater_SRCS = caRepeater.cpp
catime_SRCS = catimeMain.c catime.c
acctst_SRCS = acctstMain.c acctst.c
caEventRate_SRCS = caEventRateMain.cpp caEventRate.cpp
casw_SRCS = casw.cpp
caConnTest_SRCS = caConnTestMain.cpp caConnTest.cpp

casw_SYS_LIBS_solaris = socket

SCRIPTS_HOST = S99caRepeater
SCRIPTS_Linux = caRepeater.service

EXPAND += S99caRepeater@
EXPAND += caRepeater.service@
EXPAND_VARS = INSTALL_BIN=$(abspath $(INSTALL_BIN))

SRC_DIRS += $(TOP)/src/ca/client/test
PROD_HOST += ca_test
ca_test_SRCS = ca_test_main.c ca_test.c
ca_test_LIBS  = ca Com
ca_test_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

OBJS_vxWorks += ca_test

include $(TOP)/configure/RULES
