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

SHARED_LIBRARIES = NO

SRCS += devAiSoft.c
SRCS += devAiSoftRaw.c
SRCS += devAoSoft.c
SRCS += devAoSoftRaw.c
SRCS += devBiSoft.c
SRCS += devBiSoftRaw.c
SRCS += devBoSoft.c
SRCS += devBoSoftRaw.c
SRCS += devEventSoft.c
SRCS += devHistogramSoft.c
SRCS += devLiSoft.c
SRCS += devLoSoft.c
SRCS += devMbbiDirectSoft.c
SRCS += devMbbiDirectSoftRaw.c
SRCS += devMbbiSoft.c
SRCS += devMbbiSoftRaw.c
SRCS += devMbboDirectSoft.c
SRCS += devMbboDirectSoftRaw.c
SRCS += devMbboSoft.c
SRCS += devMbboSoftRaw.c
SRCS += devSASoft.c
SRCS += devSiSoft.c
SRCS += devSoSoft.c
SRCS += devWfSoft.c

LIBRARY_IOC += softDevIoc

# For R3.13 compatability
OBJS_vxWorks = $(SRCS:%.c=%)

include $(TOP)/configure/RULES


