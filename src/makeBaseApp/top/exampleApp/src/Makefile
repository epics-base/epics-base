TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#USR_CFLAGS += 
PROD_LIBS	+= ca
PROD_LIBS	+= Db
PROD_LIBS	+= Com
ca_DIR  = $(EPICS_BASE_LIB)
Db_DIR  = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)

PROD += caExample

#USR_CFLAGS += 

RECTYPES += xxxRecord.h
DBDEXPAND = exampleInclude.dbd
DBDNAME = exampleApp.dbd

LIBRARY_IOC = exampleLib

exampleLib_SRCS += xxxRecord.c
exampleLib_SRCS += devXxxSoft.c
exampleLib_SRCS += sncExample.c

#exampleLib_SRCS += registerRecordDeviceDriver.c

LIBOBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

LIBOBJS += $(EPICS_BASE_BIN)/iocCore
LIBOBJS += $(EPICS_BASE_BIN)/seq

-include ../baseLIBOBJS

#Note that the command line that builds the
#library $(LIBRARY_IOC) may be HUGE (>3kB)
#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
