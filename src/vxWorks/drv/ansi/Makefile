TOP=../../../..
 
include $(TOP)/configure/CONFIG

INC += drvAt5Vxi.h
INC += drvHp1404a.h
INC += drvHpe1368a.h
INC += drvKscV215.h
INC += drvMz8310.h
INC += drvStc.h

SRCS += drvAt5Vxi.c
SRCS += drvHp1404a.c
SRCS += drvHpe1368a.c
SRCS += drvHpe1445a.c
SRCS += drvKscV215.c
SRCS += drvMz8310.c
SRCS += drvStc.c
#SRCS += drvCaenV265.c

#SRCS += drvVmic2534.c

OBJS_vxWorks += $(SRCS:%.c=%)

include $(TOP)/configure/RULES

