#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

INC += registryRecordType.h
INC += registryDeviceSupport.h
INC += registryDriverSupport.h
INC += registryFunction.h
INC += registryCommon.h
INC += registryIocRegister.h

SCRIPTS_HOST += registerRecordDeviceDriver.pl

LIB_SRCS += registryRecordType.c
LIB_SRCS += registryDeviceSupport.c
LIB_SRCS += registryDriverSupport.c
LIB_SRCS += registryFunction.c
LIB_SRCS += registryCommon.c
LIB_SRCS += registryIocRegister.c

LIBRARY_IOC = registryIoc
registryIoc_LIBS  = dbStaticIoc Com

registryIoc_RCS = registryIoc.rc

ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks = registryIoc
OBJLIB_SRCS = $(LIB_SRCS)
endif

include $(TOP)/configure/RULES

