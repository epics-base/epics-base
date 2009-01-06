#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

DBD += devTestAsyn.dbd

LIBSRCS += devHistogramTestAsyn.c
LIBSRCS += devTestAsyn.c

LIBRARY_IOC = testDevIoc
testDevIoc_LIBS += miscIoc recIoc asIoc dbIoc registryIoc dbStaticIoc ca Com
testDevIoc_RCS = testDevIoc.rc

# For R3.13 compatability
ifeq ($(strip $(COMPAT_313)),YES)
OBJS_vxWorks = $(LIBSRCS:%.c=%)
endif

include $(TOP)/configure/RULES
