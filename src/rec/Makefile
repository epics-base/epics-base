TOP=../..
include $(TOP)/configure/CONFIG

SHARED_LIBRARIES = NO

INC += egDefs.h
INC += erDefs.h
INC += ereventDefs.h

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
DBDINC += egRecord
DBDINC += egeventRecord
DBDINC += erRecord
DBDINC += ereventRecord
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
LIBSRCS += egRecord.c
LIBSRCS += egeventRecord.c
LIBSRCS += erRecord.c
LIBSRCS += ereventRecord.c
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

# For R3.13 compatability
OBJS_vxWorks = $(LIBSRCS:%.c=%)

include $(TOP)/configure/RULES

