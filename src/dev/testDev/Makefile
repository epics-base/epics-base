TOP=../../..

include $(TOP)/configure/CONFIG

SRCS += devAiTestAsyn.c
SRCS += devAoTestAsyn.c
SRCS += devBiTestAsyn.c
SRCS += devBoTestAsyn.c
SRCS += devEventTestIoEvent.c
SRCS += devHistogramTestAsyn.c
SRCS += devMbbiTestAsyn.c
SRCS += devMbboTestAsyn.c
SRCS += devSiTestAsyn.c
SRCS += devSoTestAsyn.c
SRCS += devWfTestAsyn.c

IOC_PROD_vxWorks = $(SRCS:%.c=%.o)

include $(TOP)/configure/RULES_BUILD


