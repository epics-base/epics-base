TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#USR_CFLAGS +=
PROD_LIBS       += ca
PROD_LIBS       += Db
PROD_LIBS       += Com
PROD_LIBS       += Osi
ca_DIR  = $(EPICS_BASE_LIB)
Db_DIR  = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)
Osi_DIR = $(EPICS_BASE_LIB)

#RECTYPES += xxxRecord.h
#DBDEXPAND = exampleInclude.dbd
#DBDNAME = exampleApp.dbd

#LIBOBJS += 

#choose either

#LIBOBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

# or
#EXPANDED_DBD = exampleApp.dbd
#LIBOBJS += registerRecordDeviceDriver.o
#end choices

include $(TOP)/configure/RULES_BUILD
include $(TOP)/configure/RULES.registerRecordDeviceDriver
#----------------------------------------
#  ADD RULES AFTER THIS LINE

