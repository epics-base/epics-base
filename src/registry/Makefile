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

INC += registry.h
INC += registryRecordType.h
INC += registryDeviceSupport.h
INC += registryDriverSupport.h
INC += registryFunction.h


SCRIPTS_HOST += registerRecordDeviceDriver.pl

LIBSRCS += registry.c
LIBSRCS += registryRecordType.c
LIBSRCS += registryDeviceSupport.c
LIBSRCS += registryDriverSupport.c
LIBSRCS += registryFunction.c

LIBRARY_IOC = registryIoc
registryIoc_LIBS  = Com

registryIoc_RCS_WIN32 = registryIoc.rc

ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks = registryIoc
OBJLIB_SRCS = $(LIBSRCS)
endif

include $(TOP)/configure/RULES

