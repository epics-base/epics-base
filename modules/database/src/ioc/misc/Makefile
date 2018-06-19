#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/misc

DBD += system.dbd
DBD += dlload.dbd
DBD += dbCore.dbd

INC += epicsRelease.h
INC += iocInit.h
INC += miscIocRegister.h
INC += iocshRegisterCommon.h

dbCore_SRCS += epicsRelease.c
dbCore_SRCS += iocInit.c
dbCore_SRCS += miscIocRegister.c
dbCore_SRCS += dlload.c
dbCore_SRCS += iocshRegisterCommon.c

