TOP=../../..

include $(TOP)/configure/CONFIG

MYSRCS += devAiTestAsyn.c
MYSRCS += devAoTestAsyn.c
MYSRCS += devBiTestAsyn.c
MYSRCS += devBoTestAsyn.c
MYSRCS += devEventTestIoEvent.c
MYSRCS += devHistogramTestAsyn.c
MYSRCS += devMbbiTestAsyn.c
MYSRCS += devMbboTestAsyn.c
MYSRCS += devSiTestAsyn.c
MYSRCS += devSoTestAsyn.c
MYSRCS += devWfTestAsyn.c

PROD_vxWorks = $(MYSRCS:%.c=%.o)

include $(TOP)/configure/RULES_BUILD


