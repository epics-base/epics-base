#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/rsrv

# These need access to net_convert.h from the CA client
caserverio_INCLUDES = -I$(SRC)/ca/client
camessage_INCLUDES = -I$(SRC)/ca/client

INC += rsrv.h
INC += rsrvIocRegister.h

dbCore_SRCS += caserverio.c
dbCore_SRCS += caservertask.c
dbCore_SRCS += camsgtask.c
dbCore_SRCS += camessage.c
dbCore_SRCS += cast_server.c
dbCore_SRCS += online_notify.c
dbCore_SRCS += rsrvIocRegister.c

