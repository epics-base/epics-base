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

SRCS += devAiTestAsyn.c
SRCS += devAoTestAsyn.c
SRCS += devBiTestAsyn.c
SRCS += devBoTestAsyn.c
SRCS += devEventTestIoEvent.c
SRCS += devHistogramTestAsyn.c
SRCS += devMbbiTestAsyn.c
SRCS += devMbboTestAsyn.c
SRCS += devSiTestAsyn.c
SRCS += devSoTestAsyn.c
SRCS += devWfTestAsyn.c

LIBRARY_IOC = testDevIoc
testDevIoc_LIBS += dbIoc

# For R3.13 compatability
OBJS_vxWorks = $(SRCS:%.c=%)

include $(TOP)/configure/RULES
