TOP=../../../..
 
include $(TOP)/configure/CONFIG
 
CMPLR = TRAD

VX_WARN_YES = -Wall -pedantic
#	Only the 68k cross-compiler has no "nobitfield" option   -kuk-
ifeq ($(ARCH_CLASS),68k)
USR_CFLAGS = -fshared-data -fvolatile -mnobitfield 
else
USR_CFLAGS = -fshared-data -fvolatile
endif

#Following only to make Gpib stuff build
INC += drvBitBusErr.h
INC += drvBitBusInterface.h

INC += drvGpib.h
INC += drvGpibErr.h
INC += drvGpibInterface.h
INC += drvHiDEOSGpib.h
INC += drvJgvtr1.h
INC += drvOms.h
INC += steppermotor.h


SRCS += module_types.c
SRCS += drvBb902.c
SRCS += drvBb910.c
SRCS += drvComet.c
SRCS += drvCompuSm.c
SRCS += drvDvx.c
SRCS += drvFp.c
SRCS += drvFpm.c
SRCS += drvGpib.c
SRCS += drvJgvtr1.c
SRCS += drvOms.c
SRCS += drvVmi4100.c
SRCS += drvXy010.c
SRCS += drvXy210.c
SRCS += drvXy220.c
SRCS += drvXy240.c
SRCS += drvXy566.c

OBJS_vxWorks += $(SRCS:%.c=%)

include $(TOP)/configure/RULES

