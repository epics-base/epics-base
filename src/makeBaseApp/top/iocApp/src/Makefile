TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# build an ioc application

PROD_IOC = _APPNAME_

DBD += _APPNAME_.dbd
# _APPNAME_.dbd will be made up from these files:
_APPNAME__DBD += base.dbd
#include definitions for any other support applications needed
#_APPNAME__DBD += xxx.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#add a definition for each support application used by this application
#_APPNAME__LIBS += xxx

#NOTES:
#      1)It is not possible to build sncExample both as a component of _APPNAME_
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

#The following builds sncExample as a component of _APPNAME_
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq pv

_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

#The following builds sncExample as a standalone application
#PROD_HOST += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#sncExample_LIBS += seq pv
#sncExample_LIBS += $(EPICS_BASE_HOST_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

