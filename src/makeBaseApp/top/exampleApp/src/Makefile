TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USER_DBDFLAGS += -I .. -I $(EPICS_BASE)/dbd
RECTYPES += xxxRecord.h
DBDEXPAND = exampleInclude.dbd
DBDNAME = exampleApp.dbd

#USR_CFLAGS += 
PROD_LIBS	+= ca
PROD_LIBS	+= Db
PROD_LIBS	+= Com
PROD_LIBS	+= Osi
ca_DIR  = $(EPICS_BASE_LIB)
Db_DIR  = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)
Osi_DIR = $(EPICS_BASE_LIB)

PROD += caExample

IOC_LIBRARY = exampleLib

exampleLib_SRCS += xxxRecord.c
exampleLib_SRCS += devXxxSoft.c
exampleLib_SRCS += sncExample.c

LIBOBJS += $(EPICS_BASE_BIN)/iocCore
LIBOBJS += $(EPICS_BASE_BIN)/seq

#choose either

#LIBOBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

# or
EXPANDED_DBD = exampleApp.dbd
exampleLib_SRCS += registerRecordDeviceDriver.c
#end choices

DBD = exampleApp.dbd

-include ../baseLIBOBJS

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)
#
include $(TOP)/configure/RULES_BUILD
include $(TOP)/configure/RULES.registerRecordDeviceDriver
#----------------------------------------
#  ADD RULES AFTER THIS LINE
