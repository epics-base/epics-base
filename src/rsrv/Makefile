
TOP=../..

include $(TOP)/configure/CONFIG

USR_INCLUDES = -I../../ca
USR_CFLAGS = -UUNIX

rsrvLib_SRCS = \
	caserverio.c caservertask.c camsgtask.c camessage.c \
	rsrv_init.c cast_server.c online_notify.c globalsource.c 

IOC_LIBRARY_vxWorks = rsrvLib

include $(TOP)/configure/RULES_BUILD

