TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#USR_CFLAGS += 
PROD_LIBS	+= ca
PROD_LIBS	+= dbStaticHost
PROD_LIBS	+= Com
ca_DIR  = $(EPICS_BASE_LIB)
Db_DIR  = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)

PROD += caExample

#USR_CFLAGS += 

RECTYPES += xxxRecord.h
DBDEXPAND = exampleInclude.dbd
DBDNAME = exampleApp.dbd

MUNCH_LIBRARY = example

example_SRCS += xxxRecord.c
example_SRCS += devXxxSoft.c
#example_SRCS += sncExample.c

#example_SRCS += registerRecordDeviceDriver.c

example_OBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

example_OBJS += $(EPICS_BASE_BIN)/iocCoreLibrary.o
example_OBJS += $(EPICS_BASE_BIN)/iocInit.o
#example_OBJS += $(EPICS_BASE_BIN)/seq.o

-include ../baseLIBOBJS
example_OBJS += $(LIBOBJS)

#Note that the command line that builds the
#library $(LIBRARY_IOC) may be HUGE (>3kB)
#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
