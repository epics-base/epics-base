TOP=../../..
include $(TOP)/configure/CONFIG

INC += camacLib.h
INC += fast_lock.h
INC += drvTS.h
INC += devLib.h
INC += epicsDynLink.h

SRCS += drvTS.c
SRCS += devLib.c
SRCS += devLibVxWorks.c
SRCS += veclist.c
SRCS += epicsDynLink.c

OBJLIB_vxWorks += vxCom
OBJLIB_SRCS = $(SRCS)

include $(TOP)/configure/RULES
