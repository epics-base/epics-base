#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

#epicsListTest_SRCS += epicsListTest.cpp
PROD_HOST += epicsListTest
#epicsListTest build fails for vxWorks-ppc604 on win32-x86 host 
#OBJS_IOC_vxWorks += epicsListTest
OBJS_IOC_RTEMS += epicsListTest

epicsAlgorithmTest_SRCS += epicsAlgorithmTest.cpp
PROD_HOST += epicsAlgorithmTest
OBJS_IOC_vxWorks += epicsAlgorithmTest

epicsMathTestHost_SRCS += epicsMathTestMain.cpp epicsMathTest.c
PROD_HOST += epicsMathTestHost
OBJS_IOC_vxWorks += epicsMathTest

epicsStdioTestHost_SRCS += epicsStdioTestMain.cpp epicsStdioTest.c
PROD_HOST += epicsStdioTestHost
OBJS_IOC_vxWorks += epicsStdioTest

epicsStringTestHost_SRCS += epicsStringTestMain.cpp epicsStringTest.c
PROD_HOST += epicsStringTestHost
OBJS_IOC_vxWorks += epicsStringTest

epicsTimeTestHost_SRCS += epicsTimeTestMain.cpp epicsTimeTest.cpp
PROD_HOST += epicsTimeTestHost
OBJS_IOC_vxWorks += epicsTimeTest

epicsThreadTestHost_SRCS += epicsThreadTestMain.cpp epicsThreadTest.cpp
PROD_HOST += epicsThreadTestHost
OBJS_IOC_vxWorks += epicsThreadTest

epicsThreadPrivateTestHost_SRCS += epicsThreadPrivateTestMain.cpp epicsThreadPrivateTest.cpp
PROD_HOST += epicsThreadPrivateTestHost
OBJS_IOC_vxWorks += epicsThreadPrivateTest

epicsMaxThreadsHost_SRCS += epicsMaxThreadsMain.cpp epicsMaxThreads.c
PROD_HOST += epicsMaxThreadsHost
OBJS_IOC_vxWorks += epicsMaxThreads

epicsOkToBlockTestHost_SRCS += epicsOkToBlockTestMain.cpp epicsOkToBlockTest.cpp
PROD_HOST += epicsOkToBlockTestHost
OBJS_IOC_vxWorks += epicsOkToBlockTest

epicsExitTestHost_SRCS += epicsExitTestMain.c epicsExitTest.c
PROD_HOST += epicsExitTestHost
OBJS_IOC_vxWorks += epicsExitTest


epicsTimerTestHost_SRCS += epicsTimerTestMain.cpp epicsTimerTest.cpp
PROD_HOST += epicsTimerTestHost
OBJS_IOC_vxWorks += epicsTimerTest

epicsTimerExampleHost_SRCS += epicsTimerExampleMain.cpp epicsTimerExample.cpp
PROD_HOST += epicsTimerExampleHost
OBJS_IOC_vxWorks += epicsTimerExample

ringPointerTestHost_SRCS += ringPointerTestMain.cpp ringPointerTest.c
PROD_HOST += ringPointerTestHost
OBJS_IOC_vxWorks += ringPointerTest

epicsEventTestHost_SRCS += epicsEventTestMain.cpp epicsEventTest.cpp
PROD_HOST += epicsEventTestHost
OBJS_IOC_vxWorks += epicsEventTest

epicsMutexTestHost_SRCS += epicsMutexTestMain.cpp epicsMutexTest.cpp
PROD_HOST += epicsMutexTestHost
OBJS_IOC_vxWorks += epicsMutexTest

epicsExceptionTestHost_SRCS += epicsExceptionTestMain.cpp epicsExceptionTest.cpp
PROD_HOST += epicsExceptionTestHost
OBJS_IOC_vxWorks += epicsExceptionTest

epicsMessageQueueTestHost_SRCS += epicsMessageQueueTestMain.cpp epicsMessageQueueTest.cpp
PROD_HOST += epicsMessageQueueTestHost
OBJS_IOC_vxWorks += epicsMessageQueueTest

macEnvExpandTestHost_SRCS += macEnvExpandTestMain.cpp macEnvExpandTest.c
PROD_HOST += macEnvExpandTestHost
OBJS_IOC_vxWorks += macEnvExpandTest

blockingSockTestHost_SRCS += blockingSockTestMain.cpp blockingSockTest.cpp
PROD_HOST += blockingSockTestHost
OBJS_IOC_vxWorks += blockingSockTest
# needed when its an object library build
blockingSockTestHost_SYS_LIBS_WIN32 = ws2_32 advapi32 user32
blockingSockTestHost_SYS_LIBS_solaris = socket


#fdmgrTest_SRCS += fdmgrTest.c
#PROD_HOST += fdmgrTest

OBJS_IOC_RTEMS += $(OBJS_IOC_vxWorks)

include $(TOP)/configure/RULES

