TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += ioccrf.h
INC += dbStaticRegister.h
INC += dbTestRegister.h
INC += dbBkptRegister.h
INC += dbCaTestRegister.h
INC += caTestRegister.h
INC += dbAccessRegister.h
INC += ioccrfRegister.h
INC += asTestRegister.h


#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

LIBSRCS += ioccrf.c
LIBSRCS += dbStaticRegister
LIBSRCS += dbTestRegister
LIBSRCS += dbBkptRegister
LIBSRCS += dbCaTestRegister
LIBSRCS += caTestRegister
LIBSRCS += dbAccessRegister
LIBSRCS += ioccrfRegister
LIBSRCS += asTestRegister

LIBRARY_IOC = iocsh
iocsh_DLL_LIBS  = miscIoc dbIoc registryIoc Com

include $(TOP)/configure/RULES
