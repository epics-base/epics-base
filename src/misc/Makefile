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
DLL_LIBS = Com dbStaticIoc dbIoc asIoc rsrvIoc registryIoc  
LIBRARY_IOC = miscIoc

miscIoc_RCS_WIN32 = miscIoc.rc
miscIoc_DLL_LIBS  = rsrvIoc asIoc dbIoc registryIoc dbStaticIoc Com

misc_RCS_WIN32 = misc.rc

# For R3.13 compatibility only
OBJLIB_vxWorks=miscIoc
OBJLIB_SRCS = $(LIBSRCS)

include $(TOP)/configure/RULES

