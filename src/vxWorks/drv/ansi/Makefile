
TOP=../../../..
 
include $(TOP)/configure/CONFIG

INC += drvAt5Vxi.h
INC += drvEpvxi.h
INC += drvHp1404a.h
INC += drvHpe1368a.h
INC += drvKscV215.h
INC += drvMz8310.h
INC += drvStc.h
INC += drvTip810.h
INC += epvxi.h

SRCS.c += ../drvAt5Vxi.c
SRCS.c += ../drvEpvxi.c
SRCS.c += ../drvEpvxiMsg.c
SRCS.c += ../drvHp1404a.c
SRCS.c += ../drvHpe1368a.c
SRCS.c += ../drvHpe1445a.c
SRCS.c += ../drvKscV215.c
SRCS.c += ../drvMz8310.c
SRCS.c += ../drvStc.c
#SRCS.c += ../drvCaenV265.c

#SRCS.c += ../drvVmic2534.c

PROD_vxWorks = $(SRCS.c:../%.c=%.o)

MAN3 = drvEpvxi.3

include $(TOP)/configure/RULES_BUILD

