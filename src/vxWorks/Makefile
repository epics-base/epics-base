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

iocCore_OBJS += $(INSTALL_BIN)/ComLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/caLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbStaticIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/registryIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/asIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/miscIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbtoolsIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/rsrvIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/vxComLibrary.o
iocCore_SRCS += registerRecordDeviceDriver.c

ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks = iocCore
OBJLIB_OBJS += $(iocCore_OBJS)
OBJLIB_SRCS += registerRecordDeviceDriver.c
endif

include $(TOP)/configure/RULES

