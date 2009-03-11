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

USR_INCLUDES = -I../../ca

INC += rsrv.h
INC += rsrvIocRegister.h

LIB_SRCS += caserverio.c
LIB_SRCS += caservertask.c
LIB_SRCS += camsgtask.c
LIB_SRCS += camessage.c
LIB_SRCS += cast_server.c
LIB_SRCS += online_notify.c
LIB_SRCS += rsrvIocRegister.c

LIBRARY_IOC = rsrvIoc

rsrvIoc_LIBS = asIoc dbIoc ca Com

rsrvIoc_SYS_LIBS_WIN32 := ws2_32
rsrvIoc_RCS = rsrvIoc.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=rsrvIoc
OBJLIB_SRCS = $(LIB_SRCS)
endif

include $(TOP)/configure/RULES
