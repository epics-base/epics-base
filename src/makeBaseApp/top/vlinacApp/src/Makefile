TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# build an ioc application

PROD_IOC = vlinac
# <name>.dbd will be created from <name>Include.dbd
DBD += vlinac.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
vlinac_SRCS += vlinac_registerRecordDeviceDriver.cpp
vlinac_SRCS_DEFAULT += vlinacMain.cpp
vlinac_SRCS_vxWorks += -nil-

# The following adds support from base/src/vxWorks
vlinac_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# The following builds sequence programs as a component of vlinac
# To build .st files, SNCSEQ must be defined in <top>/configure/RELEASE
# Also in _APPNAME_Include.dbd uncomment #registrar(<name>Registrar) lines
#vlinac_SRCS += beamTrajectory.st
#vlinac_SRCS += autoControl.st
#vlinac_SRCS += stabilizer.st
#vlinac_LIBS += seq pv

vlinac_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

