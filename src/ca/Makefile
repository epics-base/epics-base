
TOP=../..

include $(TOP)/configure/CONFIG

CMPLR = STRICT

#
#	includes to install from this subproject
#
INC += cadef.h
INC += caerr.h
INC += caeventmask.h
INC += caProto.h
INC += db_access.h
INC += pvAdapter.h

#LIBSRCS += caOsDependent.c
LIBSRCS += access.cpp
LIBSRCS += processThread.cpp
LIBSRCS += iocinf.cpp
LIBSRCS += service.cpp
LIBSRCS += conn.cpp
LIBSRCS += flow_control.cpp
LIBSRCS += syncgrp.cpp
LIBSRCS += convert.cpp
LIBSRCS += test_event.cpp
#LIBSRCS += bsd_depen.c
LIBSRCS += repeater.cpp
LIBSRCS += ringBuffer.cpp
LIBSRCS += searchTimer.cpp
LIBSRCS += repeaterSubscribeTimer.cpp

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

PROD += catime acctst 
catime_SRCS = catime.c
acctst_SRCS = acctst.c

include $(TOP)/configure/RULES


