#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../../../..

include $(TOP)/configure/CONFIG

PROD_LIBS := cas ca gdd Com
#cas_DIR = $(INSTALL_LIB)
#ca_DIR = $(INSTALL_LIB)
#gdd_DIR = $(INSTALL_LIB)
#Com_DIR = $(INSTALL_LIB)

#
# Added ws2_32 winmm user32 for the non-dll build
#
PROD_SYS_LIBS_WIN32 := ws2_32 advapi32 user32

SRCS += main.cc
SRCS += exServer.cc
SRCS += exPV.cc
SRCS += exVectorPV.cc
SRCS += exScalarPV.cc
SRCS += exAsyncPV.cc
SRCS += exChannel.cc

PROD_HOST = excas

include $(TOP)/configure/RULES


