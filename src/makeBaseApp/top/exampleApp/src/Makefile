TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

RECTYPES += xxxRecord.h
DBDEXPAND = exampleInclude.dbd
DBDNAME = exampleApp.dbd

#USR_CFLAGS += 
PROD_LIBS	+= ca
PROD_LIBS	+= Db
PROD_LIBS	+= Com
ca_DIR  = $(EPICS_BASE_LIB)
Db_DIR  = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)

PROD_DEFAULT += caExample
PROD_vxWorks += -nil-

LIBRARY_vxWorks = exampleLib

exampleLib_SRCS += xxxRecord.c
exampleLib_SRCS += devXxxSoft.c
exampleLib_SRCS += sncExample.c

exampleLib_SRCS += registerRecordDeviceDriver.c

LIBOBJS += $(EPICS_BASE_BIN)/iocCore
LIBOBJS += $(EPICS_BASE_BIN)/seq

-include ../baseLIBOBJS

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)
#
include $(TOP)/configure/RULES_BUILD
#----------------------------------------
#  ADD RULES AFTER THIS LINE
