TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

USR_LIBS_hpux += cma

epicsListTest_SRCS += epicsListTest.cpp
PROD += epicsListTest
OBJS_IOC += epicsListTest

epicsAlgorithmTest_SRCS += epicsAlgorithmTest.cpp
PROD += epicsAlgorithmTest
OBJS_IOC += epicsAlgorithmTest

epicsTimeTestHost_SRCS += epicsTimeTestMain.cpp epicsTimeTest.cpp
PROD += epicsTimeTestHost
OBJS_IOC += epicsTimeTest

epicsThreadTestHost_SRCS += epicsThreadTestMain.cpp epicsThreadTest.cpp
PROD += epicsThreadTestHost
OBJS_IOC += epicsThreadTest

epicsThreadPrivateTestHost_SRCS += epicsThreadPrivateTestMain.cpp epicsThreadPrivateTest.cpp
PROD += epicsThreadPrivateTestHost
OBJS_IOC += epicsThreadPrivateTest

timerTestHost_SRCS += timerTestMain.c timerTest.c
PROD += timerTestHost
OBJS_IOC += timerTest

epicsTimerTestHost_SRCS += epicsTimerTestMain.cpp epicsTimerTest.cpp
PROD += epicsTimerTestHost
OBJS_IOC += epicsTimerTest

ringPointerTestHost_SRCS += ringPointerTestMain.c ringPointerTest.c
PROD += ringPointerTestHost
OBJS_IOC += ringPointerTest

epicsEventTestHost_SRCS += epicsEventTestMain.cpp epicsEventTest.cpp
PROD += epicsEventTestHost
OBJS_IOC += epicsEventTest

epicsMutexTestHost_SRCS += epicsMutexTestMain.cpp epicsMutexTest.cpp
PROD += epicsMutexTestHost
OBJS_IOC += epicsMutexTest

#fdmgrTest_SRCS += fdmgrTest.c
#PROD += fdmgrTest

include $(TOP)/configure/RULES

