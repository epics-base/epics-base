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

LIBRARY_IOC += _APPNAME_Ioc

_APPNAME_Ioc_SRCS += xxxRecord.c
_APPNAME_Ioc_SRCS += devXxxSoft.c

_APPNAME_Ioc_LIBS += $(EPICS_BASE_IOC_LIBRARIES)
_APPNAME_Ioc_SYS_LIBS_solaris += c

#=============================

PROD_IOC = _APPNAME_

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp
_APPNAME__SRCS += dbSubExample.c
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

_APPNAME__LIBS += _APPNAME_Ioc

#NOTES:
#      1)It is not possible to build sncExample both as a component of _APPNAME_
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

#The following builds sncExample as a component of _APPNAME_
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq
#_APPNAME__LIBS += pv

_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBRARIES)

#The following builds sncExample as a standalone application
#PROD_HOST += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#sncExample_LIBS += seq
#sncExample_LIBS += pv
#sncExample_LIBS += $(EPICS_BASE_IOC_LIBRARIES)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

