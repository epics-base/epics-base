TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#_APPNAME_.dbd will be created from _APPNAME_Include.dbd
DBD += _APPNAME_.dbd

#_APPNAME__registerRecordDeviceDriver.cpp will be created from _APPNAME_.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp

#=============================

PROD_IOC = _APPNAME_

## Build and add other local sources like this:
#_APPNAME__SRCS += xxx.c xxx.cpp xxx.stt

## If _APPNAME_ is to be built for both vxWorks and for the host then
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

## The following adds support from base/src/vxWorks
_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

##Define all possible libraries
## Use win32 object libs for registered support
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

## For sequence programs, use
#_APPNAME__SRCS += sncExample.stt
#_APPNAME__LIBS += seq
#_APPNAME__LIBS += pv
#seq_DIR    = $(SNCSEQ_LIB)
#pv_DIR     = $(SNCSEQ_LIB)


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
