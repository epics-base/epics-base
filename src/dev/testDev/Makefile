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

LIBSRCS += devAiTestAsyn.c
LIBSRCS += devAoTestAsyn.c
LIBSRCS += devBiTestAsyn.c
LIBSRCS += devBoTestAsyn.c
LIBSRCS += devEventTestIoEvent.c
LIBSRCS += devHistogramTestAsyn.c
LIBSRCS += devMbbiTestAsyn.c
LIBSRCS += devMbboTestAsyn.c
LIBSRCS += devSiTestAsyn.c
LIBSRCS += devSoTestAsyn.c
LIBSRCS += devWfTestAsyn.c

LIBRARY_IOC = testDevIoc
testDevIoc_LIBS += recIoc asIoc dbIoc registryIoc dbStaticIoc ca Com

# For R3.13 compatability
OBJS_vxWorks = $(LIBSRCS:%.c=%)

include $(TOP)/configure/RULES
