#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..
include $(TOP)/configure/CONFIG

INC += asDbLib.h
INC += asCa.h
INC += asIocRegister.h

asIoc_SRCS += asDbLib.c
asIoc_SRCS += asCa.c
asIoc_SRCS += asIocRegister.c

LIBRARY_IOC = asIoc

asIoc_LIBS  = dbIoc dbStaticIoc Com

asIoc_RCS = asIoc.rc

PROD_HOST = ascheck
ascheck_SRCS = ascheck.c
PROD_LIBS = dbStaticHost Com

include $(TOP)/configure/RULES
