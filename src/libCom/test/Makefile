#*************************************************************************
# Copyright (c) 2006 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com

TESTPROD_HOST += epicsUnitTestTest
epicsUnitTestTest_SRCS += epicsUnitTestTest.c
# Not much point running this on vxWorks or RTEMS...
TESTS += epicsUnitTestTest

TESTPROD_HOST += epicsCalcTest
epicsCalcTest_SRCS += epicsCalcTest.cpp
testHarness_SRCS += epicsCalcTest.cpp
TESTS += epicsCalcTest

TESTPROD_HOST += epicsAlgorithmTest
epicsAlgorithmTest_SRCS += epicsAlgorithmTest.cpp
testHarness_SRCS += epicsAlgorithmTest.cpp
TESTS += epicsAlgorithmTest

TESTPROD_HOST += epicsMathTest
epicsMathTest_SRCS += epicsMathTest.c
testHarness_SRCS += epicsMathTest.c
TESTS += epicsMathTest

TESTPROD_HOST += epicsEllTest
epicsEllTest_SRCS += epicsEllTest.c
testHarness_SRCS += epicsEllTest.c
TESTS += epicsEllTest

TESTPROD_HOST += epicsEnvTest
epicsEnvTest_SRCS += epicsEnvTest.c
testHarness_SRCS += epicsEnvTest.c
TESTS += epicsEnvTest

TESTPROD_HOST += epicsErrlogTest
epicsErrlogTest_SRCS += epicsErrlogTest.c
testHarness_SRCS += epicsErrlogTest.c
TESTS += epicsErrlogTest

TESTPROD_HOST += epicsStdioTest
epicsStdioTest_SRCS += epicsStdioTest.c
testHarness_SRCS += epicsStdioTest.c
TESTS += epicsStdioTest

TESTPROD_HOST += epicsSockResolveTest
epicsSockResolveTest_SRCS += epicsSockResolveTest.c
testHarness_SRCS += epicsSockResolveTest.c
epicsSockResolveTest_SYS_LIBS_WIN32 = ws2_32
TESTS += epicsSockResolveTest

TESTPROD_HOST += epicsStringTest
epicsStringTest_SRCS += epicsStringTest.c
testHarness_SRCS += epicsStringTest.c
TESTS += epicsStringTest

TESTPROD_HOST += epicsTimeTest
epicsTimeTest_SRCS += epicsTimeTest.cpp
testHarness_SRCS += epicsTimeTest.cpp
TESTS += epicsTimeTest

TESTPROD_HOST += epicsThreadTest
epicsThreadTest_SRCS += epicsThreadTest.cpp
testHarness_SRCS += epicsThreadTest.cpp
TESTS += epicsThreadTest

TESTPROD_HOST += epicsThreadOnceTest
epicsThreadOnceTest_SRCS += epicsThreadOnceTest.c
testHarness_SRCS += epicsThreadOnceTest.c
TESTS += epicsThreadOnceTest

TESTPROD_HOST += epicsThreadPriorityTest
epicsThreadPriorityTest_SRCS += epicsThreadPriorityTest.cpp
testHarness_SRCS += epicsThreadPriorityTest.cpp
TESTS += epicsThreadPriorityTest

TESTPROD_HOST += epicsThreadPrivateTest
epicsThreadPrivateTest_SRCS += epicsThreadPrivateTest.cpp
testHarness_SRCS += epicsThreadPrivateTest.cpp
TESTS += epicsThreadPrivateTest

TESTPROD_HOST += epicsExitTest
epicsExitTest_SRCS += epicsExitTest.c
testHarness_SRCS += epicsExitTest.c
TESTS += epicsExitTest

TESTPROD_HOST += epicsTimerTest
epicsTimerTest_SRCS += epicsTimerTest.cpp
testHarness_SRCS += epicsTimerTest.cpp
TESTS += epicsTimerTest

TESTPROD_HOST += ringPointerTest
ringPointerTest_SRCS += ringPointerTest.c
testHarness_SRCS += ringPointerTest.c
TESTS += ringPointerTest

TESTPROD_HOST += ringBytesTest
ringBytesTest_SRCS += ringBytesTest.c
testHarness_SRCS += ringBytesTest.c
TESTS += ringBytesTest

TESTPROD_HOST += epicsEventTest
epicsEventTest_SRCS += epicsEventTest.cpp
testHarness_SRCS += epicsEventTest.cpp
TESTS += epicsEventTest

TESTPROD_HOST += epicsMutexTest
epicsMutexTest_SRCS += epicsMutexTest.cpp
testHarness_SRCS += epicsMutexTest.cpp
TESTS += epicsMutexTest

TESTPROD_HOST += epicsExceptionTest
epicsExceptionTest_SRCS += epicsExceptionTest.cpp
testHarness_SRCS += epicsExceptionTest.cpp
TESTS += epicsExceptionTest

TESTPROD_HOST += macEnvExpandTest
macEnvExpandTest_SRCS += macEnvExpandTest.c
testHarness_SRCS += macEnvExpandTest.c
TESTS += macEnvExpandTest

TESTPROD_HOST += macLibTest
macLibTest_SRCS += macLibTest.c
testHarness_SRCS += macLibTest.c
TESTS += macLibTest

TESTPROD_HOST += taskwdTest
taskwdTest_SRCS += taskwdTest.c
testHarness_SRCS += taskwdTest.c
TESTS += taskwdTest

TESTPROD_HOST += blockingSockTest
blockingSockTest_SRCS += blockingSockTest.cpp
testHarness_SRCS += blockingSockTest.cpp
# needed when its an object library build
blockingSockTest_SYS_LIBS_WIN32 = ws2_32 advapi32 user32
blockingSockTest_SYS_LIBS_solaris = socket
TESTS += blockingSockTest

TESTPROD_HOST += epicsMessageQueueTest
epicsMessageQueueTest_SRCS += epicsMessageQueueTest.cpp
testHarness_SRCS += epicsMessageQueueTest.cpp
TESTS += epicsMessageQueueTest


# The testHarness runs all the test programs in a known working order.
testHarness_SRCS += epicsRunLibComTests.c

PROD_vxWorks = vxTestHarness
vxTestHarness_SRCS += $(testHarness_SRCS)
vxTestHarness_OBJS += $(INSTALL_BIN)/vxComLibrary
TESTSPEC_vxWorks = vxTestHarness.munch; epicsRunLibComTests

PROD_RTEMS += rtemsTestHarness
rtemsTestHarness_SRCS += rtemsTestHarness.c
rtemsTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_RTEMS = rtemsTestHarness.boot; epicsRunLibComTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)


# The following are not test programs, they measure performance.
# They should not be added to TESTS or to epicsRunLibComTests.c

TESTPROD_HOST += epicsThreadPerform
epicsThreadPerform_SRCS += epicsThreadPerform.cpp
testHarness_SRCS += epicsThreadPerform.cpp

TESTPROD_HOST += epicsMaxThreads
epicsMaxThreads_SRCS += epicsMaxThreads.c
testHarness_SRCS += epicsMaxThreads.c

TESTPROD_HOST += buckTest
buckTest_SRCS += buckTest.c
testHarness_SRCS += buckTest.c

#TESTPROD_HOST += fdmgrTest
fdmgrTest_SRCS += fdmgrTest.c
fdmgrTest_LIBS += ca
# FIXME: program never exits.

TESTPROD_HOST += cvtFastPerform
cvtFastPerform_SRCS += cvtFastPerform.cpp
testHarness_SRCS += cvtFastPerform.cpp

include $(TOP)/configure/RULES

