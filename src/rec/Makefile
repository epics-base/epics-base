TOP=../..
include $(TOP)/configure/CONFIG


INC += egDefs.h
INC += erDefs.h
INC += ereventDefs.h

#USER_DBDFLAGS += -I .. -I $(EPICS_BASE)/dbd
RECTYPES += aaiRecord.h
RECTYPES += aaoRecord.h
RECTYPES += aiRecord.h
RECTYPES += aoRecord.h
RECTYPES += biRecord.h
RECTYPES += boRecord.h
RECTYPES += calcRecord.h
RECTYPES += calcoutRecord.h
RECTYPES += compressRecord.h
RECTYPES += dfanoutRecord.h
RECTYPES += egRecord.h
RECTYPES += egeventRecord.h
RECTYPES += erRecord.h
RECTYPES += ereventRecord.h
RECTYPES += eventRecord.h
RECTYPES += fanoutRecord.h
RECTYPES += gsubRecord.h
RECTYPES += histogramRecord.h
RECTYPES += longinRecord.h
RECTYPES += longoutRecord.h
RECTYPES += mbbiRecord.h
RECTYPES += mbbiDirectRecord.h
RECTYPES += mbboRecord.h
RECTYPES += mbboDirectRecord.h
RECTYPES += palRecord.h
RECTYPES += permissiveRecord.h
RECTYPES += pidRecord.h
RECTYPES += pulseCounterRecord.h
RECTYPES += pulseDelayRecord.h
RECTYPES += pulseTrainRecord.h
RECTYPES += selRecord.h
RECTYPES += seqRecord.h
RECTYPES += stateRecord.h
RECTYPES += steppermotorRecord.h
RECTYPES += stringinRecord.h
RECTYPES += stringoutRecord.h
RECTYPES += subRecord.h
RECTYPES += subArrayRecord.h
RECTYPES += timerRecord.h
RECTYPES += waveformRecord.h

OBJS_IOC += aaiRecord.o
OBJS_IOC += aaoRecord.o
OBJS_IOC += aiRecord.o
OBJS_IOC += aoRecord.o
OBJS_IOC += biRecord.o
OBJS_IOC += boRecord.o
OBJS_IOC += calcRecord.o
OBJS_IOC += calcoutRecord.o
OBJS_IOC += compressRecord.o
OBJS_IOC += dfanoutRecord.o
OBJS_IOC += egRecord.o
OBJS_IOC += egeventRecord.o
OBJS_IOC += erRecord.o
OBJS_IOC += ereventRecord.o
OBJS_IOC += eventRecord.o
OBJS_IOC += fanoutRecord.o
OBJS_IOC += gsubRecord.o
OBJS_IOC += histogramRecord.o
OBJS_IOC += longinRecord.o
OBJS_IOC += longoutRecord.o
OBJS_IOC += mbbiRecord.o
OBJS_IOC += mbbiDirectRecord.o
OBJS_IOC += mbboRecord.o
OBJS_IOC += mbboDirectRecord.o
OBJS_IOC += palRecord.o
OBJS_IOC += permissiveRecord.o
OBJS_IOC += pidRecord.o
OBJS_IOC += pulseCounterRecord.o
OBJS_IOC += pulseDelayRecord.o
OBJS_IOC += pulseTrainRecord.o
OBJS_IOC += selRecord.o
OBJS_IOC += seqRecord.o
OBJS_IOC += stateRecord.o
#OBJS_IOC += steppermotorRecord.o
OBJS_IOC += stringinRecord.o
OBJS_IOC += stringoutRecord.o
OBJS_IOC += subRecord.o
OBJS_IOC += subArrayRecord.o
OBJS_IOC += timerRecord.o
OBJS_IOC += waveformRecord.o

include $(TOP)/configure/RULES

aaiRecord.o : aaiRecord.h

%Record.o : %Record.h


$(OBJS_IOC) : $(RECTYPES)
