TOP=../../..

include $(TOP)/configure/CONFIG

SHARED_LIBRARIES = NO

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

LIBRARY_IOC = testDevIoc

# For R3.13 compatability
OBJS_vxWorks = $(SRCS:%.c=%)

include $(TOP)/configure/RULES
