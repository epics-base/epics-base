TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

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

osiTimeTestHost_SRCS += osiTimeTestMain.c osiTimeTest.cc
PROD += osiTimeTestHost
OBJS_IOC += osiTimeTest

SYS_PROD_LIBS_solaris += pthread thread posix4

include $(TOP)/configure/RULES

