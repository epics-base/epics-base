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

LIBSRCS += registry.c
LIBSRCS += registryRecordType.c
LIBSRCS += registryDeviceSupport.c
LIBSRCS += registryDriverSupport.c

LIBRARY_IOC = registryIoc
registryIoc_DLL_LIBS  = dbStaticIoc Com

OBJS_IOC = registerRecordDeviceDriver

include $(TOP)/configure/RULES

