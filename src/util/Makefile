#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

PROD_LIBS  = ca Com

#
# Added winmm user32 for the non-dll build
#
PROD_HOST_DEFAULT = ca_test iocLogServer
PROD_HOST_WIN32   = ca_test iocLogServer
PROD_SYS_LIBS_WIN32 = ws2_32 advapi32 user32

iocLogServer_SYS_LIBS_solaris = socket

ca_test_SRCS = ca_test_main.c ca_test.c
iocLogServer_SRCS = iocLogServer.c

OBJS_vxWorks = ca_test

SCRIPTS_solaris := S99logServer S99caRepeater 
SCRIPTS_Linux := S99logServer S99caRepeater 

include $(TOP)/configure/RULES
 
S99%: ../rc2.%
	sed -e s%:INSTALL_BIN:%`cd $(INSTALL_BIN); pwd`% $< >$@

#	EOF Makefile.Host for base/src/util
