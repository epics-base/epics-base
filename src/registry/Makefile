TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += registry.h
#INC += registryDeviceSupport.h moved header to src/include
#INC += registryDriverSupport.h moved header to src/include
#INC += registryRecordType.h moved header to src/include

#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

SCRIPTS_DEFAULT += registerRecordDeviceDriver.pl
SCRIPTS_vxWorks += registerRecordDeviceDriver.pl

registryLibrary_SRCS += registry.c
registryLibrary_SRCS += registryRecordType.c
registryLibrary_SRCS += registryDeviceSupport.c
registryLibrary_SRCS += registryDriverSupport.c

PROD_vxWorks = registerRecordDeviceDriver.o

LIBRARY_vxWorks = registryLibrary

include $(TOP)/configure/RULES_BUILD

