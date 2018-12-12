#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../../..
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

TESTPROD_HOST += softTest
softTest_SRCS += softTest.c
softTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += softTest.c
TESTFILES += ../softTest.db
TESTS += softTest

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

TARGETS += $(COMMON_DIR)/scanEventTest.dbd
DBDDEPENDS_FILES += scanEventTest.dbd$(DEP)
scanEventTest_DBD += base.dbd
TESTPROD_HOST += scanEventTest
scanEventTest_SRCS += scanEventTest.c
scanEventTest_SRCS += scanEventTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += scanEventTest.c
testHarness_SRCS += scanEventTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/scanEventTest.dbd ../scanEventTest.db
TESTS += scanEventTest

TARGETS += $(COMMON_DIR)/regressTest.dbd
DBDDEPENDS_FILES += regressTest.dbd$(DEP)
regressTest_DBD += base.dbd
TESTPROD_HOST += regressTest
regressTest_SRCS += regressTest.c
regressTest_SRCS += regressTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/regressTest.dbd ../regressArray1.db ../regressHex.db ../regressLinkMS.db
TESTFILES += ../badCaLink.db
TESTS += regressTest

TARGETS += $(COMMON_DIR)/simmTest.dbd
TARGETS += $(COMMON_DIR)/simmTest.db
DBDDEPENDS_FILES += simmTest.dbd$(DEP)
DBDDEPENDS_FILES += simmTest.db$(DEP)
simmTest_DBD += base.dbd
TESTPROD_HOST += simmTest
simmTest_SRCS += simmTest.c
simmTest_SRCS += simmTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += simmTest.c
testHarness_SRCS += simmTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/simmTest.dbd $(COMMON_DIR)/simmTest.db
TESTS += simmTest

TESTPROD_HOST += mbbioDirectTest
mbbioDirectTest_SRCS += mbbioDirectTest.c
mbbioDirectTest_SRCS += recTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += mbbioDirectTest.c
TESTFILES += ../mbbioDirectTest.db
TESTS += mbbioDirectTest

TARGETS += $(COMMON_DIR)/asyncproctest.dbd
DBDDEPENDS_FILES += asyncproctest.dbd$(DEP)
asyncproctest_DBD += base.dbd
TESTPROD_HOST += asyncproctest
asyncproctest_SRCS += asyncproctest.c
asyncproctest_SRCS += asyncproctest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/asyncproctest.dbd ../asyncproctest.db
TESTS += asyncproctest

ifeq ($(T_A),$(EPICS_HOST_ARCH))
# Host-only tests of softIoc/softIocPVA, caget and pvget (if present)
TESTS += netget
endif
# epicsRunRecordTests runs all the test programs in a known working order.
testHarness_SRCS += epicsRunRecordTests.c

recordTestHarness_SRCS += $(testHarness_SRCS)
recordTestHarness_SRCS_RTEMS += rtemsTestHarness.c

PROD_SRCS_RTEMS += rtemsTestData.c

PROD_vxWorks = recordTestHarness
PROD_RTEMS = recordTestHarness

TESTSPEC_vxWorks = recordTestHarness.munch; epicsRunRecordTests
TESTSPEC_RTEMS = recordTestHarness.boot; epicsRunRecordTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)
ifneq ($(filter $(T_A),$(CROSS_COMPILER_RUNTEST_ARCHS)),)
TESTPROD_RTEMS = $(TESTPROD_HOST)
TESTSCRIPTS_RTEMS += $(TESTS:%=%.t)
endif

include $(TOP)/configure/RULES

rtemsTestData.c : $(TESTFILES) $(TOOLS)/epicsMakeMemFs.pl
	$(PERL) $(TOOLS)/epicsMakeMemFs.pl $@ epicsRtemsFSImage $(TESTFILES)
