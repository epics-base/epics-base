TOP=../../..

include $(TOP)/configure/CONFIG

SRCS += devAnalytekGpib.c
SRCS+= devXxDg535Gpib.c
SRCS+= devGpibInteract.c
SRCS+= devXxSr620Gpib.c
SRCS+= devK486Gpib.c
SRCS+= devXxK196Gpib.c
SRCS+= devXxDc5009Gpib.c
SRCS+= devXxK263Gpib.c
SRCS+= devXxSkeletonGpib.c

OBJS_vxWorks += $(SRCS:%.c=%)

LIBRARY_vxWorks += devLibOpt

include $(TOP)/configure/RULES

