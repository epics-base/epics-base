TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += registry.h

#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

SCRIPTS += registerRecordDeviceDriver.pl

registry_SRCS += registry.c
registry_SRCS += registryRecordType.c
registry_SRCS += registryDeviceSupport.c
registry_SRCS += registryDriverSupport.c

LIBRARY = registry

OBJS = registerRecordDeviceDriver

include $(TOP)/configure/RULES

