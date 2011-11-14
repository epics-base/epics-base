#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..
include $(TOP)/configure/CONFIG

INC += epicsRtemsInitHooks.h

rtemsCom_SRCS += rtems_init.c
rtemsCom_SRCS += rtems_config.c
rtemsCom_SRCS += rtems_netconfig.c
rtemsCom_SRCS += rtems_util.c
rtemsCom_SRCS += setBootConfigFromNVRAM.c
rtemsCom_SRCS += epicsRtemsInitHookPre.c
rtemsCom_SRCS += epicsRtemsInitHookPost.c

LIBRARY_RTEMS = rtemsCom

include $(TOP)/configure/RULES
