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

example_SRCS += xxxRecord.c
example_SRCS += devXxxSoft.c
example_SRCS += caMonitor.c

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
example_SRCS_DEFAULT += example_registerRecordDeviceDriver.cpp
example_SRCS_DEFAULT += exampleMain.c
example_SRCS_vxWorks += -nil-

example_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary

#The following adds drvTS support for vxWorks
#example_OBJS_vxWorks += $(EPICS_BASE_BIN)/drvTS

#NOTES:
#      1)It is not possible to build sncExample both as a component of example
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

#The following builds sncExample as a component of example
#example_SRCS += sncExample.stt
#example_OBJS_vxWorks += $(SNCSEQ_BIN)/seqLibrary
#example_OBJS_vxWorks += $(SNCSEQ_BIN)/pvLibrary
#seq_DIR    = $(SNCSEQ_LIB)
#example_LIBS += seq
#example_LIBS += pv

#The following builds sncExample as a standalone application
#PROD += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#seq_DIR    = $(SNCSEQ_LIB)
#sncExample_LIBS += seq
#sncExample_LIBS += pv
#sncExample_LIBS += iocsh
#sncExample_LIBS += miscIoc
#sncExample_LIBS += rsrvIoc
#sncExample_LIBS += dbtoolsIoc
#sncExample_LIBS += asIoc
#sncExample_LIBS += dbIoc
#sncExample_LIBS += registryIoc
#sncExample_LIBS += dbStaticIoc
#sncExample_LIBS += ca
#sncExample_LIBS += Com

-include ../baseLIBOBJS
example_OBJS += $(LIBOBJS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

