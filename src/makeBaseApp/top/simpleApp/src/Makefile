TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================


#_APPNAME_.dbd will be created from _APPNAME_Include.dbd
DBD += _APPNAME_.dbd

#=============================

#The following creates a library
#LIBRARY_IOC += _APPNAME_Ioc
#_APPNAME_Ioc_LIBS += $(EPICS_BASE_IOC_LIBS)
#_APPNAME_Ioc_SRCS += xxx.c xxx.cpp xxx.stt

#=============================

#The following creates an application
PROD_IOC = _APPNAME_
#_APPNAME__SRCS += yyy.c yyy.cpp yyy.stt

#_APPNAME__registerRecordDeviceDriver.cpp will be created from _APPNAME_.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp

## If _APPNAME_ is to be built for both vxWorks and for the host then
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

## For sequence programs, use
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq pv

## The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

## The following adds libraries built here
#_APPNAME__LIBS += _APPNAME_Ioc

## The following adds other module libraries
#_APPNAME__LIBS +=  xxx

## The following adds EPICS base libraries
_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
