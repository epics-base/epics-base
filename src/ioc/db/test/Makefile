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

PROD_vxWorks += dbTestHarness chfPluginTest
PROD_RTEMS += dbTestHarness chfPluginTest

PROD_LIBS = dbCore ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
dbTestHarness_SRCS += callbackTest.c
TESTS += callbackTest

TESTPROD_HOST += dbStateTest
dbStateTest_SRCS += dbStateTest.c
dbTestHarness_SRCS += dbStateTest.c
TESTS += dbStateTest

TESTPROD_HOST += dbChannelTest
dbChannelTest_SRCS += xRecord.c
dbChannelTest_SRCS += dbChannelTest.c
dbChannelTest_SRCS += dbChannelTest_registerRecordDeviceDriver.cpp
dbTestHarness_SRCS += xRecord.c
dbTestHarness_SRCS += dbChannelTest.c
dbTestHarness_SRCS += dbChannelTest_registerRecordDeviceDriver.cpp
TESTS += dbChannelTest

TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += xRecord.c
chfPluginTest_SRCS += chfPluginTest.c
chfPluginTest_SRCS += chfPluginTest_registerRecordDeviceDriver.cpp
dbTestHarness_SRCS += chfPluginTest.c
dbTestHarness_SRCS += chfPluginTest_registerRecordDeviceDriver.cpp
TESTS += chfPluginTest

TESTPROD_HOST += arrShorthandTest
arrShorthandTest_SRCS += xRecord.c
arrShorthandTest_SRCS += arrShorthandTest.c
arrShorthandTest_SRCS += arrShorthandTest_registerRecordDeviceDriver.cpp
dbTestHarness_SRCS += arrShorthandTest.c
dbTestHarness_SRCS += arrShorthandTest_registerRecordDeviceDriver.cpp
TESTS += arrShorthandTest


dbTestHarness_SRCS += epicsRunDbTests.c
dbTestHarness_SRCS_RTEMS = rtemsTestHarness.c

TESTSPEC_vxWorks = dbTestHarness.munch; epicsRunDbTests
TESTSPEC_RTEMS = dbTestHarness.boot; epicsRunDbTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

# xRecord$(OBJ): $(COMMON_DIR)/xRecord.h
xRecord$(DEP): $(COMMON_DIR)/xRecord.h
