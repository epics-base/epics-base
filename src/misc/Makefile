#*************************************************************************
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../..
include $(TOP)/configure/CONFIG

DBD += base.dbd
DBD += system.dbd
DBD += dlload.dbd

INC += epicsRelease.h
INC += iocInit.h
INC += miscIocRegister.h
INC += iocshRegisterCommon.h

LIB_SRCS += epicsRelease.c
LIB_SRCS += iocInit.c
LIB_SRCS += asSubRecordFunctions.c
LIB_SRCS += miscIocRegister.c
LIB_SRCS += dlload.c
LIB_SRCS += iocshRegisterCommon.c


LIBRARY_IOC = miscIoc

miscIoc_RCS = miscIoc.rc
miscIoc_LIBS  = rsrvIoc asIoc dbtoolsIoc dbIoc registryIoc dbStaticIoc ca Com

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=miscIoc
OBJLIB_SRCS = $(LIB_SRCS)
endif

include $(TOP)/configure/RULES

