TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Use typed rset structure (see 3.16.1 release notes)
USR_CPPFLAGS += -DUSE_TYPED_RSET

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord

# Install xxxSupport.dbd into <top>/dbd
DBD += xxxSupport.dbd

# Build an IOC support library
LIBRARY_IOC += _APPNAME_Support

# Compile and add code to the support library
_APPNAME_Support_SRCS += xxxRecord.c
_APPNAME_Support_SRCS += devXxxSoft.c

# Link locally-provided code into the support library,
# rather than directly into the IOC application, that
# causes problems on Windows DLL builds
_APPNAME_Support_SRCS += dbSubExample.c
_APPNAME_Support_SRCS += dev_APPNAME_Version.c
_APPNAME_Support_SRCS += _APPNAME_Hello.c
_APPNAME_Support_SRCS += initTrace.c

_APPNAME_Support_LIBS += $(EPICS_BASE_IOC_LIBS)

# Auto-generate a header file containing a version string.
# Version comes from the VCS if available, else date+time.
GENVERSION = _APPNAME_Version.h
# Macro name
GENVERSIONMACRO = _APPNAME_VERSION

# Build the IOC application
PROD_IOC = _APPNAME_

# _APPNAME_.dbd will be created and installed
DBD += _APPNAME_.dbd

# _APPNAME_.dbd will include these files:
_APPNAME__DBD += base.dbd
_APPNAME__DBD += xxxSupport.dbd
_APPNAME__DBD += dbSubExample.dbd
_APPNAME__DBD += dev_APPNAME_Version.dbd
_APPNAME__DBD += _APPNAME_Hello.dbd
_APPNAME__DBD += initTrace.dbd

# _APPNAME__registerRecordDeviceDriver.cpp derives from _APPNAME_.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp

# Build the main IOC entry point where needed
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

# Link in the code from our support library
_APPNAME__LIBS += _APPNAME_Support

# To build SNL programs, SNCSEQ must be defined
# in the <top>/configure/RELEASE file
ifneq ($(SNCSEQ),)
    # Build sncExample into _APPNAME_Support
    sncExample_SNCFLAGS += +r
    _APPNAME__DBD += sncExample.dbd
    # A .stt sequence program is *not* pre-processed:
    _APPNAME_Support_SRCS += sncExample.stt
    _APPNAME_Support_LIBS += seq pv
    _APPNAME__LIBS += seq pv

    # Build sncProgram as a standalone program
    PROD_HOST += sncProgram
    sncProgram_SNCFLAGS += +m
    # A .st sequence program *is* pre-processed:
    sncProgram_SRCS += sncProgram.st
    sncProgram_LIBS += seq pv
    sncProgram_LIBS += $(EPICS_BASE_HOST_LIBS)
endif

# Finally link IOC to the EPICS Base libraries
_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

# Explicit dependency needed for generated header file
dev_APPNAME_Version$(DEP): $(COMMON_DIR)/$(GENVERSION)
