
TOP=../../..

include $(TOP)/configure/CONFIG

devLibOpt_SRCS += devAnalytekGpib.c
devLibOpt_SRCS+= devXxDg535Gpib.c
devLibOpt_SRCS+= devGpibInteract.c
devLibOpt_SRCS+= devXxSr620Gpib.c
devLibOpt_SRCS+= devK486Gpib.c
devLibOpt_SRCS+= devXxK196Gpib.c
devLibOpt_SRCS+= devXxDc5009Gpib.c
devLibOpt_SRCS+= devXxK263Gpib.c
devLibOpt_SRCS+= devXxSkeletonGpib.c

IOC_PROD_vxWorks = $(devLibOpt_SRCS:%.c=%.o)

IOC_LIBRARY_vxWorks = devLibOpt

include $(TOP)/configure/RULES_BUILD

