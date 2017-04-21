#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/log
INC += iocLog.h
INC += logClient.h
Com_SRCS += iocLog.c
Com_SRCS += logClient.c

PROD_HOST += iocLogServer

iocLogServer_SRCS = iocLogServer.c
iocLogServer_LIBS = Com

iocLogServer_SYS_LIBS_solaris += socket
iocLogServer_SYS_LIBS_WIN32   += user32 ws2_32

SCRIPTS_HOST = S99logServer

EXPAND += S99logServer@
EXPAND_VARS = INSTALL_BIN=$(abspath $(INSTALL_BIN))

