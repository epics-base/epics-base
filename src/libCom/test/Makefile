TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

USR_LIBS_hpux += cma

#epicsListTest_SRCS += epicsListTest.cpp
#PROD_HOST += epicsListTest
#OBJS_IOC += epicsListTest

epicsAlgorithmTest_SRCS += epicsAlgorithmTest.cpp
PROD_HOST += epicsAlgorithmTest
OBJS_IOC += epicsAlgorithmTest

epicsTimeTestHost_SRCS += epicsTimeTestMain.cpp epicsTimeTest.cpp
PROD_HOST += epicsTimeTestHost
OBJS_IOC += epicsTimeTest

epicsThreadTestHost_SRCS += epicsThreadTestMain.cpp epicsThreadTest.cpp
PROD_HOST += epicsThreadTestHost
OBJS_IOC += epicsThreadTest

epicsThreadPrivateTestHost_SRCS += epicsThreadPrivateTestMain.cpp epicsThreadPrivateTest.cpp
PROD_HOST += epicsThreadPrivateTestHost
OBJS_IOC += epicsThreadPrivateTest

epicsTimerTestHost_SRCS += epicsTimerTestMain.cpp epicsTimerTest.cpp
PROD_HOST += epicsTimerTestHost
OBJS_IOC += epicsTimerTest

ringPointerTestHost_SRCS += ringPointerTestMain.c ringPointerTest.c
PROD_HOST += ringPointerTestHost
OBJS_IOC += ringPointerTest

epicsEventTestHost_SRCS += epicsEventTestMain.cpp epicsEventTest.cpp
PROD_HOST += epicsEventTestHost
OBJS_IOC += epicsEventTest

epicsMutexTestHost_SRCS += epicsMutexTestMain.cpp epicsMutexTest.cpp
PROD_HOST += epicsMutexTestHost
OBJS_IOC += epicsMutexTest

#fdmgrTest_SRCS += fdmgrTest.c
#PROD_HOST += fdmgrTest

include $(TOP)/configure/RULES

