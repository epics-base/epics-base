TOP=../..

include $(TOP)/configure/CONFIG

USR_INCLUDES = -I../../ca
USR_CFLAGS += -UUNIX -DiocCore

rsrv_SRCS = \
	caserverio.c caservertask.c camsgtask.c camessage.c \
	cast_server.c online_notify.c 
LIBRARY = rsrv
DLL_LIBS = Db As Com 
SYS_DLL_LIBS := ws2_32

include $(TOP)/configure/RULES
