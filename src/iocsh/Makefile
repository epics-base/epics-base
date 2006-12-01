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

#	includes to install from this sub-project
#

INC += iocsh.h
INC += osiRegister.h
INC += iocUtilRegister.h
INC += dbStaticRegister.h
INC += dbTestRegister.h
INC += dbBkptRegister.h
INC += dbCaTestRegister.h
INC += caTestRegister.h
INC += dbAccessRegister.h
INC += iocshRegisterCommon.h
INC += asTestRegister.h
INC += envRegister.h
INC += iocCoreLimitsRegister.h
INC += registryCommon.h
INC += registryRegister.h


LIB_SRCS += iocsh.cpp
LIB_SRCS += systemCommandRegister.c

iocsh_SRCS += osiRegister.c
iocsh_SRCS += dbStaticRegister.c
iocsh_SRCS += dbTestRegister.c
iocsh_SRCS += dbBkptRegister.c
iocsh_SRCS += dbCaTestRegister.c
iocsh_SRCS += caTestRegister.c
iocsh_SRCS += dbAccessRegister.c
iocsh_SRCS += iocshRegisterCommon.c
iocsh_SRCS += asTestRegister.c
iocsh_SRCS += envRegister.c
iocsh_SRCS += iocUtil.c
iocsh_SRCS += iocCoreLimitsRegister.c
iocsh_SRCS += registryCommon.c
iocsh_SRCS += registryRegister.c

LIBRARY_IOC = iocsh

iocsh_LIBS  = miscIoc rsrvIoc dbtoolsIoc asIoc dbIoc registryIoc dbStaticIoc Com
iocsh_RCS_WIN32 = iocsh.rc

LIBRARY_HOST = iocshHost

iocshHost_LIBS = registryIoc Com
iocshHost_RCS_WIN32 = iocshHost.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=iocsh
OBJLIB_SRCS = $(LIB_SRCS) $(iocsh_SRCS)
endif

include $(TOP)/configure/RULES

