TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += xxxSupport

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord
# install devXxxSoft.dbd into <top>/dbd
DBD += xxxSupport.dbd

# The following are compiled and added to the Support library
xxxSupport_SRCS += xxxRecord.c
xxxSupport_SRCS += devXxxSoft.c

xxxSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

PROD_IOC = _APPNAME_
# <name>.dbd will be created from <name>Include.dbd
DBD += _APPNAME_.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

# Add locally compiled object code
_APPNAME__SRCS += dbSubExample.c

# The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

_APPNAME__LIBS += xxxSupport

# NOTES:
#      1)It is not possible to build sncExample both as a component of _APPNAME_
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

# The following builds sncExample as a component of _APPNAME_
# Also in _APPNAME_Include.dbd uncomment #registrar(sncExampleRegistrar)
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq pv

_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

# The following builds sncExample as a standalone application
#PROD_HOST += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#sncExample_LIBS += seq pv
#sncExample_LIBS += $(EPICS_BASE_HOST_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

