#*************************************************************************
# Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

PROD_LIBS  = ca Com

PROD_HOST_DEFAULT = ca_test iocLogServer
PROD_HOST_WIN32   = ca_test iocLogServer
PROD_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

iocLogServer_SYS_LIBS_solaris = socket

ca_test_SRCS = ca_test_main.c ca_test.c
iocLogServer_SRCS = iocLogServer.c

OBJS_vxWorks = ca_test

SCRIPTS_HOST = S99logServer S99caRepeater
SCRIPTS_Linux = caRepeater.service

EXPAND += S99logServer@
EXPAND += S99caRepeater@
EXPAND += caRepeater.service@

EXPAND_VARS = INSTALL_BIN=$(abspath $(INSTALL_BIN))

include $(TOP)/configure/RULES
