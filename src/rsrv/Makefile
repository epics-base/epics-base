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

USR_INCLUDES = -I../../ca

INC += rsrv.h

LIBSRCS += caserverio.c
LIBSRCS += caservertask.c
LIBSRCS += camsgtask.c
LIBSRCS += camessage.c
LIBSRCS += cast_server.c
LIBSRCS += online_notify.c 

LIBRARY_IOC = rsrvIoc
rsrvIoc_LIBS = asIoc dbIoc ca Com 
rsrvIoc_SYS_LIBS_WIN32 := ws2_32
rsrvIoc_RCS_WIN32 = rsrvIoc.rc

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=rsrvIoc
OBJLIB_SRCS = $(LIBSRCS)
endif

include $(TOP)/configure/RULES
