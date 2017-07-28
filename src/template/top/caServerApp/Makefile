#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=..

include $(TOP)/configure/CONFIG

PROD_LIBS += $(EPICS_BASE_HOST_LIBS)

#
# Added ws2_32 winmm user32 for the non-dll build
#
PROD_SYS_LIBS_WIN32 += ws2_32 advapi32 user32

casexample_SRCS += main.cc
casexample_SRCS += exServer.cc
casexample_SRCS += exPV.cc
casexample_SRCS += exVectorPV.cc
casexample_SRCS += exScalarPV.cc
casexample_SRCS += exAsyncPV.cc
casexample_SRCS += exChannel.cc

PROD_HOST = casexample

include $(TOP)/configure/RULES


