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

DBD += base.dbd

INC +=  epicsRelease.h
INC +=  iocInit.h

LIBSRCS +=  epicsRelease.c
LIBSRCS +=  iocInit.c
LIBSRCS +=  asSubRecordFunctions.c

LIBRARY_IOC = miscIoc

miscIoc_RCS_WIN32 = miscIoc.rc
miscIoc_LIBS  = rsrvIoc asIoc dbIoc registryIoc dbStaticIoc ca Com

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=miscIoc
OBJLIB_SRCS = $(LIBSRCS)
endif

include $(TOP)/configure/RULES

