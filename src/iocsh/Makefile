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


#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

LIBSRCS += iocsh.cpp
LIBSRCS += osiRegister.c
LIBSRCS += dbStaticRegister.c
LIBSRCS += dbTestRegister.c
LIBSRCS += dbBkptRegister.c
LIBSRCS += dbCaTestRegister.c
LIBSRCS += caTestRegister.c
LIBSRCS += dbAccessRegister.c
LIBSRCS += iocshRegisterCommon.c
LIBSRCS += asTestRegister.c
LIBSRCS += envRegister.c
LIBSRCS += iocUtil.c
LIBSRCS += iocCoreLimitsRegister.c
LIBSRCS += systemCommandRegister.c
LIBSRCS += registryCommon.c
LIBSRCS += registryRegister.c

LIBRARY_IOC = iocsh

iocsh_LIBS  = miscIoc rsrvIoc dbtoolsIoc asIoc dbIoc registryIoc dbStaticIoc Com
iocsh_RCS_WIN32 = iocsh.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=iocsh
OBJLIB_SRCS = $(LIBSRCS)
endif

include $(TOP)/configure/RULES

