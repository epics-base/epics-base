
TOP=../..
include $(TOP)/configure/CONFIG

INC +=  epicsRelease.h
INC +=  iocInit.h
INC +=  asSubRecordFunctions.h

LIBSRCS +=  epicsRelease.c
LIBSRCS +=  iocInit.c
LIBSRCS +=  asSubRecordFunctions.c
LIBRARY_IOC = miscIoc

miscIoc_DLL_LIBS  = rsrvIoc asIoc dbIoc registryIoc dbStaticIoc Com
rsrvIoc_DIR=$(EPICS_BASE_LIB)
asIoc_DIR=$(EPICS_BASE_LIB)
registryIoc_DIR=$(EPICS_BASE_LIB)

include $(TOP)/configure/RULES
