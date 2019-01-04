#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************
TOP=../../../../..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -DUSE_TYPED_RSET

TESTLIBRARY = Recs

Recs_SRCS += ioRecord.c
Recs_LIBS += dbCore ca Com

PROD_LIBS = Recs dbRecStd dbCore ca Com

DBDDEPENDS_FILES += linkTest.dbd$(DEP)
TARGETS += $(COMMON_DIR)/linkTest.dbd
linkTest_DBD += menuGlobal.dbd
linkTest_DBD += menuConvert.dbd
linkTest_DBD += menuScan.dbd
linkTest_DBD += links.dbd
linkTest_DBD += ioRecord.dbd
TESTFILES += $(COMMON_DIR)/linkTest.dbd
TESTFILES += ../ioRecord.db

testHarness_SRCS += linkTest_registerRecordDeviceDriver.cpp

TESTPROD_HOST += lnkStateTest
lnkStateTest_SRCS += lnkStateTest.c
lnkStateTest_SRCS += linkTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += lnkStateTest.c
TESTS += lnkStateTest

TESTPROD_HOST += lnkCalcTest
lnkCalcTest_SRCS += lnkCalcTest.c
lnkCalcTest_SRCS += linkTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += lnkCalcTest.c
TESTS += lnkCalcTest

# epicsRunLinkTests runs all the test programs in a known working order.
testHarness_SRCS += epicsRunLinkTests.c

linkTestHarness_SRCS += $(testHarness_SRCS)
linkTestHarness_SRCS_RTEMS += rtemsTestHarness.c

PROD_vxWorks = linkTestHarness
PROD_RTEMS = linkTestHarness

TESTSPEC_vxWorks = linkTestHarness.munch; epicsRunLinkTests
TESTSPEC_RTEMS = linkTestHarness.boot; epicsRunLinkTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

ioRecord$(DEP): $(COMMON_DIR)/ioRecord.h
lnkStateTest$(DEP): $(COMMON_DIR)/ioRecord.h
lnkCalcTest$(DEP): $(COMMON_DIR)/ioRecord.h
