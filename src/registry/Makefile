TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += registry.h
INC += registryDeviceSupport.h
INC += registryDriverSupport.h
INC += registryRecordType.h

#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

SCRIPTS += registerRecordDeviceDriver.pl

registryLibrary_SRCS += registry.c
registryLibrary_SRCS += registryRecordType.c
registryLibrary_SRCS += registryDeviceSupport.c
registryLibrary_SRCS += registryDriverSupport.c

IOC_PROD_vxWorks = registerRecordDeviceDriver.o

IOC_LIBRARY_vxWorks = registryLibrary

include $(TOP)/configure/RULES_BUILD
