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

TESTLIBRARY = dbRecStdTest

dbRecStdTest_SRCS += asTestLib.c
dbRecStdTest_LIBS += dbRecStd dbCore ca Com

PROD_LIBS = dbRecStdTest dbRecStd dbCore ca Com

TARGETS += $(COMMON_DIR)/recTestIoc.dbd
DBDDEPENDS_FILES += recTestIoc.dbd$(DEP)
recTestIoc_DBD = base.dbd
TESTFILES += $(COMMON_DIR)/recTestIoc.dbd

testHarness_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += asTestIoc_registerRecordDeviceDriver.cpp

TESTPROD_HOST += arrayOpTest
arrayOpTest_SRCS += arrayOpTest.c
arrayOpTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += arrayOpTest.c
TESTFILES += ../arrayOpTest.db
TESTS += arrayOpTest

TESTPROD_HOST += recMiscTest
recMiscTest_SRCS += recMiscTest.c
recMiscTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += recMiscTest.c
TESTFILES += ../recMiscTest.db
TESTS += recMiscTest

TESTPROD_HOST += linkRetargetLinkTest
linkRetargetLinkTest_SRCS += linkRetargetLinkTest.c
linkRetargetLinkTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += linkRetargetLinkTest.c
TESTFILES += ../linkRetargetLink.db
TESTS += linkRetargetLinkTest

TESTPROD_HOST += linkInitTest
linkInitTest_SRCS += linkInitTest.c
linkInitTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += linkInitTest.c
TESTFILES += ../linkInitTest.db
TESTS += linkInitTest

TESTPROD_HOST += compressTest
compressTest_SRCS += compressTest.c
compressTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += compressTest.c
TESTFILES += ../compressTest.db
TESTS += compressTest

TESTPROD_HOST += asyncSoftTest
asyncSoftTest_SRCS += asyncSoftTest.c
asyncSoftTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += asyncSoftTest.c
TESTFILES += ../asyncSoftTest.db
TESTS += asyncSoftTest

TARGETS += $(COMMON_DIR)/asTestIoc.dbd
DBDDEPENDS_FILES += asTestIoc.dbd$(DEP)
asTestIoc_DBD += base.dbd
asTestIoc_DBD += asTest.dbd
TESTPROD_HOST += asTest
asTest_SRCS += asTest.c
asTest_SRCS += asTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += asTest.c
TESTFILES += $(COMMON_DIR)/asTestIoc.dbd ../asTest.db
TESTS += asTest

TARGETS += $(COMMON_DIR)/analogMonitorTest.dbd
DBDDEPENDS_FILES += analogMonitorTest.dbd$(DEP)
analogMonitorTest_DBD += base.dbd
TESTPROD_HOST += analogMonitorTest
analogMonitorTest_SRCS += analogMonitorTest.c
analogMonitorTest_SRCS += analogMonitorTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += analogMonitorTest.c
testHarness_SRCS += analogMonitorTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/analogMonitorTest.dbd ../analogMonitorTest.db
TESTS += analogMonitorTest

TARGETS += $(COMMON_DIR)/regressTest.dbd
regressTest_DBD += base.dbd
TESTPROD_HOST += regressTest
regressTest_SRCS += regressTest.c
regressTest_SRCS += regressTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/regressTest.dbd ../regressArray1.db
TESTS += regressTest

# epicsRunRecordTests runs all the test programs in a known working order.
testHarness_SRCS += epicsRunRecordTests.c

recordTestHarness_SRCS += $(testHarness_SRCS)
recordTestHarness_SRCS_RTEMS += rtemsTestHarness.c

PROD_vxWorks = recordTestHarness
PROD_RTEMS = recordTestHarness

TESTSPEC_vxWorks = recordTestHarness.munch; epicsRunRecordTests
TESTSPEC_RTEMS = recordTestHarness.boot; epicsRunRecordTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
