TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC support library

LIBRARY_IOC += _APPNAME_Support

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord
# Install devXxxSoft.dbd into <top>/dbd
DBD += xxxSupport.dbd

# Compile and add the code to the support library
_APPNAME_Support_SRCS += xxxRecord.c
_APPNAME_Support_SRCS += devXxxSoft.c

# Link locally-provided code into the support library,
# rather than directly into the IOC application.
_APPNAME_Support_SRCS += dbSubExample.c
_APPNAME_Support_SRCS += _APPNAME_Hello.c
_APPNAME_Support_SRCS += initTrace.c

_APPNAME_Support_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# Build the IOC application

PROD_IOC = _APPNAME_
# _APPNAME_.dbd will be created and installed
DBD += _APPNAME_.dbd

# _APPNAME_.dbd will be made up from these files:
_APPNAME__DBD += base.dbd
_APPNAME__DBD += xxxSupport.dbd
_APPNAME__DBD += dbSubExample.dbd
_APPNAME__DBD += _APPNAME_Hello.dbd
_APPNAME__DBD += initTrace.dbd

# _APPNAME__registerRecordDeviceDriver.cpp derives from _APPNAME_.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Link in the code from the support library
_APPNAME__LIBS += _APPNAME_Support

# NOTE: To build SNL programs, SNCSEQ must be defined
# in the <top>/configure/RELEASE file

ifneq ($(SNCSEQ),)
    # Build sncExample into _APPNAME_Support
    sncExample_SNCFLAGS += +r
    _APPNAME__DBD += sncExample.dbd
    _APPNAME_Support_SRCS += sncExample.stt
    _APPNAME_Support_LIBS += seq pv
    _APPNAME__LIBS += seq pv

    # Build sncProgram as a standalone program
    PROD_HOST += sncProgram
    sncProgram_SNCFLAGS += +m
    sncProgram_SRCS += sncProgram.st
    sncProgram_LIBS += seq pv
    sncProgram_LIBS += $(EPICS_BASE_HOST_LIBS)
endif

# Finally link to the EPICS Base libraries
_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

