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

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += example.dbd

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
example_SRCS += caMonitor.c

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
example_SRCS_DEFAULT += example_registerRecordDeviceDriver.cpp
example_SRCS_DEFAULT += exampleMain.c
example_SRCS_vxWorks += -nil-

example_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary

#example_SRCS_vxWorks += sncExample.c
#example_OBJS_vxWorks += $(SEQ_BIN)/seqLibrary
#example_LIBS += seq
#example_LIBS += pv
#seq_DIR    = $(SEQ_LIB)
#example_LIBS += pvCa



-include ../baseLIBOBJS
example_OBJS += $(LIBOBJS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

