TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

semBinaryTestHost_SRCS += semBinaryTestMain.c semBinaryTest.c
PROD += semBinaryTestHost
OBJS_IOC += semBinaryTest.o

semMutexTestHost_SRCS += semMutexTestMain.c semMutexTest.c
PROD += semMutexTestHost
OBJS_IOC += semMutexTest.o

threadTestHost_SRCS += threadTestMain.c threadTest.c
PROD += threadTestHost
OBJS_IOC += threadTest.o

#fdmgrTest_SRCS += fdmgrTest.c
#PROD += fdmgrTest

#osiTimeTest_SRCS += osiTimeTest.cc
#PROD += osiTimeTest

include $(TOP)/configure/RULES

