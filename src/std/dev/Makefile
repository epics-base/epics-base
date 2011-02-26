#*************************************************************************
# Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

DBD += devSoft.dbd

LIBSRCS += devAaiSoft.c
LIBSRCS += devAaoSoft.c
LIBSRCS += devAiSoft.c
LIBSRCS += devAiSoftRaw.c
LIBSRCS += devAoSoft.c
LIBSRCS += devAoSoftRaw.c
LIBSRCS += devBiSoft.c
LIBSRCS += devBiSoftRaw.c
LIBSRCS += devBoSoft.c
LIBSRCS += devBoSoftRaw.c
LIBSRCS += devCalcoutSoft.c
LIBSRCS += devEventSoft.c
LIBSRCS += devHistogramSoft.c
LIBSRCS += devLiSoft.c
LIBSRCS += devLoSoft.c
LIBSRCS += devMbbiDirectSoft.c
LIBSRCS += devMbbiDirectSoftRaw.c
LIBSRCS += devMbbiSoft.c
LIBSRCS += devMbbiSoftRaw.c
LIBSRCS += devMbboDirectSoft.c
LIBSRCS += devMbboDirectSoftRaw.c
LIBSRCS += devMbboSoft.c
LIBSRCS += devMbboSoftRaw.c
LIBSRCS += devSASoft.c
LIBSRCS += devSiSoft.c
LIBSRCS += devSoSoft.c
LIBSRCS += devWfSoft.c
LIBSRCS += devGeneralTime.c

LIBSRCS += devAoSoftCallback.c
LIBSRCS += devBoSoftCallback.c
LIBSRCS += devCalcoutSoftCallback.c
LIBSRCS += devLoSoftCallback.c
LIBSRCS += devMbboSoftCallback.c
LIBSRCS += devMbboDirectSoftCallback.c
LIBSRCS += devSoSoftCallback.c

LIBSRCS += devTimestamp.c
LIBSRCS += devSoStdio.c

LIB_SRCS += asSubRecordFunctions.c

LIBRARY_IOC += softDevIoc
softDevIoc_LIBS += dbCore ca Com
softDevIoc_RCS = softDevIoc.rc

include $(TOP)/configure/RULES


