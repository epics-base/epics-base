TOP=../../..

include $(TOP)/configure/CONFIG

INC += drvTS.h
INC += devLib.h

SRCS += devLib.c
SRCS += devLibVxWorks.c

OBJS_vxWorks += drvTS

OBJLIB_vxWorks = vxDb
OBJLIB_SRCS = $(SRCS)

include $(TOP)/configure/RULES

