TOP=../..
include $(TOP)/configure/CONFIG

#	includes to install from this sub-project
#

INC += ioccrf.h
INC += iocUtilRegister.h
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
LIBSRCS += iocUtil.c
LIBSRCS += dbStaticRegister
LIBSRCS += dbTestRegister
LIBSRCS += dbBkptRegister
LIBSRCS += dbCaTestRegister
LIBSRCS += caTestRegister
LIBSRCS += dbAccessRegister
LIBSRCS += ioccrfRegister
LIBSRCS += asTestRegister

LIBRARY_IOC = iocsh

iocsh_OBJS += $(EPICS_BASE_BIN)/registerRecordDeviceDriver.o

iocsh_DLL_LIBS  = Com dbStaticIoc dbIoc asIoc rsrvIoc dbtoolsIoc registryIoc miscIoc 
registryIoc_DIR=$(EPICS_BASE_LIB)
dbtoolsIoc_DIR=$(EPICS_BASE_LIB)
miscIoc_DIR=$(EPICS_BASE_LIB)
rsrvIoc_DIR=$(EPICS_BASE_LIB)

include $(TOP)/configure/RULES

