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

INC += camacLib.h
INC += fast_lock.h
INC += drvTS.h
INC += devLib.h
INC += epicsDynLink.h
INC += module_types.h
INC += task_params.h

SRCS += drvTS.c
SRCS += devLib.c
SRCS += devLibVxWorks.c
SRCS += veclist.c
SRCS += epicsDynLink.c

OBJLIB_vxWorks += vxCom
OBJLIB_SRCS = $(SRCS)

include $(TOP)/configure/RULES
