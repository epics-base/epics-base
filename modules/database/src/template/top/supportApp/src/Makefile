TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += _APPNAME_

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install _APPNAME_.dbd into <top>/dbd
DBD += _APPNAME_.dbd

# specify all source files to be compiled and added to the library
#_APPNAME__SRCS += xxx

_APPNAME__LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

