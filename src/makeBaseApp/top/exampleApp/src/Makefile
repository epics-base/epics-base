TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += _APPNAME_.dbd

#=============================

PROD_IOC = _APPNAME_

_APPNAME__SRCS += xxxRecord.c
_APPNAME__SRCS += devXxxSoft.c
_APPNAME__SRCS += dbSubExample.c

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#NOTES:
#      1)It is not possible to build sncExample both as a component of _APPNAME_
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

#The following builds sncExample as a component of _APPNAME_
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq
#_APPNAME__LIBS += pv
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
_APPNAME__LIBS_win32 += recIocObj
_APPNAME__LIBS_win32 += softDevIocObj
_APPNAME__LIBS_win32 += testDevIocObj
_APPNAME__LIBS_DEFAULT += recIoc
_APPNAME__LIBS_DEFAULT += softDevIoc
_APPNAME__LIBS_DEFAULT += testDevIoc

_APPNAME__LIBS += iocsh
_APPNAME__LIBS += miscIoc
_APPNAME__LIBS += rsrvIoc
_APPNAME__LIBS += dbtoolsIoc
_APPNAME__LIBS += asIoc
_APPNAME__LIBS += dbIoc
_APPNAME__LIBS += registryIoc
_APPNAME__LIBS += dbStaticIoc
_APPNAME__LIBS += ca
_APPNAME__LIBS += Com

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

