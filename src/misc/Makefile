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
INC +=  asSubRecordFunctions.h

LIBSRCS +=  epicsRelease.c
LIBSRCS +=  iocInit.c
LIBSRCS +=  asSubRecordFunctions.c

LIBRARY_IOC = miscIoc

miscIoc_RCS_WIN32 = miscIoc.rc
miscIoc_LIBS  = dbStaticIoc dbIoc asIoc rsrvIoc registryIoc Com

# For R3.13 compatibility only
OBJLIB_vxWorks=miscIoc
OBJLIB_SRCS = $(LIBSRCS)

include $(TOP)/configure/RULES

