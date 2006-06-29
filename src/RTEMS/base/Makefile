#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..
include $(TOP)/configure/CONFIG

INC += epicsRtemsInitHooks.h

SRCS += rtems_init.c
SRCS += rtems_config.c
SRCS += rtems_netconfig.c
SRCS += rtems_util.c
SRCS += rtems_dummyreg.c
SRCS += setBootConfigFromNVRAM.c
SRCS += epicsRtemsInitHookPre.c
SRCS += epicsRtemsInitHookPost.c

LIBRARY_RTEMS = rtemsCom
LIBRARY_SRCS = $(SRCS) $(BUILD_ARCHS)

include $(TOP)/configure/RULES
