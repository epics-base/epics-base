
TOP=../..

include $(TOP)/configure/CONFIG

USR_INCLUDES = -I../../ca

LIBSRCS += caserverio.c
LIBSRCS += caservertask.c
LIBSRCS += camsgtask.c
LIBSRCS += camessage.c
LIBSRCS += cast_server.c
LIBSRCS += online_notify.c 

LIBRARY_IOC = rsrvIoc
rsrvIoc_DLL_LIBS = asIoc dbIoc ca Com 
SYS_DLL_LIBS := ws2_32

include $(TOP)/configure/RULES
