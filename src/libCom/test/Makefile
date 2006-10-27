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

epicsUnitTestTest_SRCS += epicsUnitTestTest.c
PROD_HOST += epicsUnitTestTest
TESTS += epicsUnitTestTest

epicsCalcTest_SRCS += epicsCalcTest.cpp
PROD_HOST += epicsCalcTest
OBJS_IOC_vxWorks += epicsCalcTest
TESTS += epicsCalcTest

epicsAlgorithmTest_SRCS += epicsAlgorithmTest.cpp
PROD_HOST += epicsAlgorithmTest
OBJS_IOC_vxWorks += epicsAlgorithmTest
TESTS += epicsAlgorithmTest

epicsMathTestHost_SRCS += epicsMathTestMain.cpp epicsMathTest.c
PROD_HOST += epicsMathTestHost
OBJS_IOC_vxWorks += epicsMathTest
TESTS += epicsMathTestHost

epicsStdioTestHost_SRCS += epicsStdioTestMain.cpp epicsStdioTest.c
PROD_HOST += epicsStdioTestHost
OBJS_IOC_vxWorks += epicsStdioTest
TESTS += epicsStdioTestHost

epicsStringTestHost_SRCS += epicsStringTestMain.cpp epicsStringTest.c
PROD_HOST += epicsStringTestHost
OBJS_IOC_vxWorks += epicsStringTest
TESTS += epicsStringTestHost

epicsTimeTestHost_SRCS += epicsTimeTestMain.cpp epicsTimeTest.cpp
PROD_HOST += epicsTimeTestHost
OBJS_IOC_vxWorks += epicsTimeTest
TESTS += epicsTimeTestHost

epicsThreadTestHost_SRCS += epicsThreadTestMain.cpp epicsThreadTest.cpp
PROD_HOST += epicsThreadTestHost
OBJS_IOC_vxWorks += epicsThreadTest

epicsThreadPriorityTestHost_SRCS += epicsThreadPriorityTestMain.cpp epicsThreadPriorityTest.cpp
PROD_HOST += epicsThreadPriorityTestHost
OBJS_IOC_vxWorks += epicsThreadPriorityTest

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
TESTS += epicsExceptionTestHost

epicsMessageQueueTestHost_SRCS += epicsMessageQueueTestMain.cpp epicsMessageQueueTest.cpp
PROD_HOST += epicsMessageQueueTestHost
OBJS_IOC_vxWorks += epicsMessageQueueTest

macEnvExpandTestHost_SRCS += macEnvExpandTestMain.cpp macEnvExpandTest.c
PROD_HOST += macEnvExpandTestHost
OBJS_IOC_vxWorks += macEnvExpandTest
TESTS += macEnvExpandTestHost

blockingSockTestHost_SRCS += blockingSockTestMain.cpp blockingSockTest.cpp
PROD_HOST += blockingSockTestHost
OBJS_IOC_vxWorks += blockingSockTest
# needed when its an object library build
blockingSockTestHost_SYS_LIBS_WIN32 = ws2_32 advapi32 user32
blockingSockTestHost_SYS_LIBS_solaris = socket


#buckTest_SRCS += buckTest.c
#PROD_HOST += buckTest

#fdmgrTest_SRCS += fdmgrTest.c
#PROD_HOST += fdmgrTest

PROD_RTEMS += rtemsTestHarness
rtemsTestHarness_SRCS += rtemsTestHarness.c
rtemsTestHarness_SRCS += epicsRunLibComTests.c
rtemsTestHarness_SRCS += blockingSockTest.cpp
rtemsTestHarness_SRCS += epicsAlgorithmTest.cpp
rtemsTestHarness_SRCS += epicsCalcTest.cpp
rtemsTestHarness_SRCS += epicsEventTest.cpp
rtemsTestHarness_SRCS += epicsExceptionTest.cpp
rtemsTestHarness_SRCS += epicsExitTest.cpp
rtemsTestHarness_SRCS += epicsMathTest.c
rtemsTestHarness_SRCS += epicsMessageQueueTest.cpp
rtemsTestHarness_SRCS += epicsMutexTest.cpp
rtemsTestHarness_SRCS += epicsOkToBlockTest.cpp
rtemsTestHarness_SRCS += epicsStdioTest.c
rtemsTestHarness_SRCS += epicsStringTest.c
rtemsTestHarness_SRCS += epicsThreadPriorityTest.cpp
rtemsTestHarness_SRCS += epicsThreadPrivateTest.cpp
rtemsTestHarness_SRCS += epicsThreadTest.cpp
rtemsTestHarness_SRCS += epicsTimeTest.cpp
rtemsTestHarness_SRCS += epicsTimerTest.cpp
rtemsTestHarness_SRCS += macEnvExpandTest.c
rtemsTestHarness_SRCS += ringPointerTest.c


TEST_SCRIPTS += $(TESTS:%=%.t)


include $(TOP)/configure/RULES

test: $(TEST_SCRIPTS)
	@perl -e 'use Test::Harness; runtests @ARGV;' $(TEST_SCRIPTS)

# If there's a perl test script (.plt) available, use it
%.t: ../%.plt
	@$(RM) $@
	@$(CP) $< $@

# Early versions of Test::Harness expect test programs in perl only.
# Generate a 1-line test program to run the real test binary
%.t: %$(EXE)
	@$(RM) $@
	@echo 'exec "$<";' >$@
