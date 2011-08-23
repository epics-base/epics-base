TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = _APPNAME_
# _APPNAME_.dbd will be created and installed
DBD += _APPNAME_.dbd

# _APPNAME_.dbd will be made up from these files:
_APPNAME__DBD += base.dbd

# Include dbd files from all support applications:
#_APPNAME__DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#_APPNAME__LIBS += xxx

# _APPNAME__registerRecordDeviceDriver.cpp derives from _APPNAME_.dbd
_APPNAME__SRCS += _APPNAME__registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
_APPNAME__SRCS_DEFAULT += _APPNAME_Main.cpp
_APPNAME__SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#_APPNAME__OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

