#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../../..

include $(TOP)/configure/CONFIG

PROD_vxWorks += dbTestHarness
PROD_RTEMS += dbTestHarness

PROD_LIBS = dbCore ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
dbTestHarness_SRCS += callbackTest.c
TESTS += callbackTest


dbTestHarness_SRCS += epicsRunDbTests.c
dbTestHarness_SRCS_RTEMS = rtemsTestHarness.c

TESTSPEC_vxWorks = dbTestHarness.munch; epicsRunDbTests
TESTSPEC_RTEMS = dbTestHarness.boot; epicsRunDbTests

TESTPROD_HOST += dbChannelTest
dbChannelTest_SRCS += dbChannelTest.c xRecord.c
dbTestHarness_SRCS += dbChannelTest.c
OBJS_IOC_vxWorks += dbChannelTest
TESTS += dbChannelTest

TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += chfPluginTest.c xRecord.c
dbTestHarness_SRCS += chfPluginTest.c
OBJS_IOC_vxWorks += chfPluginTest
TESTS += chfPluginTest

TESTPROD_HOST += arrShorthandTest
arrShorthandTest_SRCS += arrShorthandTest.c xRecord.c
dbTestHarness_SRCS += arrShorthandTest.c
OBJS_IOC_vxWorks += arrShorthandTest
TESTS += arrShorthandTest

TESTPROD_HOST += dbStateTest
dbStateTest_SRCS += dbStateTest.c
dbTestHarness_SRCS += dbStateTest.c
OBJS_IOC_vxWorks += dbStateTest
TESTS += dbStateTest

dbTestHarness_SRCS += xRecord.c

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
