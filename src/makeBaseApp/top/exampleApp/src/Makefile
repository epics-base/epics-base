TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#USER_DBDFLAGS += 

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

PROD += caExample

LIBOBJS += xxxRecord.o
LIBOBJS += devXxxSoft.o
LIBOBJS += sncExample.o

LIBOBJS += $(EPICS_BASE_BIN)/iocCore
LIBOBJS += $(EPICS_BASE_BIN)/seq

#choose either

#LIBOBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

# or
EXPANDED_DBD = exampleApp.dbd
LIBOBJS += registerRecordDeviceDriver.o
#end choices


DBD = exampleApp.dbd

include ../baseLIBOBJS

LIBOBJS += $(EPICS_BASE_BIN)/iocCore
LIBOBJS += $(EPICS_BASE_BIN)/seq

IOC_LIBRARY = exampleLib

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)
#
include $(TOP)/configure/RULES_BUILD
include $(TOP)/config/RULES.registerRecordDeviceDriver
#----------------------------------------
#  ADD RULES AFTER THIS LINE
