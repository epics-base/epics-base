
TOP=../..
include $(TOP)/configure/CONFIG

INC +=  epicsRelease.h
INC +=  iocInit.h
INC +=  asSubRecordFunctions.h

LIBSRCS +=  epicsRelease.c
LIBSRCS +=  iocInit.c
LIBSRCS +=  asSubRecordFunctions.c
DLL_LIBS = Com dbStaticIoc dbIoc asIoc rsrvIoc registryIoc  
LIBRARY_IOC = miscIoc

miscIoc_DLL_LIBS  = rsrvIoc asIoc dbIoc registryIoc dbStaticIoc Com

misc_RCS_WIN32 = misc.rc

# For R3.13 compatibility only
OBJLIB_vxWorks=miscIoc
OBJLIB_SRCS = $(LIBSRCS)

include $(TOP)/configure/RULES

