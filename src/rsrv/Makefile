TOP=../..

include $(TOP)/configure/CONFIG

USR_INCLUDES = -I../../ca
USR_CFLAGS += -UUNIX -DiocCore

rsrv_SRCS = \
	caserverio.c caservertask.c camsgtask.c camessage.c \
	rsrv_init.c cast_server.c online_notify.c globalsource.c 

LIBRARY = rsrv

include $(TOP)/configure/RULES
