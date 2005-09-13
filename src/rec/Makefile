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

DBDINC += aaiRecord
DBDINC += aaoRecord
DBDINC += aiRecord
DBDINC += aoRecord
DBDINC += biRecord
DBDINC += boRecord
DBDINC += calcRecord
DBDINC += calcoutRecord
DBDINC += compressRecord
DBDINC += dfanoutRecord
DBDINC += eventRecord
DBDINC += fanoutRecord
DBDINC += histogramRecord
DBDINC += longinRecord
DBDINC += longoutRecord
DBDINC += mbbiRecord
DBDINC += mbbiDirectRecord
DBDINC += mbboRecord
DBDINC += mbboDirectRecord
DBDINC += permissiveRecord
DBDINC += selRecord
DBDINC += seqRecord
DBDINC += stateRecord
DBDINC += stringinRecord
DBDINC += stringoutRecord
DBDINC += subRecord
DBDINC += subArrayRecord
DBDINC += waveformRecord

LIBSRCS += aaiRecord.c
LIBSRCS += aaoRecord.c
LIBSRCS += aiRecord.c
LIBSRCS += aoRecord.c
LIBSRCS += biRecord.c
LIBSRCS += boRecord.c
LIBSRCS += calcRecord.c
LIBSRCS += calcoutRecord.c
LIBSRCS += compressRecord.c
LIBSRCS += dfanoutRecord.c
LIBSRCS += eventRecord.c
LIBSRCS += fanoutRecord.c
LIBSRCS += histogramRecord.c
LIBSRCS += longinRecord.c
LIBSRCS += longoutRecord.c
LIBSRCS += mbbiRecord.c
LIBSRCS += mbbiDirectRecord.c
LIBSRCS += mbboRecord.c
LIBSRCS += mbboDirectRecord.c
LIBSRCS += permissiveRecord.c
LIBSRCS += selRecord.c
LIBSRCS += seqRecord.c
LIBSRCS += stateRecord.c
LIBSRCS += stringinRecord.c
LIBSRCS += stringoutRecord.c
LIBSRCS += subRecord.c
LIBSRCS += subArrayRecord.c
LIBSRCS += waveformRecord.c

LIBRARY_IOC += recIoc

recIoc_LIBS = asIoc dbIoc registryIoc dbStaticIoc ca Com
recIoc_RCS_WIN32 = recIoc.rc

# For R3.13 compatability
ifeq ($(strip $(COMPAT_313)),YES)
OBJS_vxWorks = $(LIBSRCS:%.c=%)
endif

include $(TOP)/configure/RULES

