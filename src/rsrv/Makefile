
TOP=../..

include $(TOP)/configure/CONFIG

USR_INCLUDES = -I../../ca

INC += rsrv.h

LIBSRCS += caserverio.c
LIBSRCS += caservertask.c
LIBSRCS += camsgtask.c
LIBSRCS += camessage.c
LIBSRCS += cast_server.c
LIBSRCS += online_notify.c 

LIBRARY_IOC = rsrvIoc
rsrvIoc_DLL_LIBS = asIoc dbIoc ca Com 
SYS_DLL_LIBS := ws2_32

rsrvIoc_RCS_WIN32 = rsrvIoc.rc

# For R3.13 compatibility only
OBJLIB_vxWorks=rsrvIoc
OBJLIB_SRCS = $(LIBSRCS)

include $(TOP)/configure/RULES
