
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


SRCS.c += ../module_types.c
SRCS.c += ../drvBb902.c
SRCS.c += ../drvBb910.c
SRCS.c += ../drvComet.c
SRCS.c += ../drvCompuSm.c
SRCS.c += ../drvDvx.c
SRCS.c += ../drvFp.c
SRCS.c += ../drvFpm.c
SRCS.c += ../drvGpib.c
SRCS.c += ../drvJgvtr1.c
SRCS.c += ../drvOms.c
SRCS.c += ../drvVmi4100.c
SRCS.c += ../drvXy010.c
SRCS.c += ../drvXy210.c
SRCS.c += ../drvXy220.c
SRCS.c += ../drvXy240.c
SRCS.c += ../drvXy566.c

IOC_PROD_vxWorks = $(SRCS.c:../%.c=%.o)

include $(TOP)/configure/RULES_BUILD

