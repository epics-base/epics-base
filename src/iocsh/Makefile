TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += ioccrf.h
INC += osiRegister.h
INC += iocUtilRegister.h
INC += dbStaticRegister.h
INC += dbTestRegister.h
INC += dbBkptRegister.h
INC += dbCaTestRegister.h
INC += caTestRegister.h
INC += dbAccessRegister.h
INC += ioccrfRegisterCommon.h
INC += asTestRegister.h
INC += envRegister.h
INC += registerRecordDeviceDriverRegister.h


#       general contents of library on all platforms
#
#       system specific lines in RULES.Host figure out
#       1) what object names (.o, .obj,...) to build
#	2) where to find the sources: ., ./os/<ARCH_CLASS>, ./os/generic, ...
#

LIBSRCS += ioccrf.cpp
LIBSRCS += epicsReadline.c
LIBSRCS += osiRegister.c
LIBSRCS += dbStaticRegister.c
LIBSRCS += dbTestRegister.c
LIBSRCS += dbBkptRegister.c
LIBSRCS += dbCaTestRegister.c
LIBSRCS += caTestRegister.c
LIBSRCS += dbAccessRegister.c
LIBSRCS += ioccrfRegisterCommon.c
LIBSRCS += asTestRegister.c
LIBSRCS += envRegister.c
LIBSRCS += iocUtil.c

LIBRARY_IOC = iocsh

OBJS += registerRecordDeviceDriverRegister

iocsh_DLL_LIBS  = miscIoc rsrvIoc dbtoolsIoc asIoc dbIoc registryIoc dbStaticIoc Com
registryIoc_DIR=$(EPICS_BASE_LIB)
dbtoolsIoc_DIR=$(EPICS_BASE_LIB)
miscIoc_DIR=$(EPICS_BASE_LIB)
rsrvIoc_DIR=$(EPICS_BASE_LIB)

iocsh_RCS_WIN32 = iocsh.rc

include $(TOP)/configure/RULES

