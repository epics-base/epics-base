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
HOST_WARN=NO
USR_CFLAGS = -D_NO_PROTO

SRCS += closure.c
SRCS += error.c
SRCS += lalr.c
SRCS += lr0.c
SRCS += main.c
SRCS += mkpar.c
SRCS += output.c
SRCS += reader.c
SRCS += skeleton.c
SRCS += symtab.c
SRCS += verbose.c
SRCS += warshall.c

PROD_HOST = antelope
PROD_LIBS = Com

include $(TOP)/configure/RULES

inc: $(INSTALL_PROD)
