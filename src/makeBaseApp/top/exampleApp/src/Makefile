TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 

PROD_HOST += caExample
caExample_SRCS	+= caExample.c
caExample_LIBS	+= ca
caExample_LIBS	+= Com

PROD_HOST += caMonitor
caMonitor_SRCS	+= caMonitor.c
caMonitor_LIBS	+= ca
caMonitor_LIBS	+= Com

ca_DIR           = $(EPICS_BASE_LIB)
Com_DIR          = $(EPICS_BASE_LIB)


#=============================

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += example.dbd

#=============================

PROD_IOC = example

example_SRCS += xxxRecord.c
example_SRCS += devXxxSoft.c
example_SRCS += dbSubExample.c

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
example_SRCS += example_registerRecordDeviceDriver.cpp
example_SRCS_DEFAULT += exampleMain.cpp
example_SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
example_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#NOTES:
#      1)It is not possible to build sncExample both as a component of example
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

#The following builds sncExample as a component of example
#example_SRCS += sncExample.stt
#example_LIBS += seq
#example_LIBS += pv
#seq_DIR    = $(SNCSEQ_LIB)
#pv_DIR     = $(SNCSEQ_LIB)

#The following builds sncExample as a standalone application
#PROD_HOST += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#seq_DIR    = $(SNCSEQ_LIB)
#pv_DIR     = $(SNCSEQ_LIB)
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

# Use win32 object libs for registered support
example_LIBS_win32 += recIocObj
example_LIBS_win32 += softDevIocObj
example_LIBS_win32 += testDevIocObj
example_LIBS_DEFAULT += recIoc
example_LIBS_DEFAULT += softDevIoc
example_LIBS_DEFAULT += testDevIoc

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

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

