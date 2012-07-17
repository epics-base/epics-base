#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../../..

include $(TOP)/configure/CONFIG

TESTLIBRARY = xRec

xRec_SRCS = xRecord.c

PROD_LIBS = xRec dbCore ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
testHarness_SRCS += callbackTest.c
TESTS += callbackTest

TESTPROD_HOST += dbStateTest
dbStateTest_SRCS += dbStateTest.c
testHarness_SRCS += dbStateTest.c
TESTS += dbStateTest

TARGETS += $(COMMON_DIR)/dbChannelTest.dbd
dbChannelTest_DBD += xRecord.dbd
TESTPROD_HOST += dbChannelTest
dbChannelTest_SRCS += dbChannelTest.c
dbChannelTest_SRCS += dbChannelTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbChannelTest.c
testHarness_SRCS += dbChannelTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/dbChannelTest.dbd ../xRecord.db
TESTS += dbChannelTest

TARGETS += $(COMMON_DIR)/chfPluginTest.dbd
chfPluginTest_DBD += xRecord.dbd
TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += chfPluginTest.c
chfPluginTest_SRCS += chfPluginTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += chfPluginTest.c
testHarness_SRCS += chfPluginTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/chfPluginTest.dbd
TESTS += chfPluginTest

TARGETS += $(COMMON_DIR)/arrShorthandTest.dbd
arrShorthandTest_DBD += xRecord.dbd
TESTPROD_HOST += arrShorthandTest
arrShorthandTest_SRCS += arrShorthandTest.c
arrShorthandTest_SRCS += arrShorthandTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += arrShorthandTest.c
testHarness_SRCS += arrShorthandTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/arrShorthandTest.dbd
TESTS += arrShorthandTest

# The testHarness runs all the test programs in a known working order.
testHarness_SRCS += epicsRunDbTests.c

dbTestHarness_SRCS += $(testHarness_SRCS)
dbTestHarness_SRCS_RTEMS += rtemsTestHarness.c

PROD_vxWorks = dbTestHarness
PROD_RTEMS = dbTestHarness

TESTSPEC_vxWorks = dbTestHarness.munch; epicsRunDbTests
TESTSPEC_RTEMS = dbTestHarness.boot; epicsRunDbTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

xRecord$(DEP): $(COMMON_DIR)/xRecord.h
