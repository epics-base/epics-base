TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 

PROD += caExample

caExample_LIBS	+= ca
caExample_LIBS	+= Com

ca_DIR           = $(EPICS_BASE_LIB)
Com_DIR          = $(EPICS_BASE_LIB)

#=============================

RECTYPES += xxxRecord.h
DBDEXPAND = exampleInclude.dbd
DBDNAME = exampleApp.dbd

#=============================

LIBRARY_vxWorks = example
PROD_RTEMS = example
PROD += example

example_LIBS += iocsh
example_LIBS += miscIoc
example_LIBS += rsrvIoc
example_LIBS += dbtoolsIoc
example_LIBS += asIoc
example_LIBS += dbIoc
example_LIBS += registryIoc
example_LIBS += dbStaticIoc
example_LIBS += ca
example_LIBS += Com

iocsh_DIR        = $(EPICS_BASE_LIB)
miscIoc_DIR      = $(EPICS_BASE_LIB)
rsrvIoc_DIR      = $(EPICS_BASE_LIB)
dbtoolsIoc_DIR   = $(EPICS_BASE_LIB)
asIoc_DIR        = $(EPICS_BASE_LIB)
dbIoc_DIR        = $(EPICS_BASE_LIB)
registryIoc_DIR  = $(EPICS_BASE_LIB)
dbStaticIoc_DIR  = $(EPICS_BASE_LIB)
ca_DIR           = $(EPICS_BASE_LIB)
Com_DIR          = $(EPICS_BASE_LIB)

example_SRCS += xxxRecord.c
example_SRCS += devXxxSoft.c

example_SRCS_DEFAULT += registerRecordDeviceDriver.c
example_SRCS_DEFAULT += exampleMain.c
example_SRCS_vxWorks += -nil-
#example_SRCS_vxWorks += sncExample.c

example_OBJS_DEFAULT += $(EPICS_BASE_BIN)/registerRecordDeviceDriverRegister
example_OBJS_vxWorks += $(EPICS_BASE_BIN)/registerRecordDeviceDriver
example_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary
#example_OBJS_vxWorks += $(EPICS_BASE_BIN)/seq

-include ../baseLIBOBJS
example_OBJS += $(LIBOBJS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

