TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

USR_LIBS_hpux += cma

timerTestHost_SRCS += timerTestMain.c timerTest.c
PROD += timerTestHost
OBJS_IOC += timerTest

semBinaryTestHost_SRCS += semBinaryTestMain.c semBinaryTest.c
PROD += semBinaryTestHost
OBJS_IOC += semBinaryTest

semMutexTestHost_SRCS += semMutexTestMain.c semMutexTest.c
PROD += semMutexTestHost
OBJS_IOC += semMutexTest

threadTestHost_SRCS += threadTestMain.c threadTest.c
PROD += threadTestHost
OBJS_IOC += threadTest

#fdmgrTest_SRCS += fdmgrTest.c
#PROD += fdmgrTest

osiTimeTestHost_SRCS += osiTimeTestMain.c osiTimeTest.cpp
PROD += osiTimeTestHost
OBJS_IOC += osiTimeTest

include $(TOP)/configure/RULES

