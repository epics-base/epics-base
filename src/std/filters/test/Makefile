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

TESTLIBRARY = Recs

Recs_SRCS += xRecord.c
Recs_SRCS += arrRecord.c
Recs_LIBS += dbCore Com

PROD_LIBS = Recs dbRecStd dbCore ca Com

TARGETS += $(COMMON_DIR)/tsTest.dbd
DBDDEPENDS_FILES += tsTest.dbd$(DEP)
tsTest_DBD += xRecord.dbd
TESTPROD_HOST += tsTest
tsTest_SRCS += tsTest.c
tsTest_SRCS += tsTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += tsTest.c
testHarness_SRCS += tsTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/tsTest.dbd ../xRecord.db
TESTS += tsTest

TARGETS += $(COMMON_DIR)/dbndTest.dbd
DBDDEPENDS_FILES += dbndTest.dbd$(DEP)
dbndTest_DBD += xRecord.dbd
TESTPROD_HOST += dbndTest
dbndTest_SRCS += dbndTest.c
dbndTest_SRCS += dbndTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbndTest.c
testHarness_SRCS += dbndTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/dbndTest.dbd
TESTS += dbndTest

TARGETS += $(COMMON_DIR)/arrTest.dbd
DBDDEPENDS_FILES += arrTest.dbd$(DEP)
arrTest_DBD += arrRecord.dbd
TESTPROD_HOST += arrTest
arrTest_SRCS += arrTest.cpp
arrTest_SRCS += arrTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += arrTest.cpp
testHarness_SRCS += arrTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/arrTest.dbd ../arrTest.db
TESTS += arrTest

TARGETS += $(COMMON_DIR)/syncTest.dbd
DBDDEPENDS_FILES += syncTest.dbd$(DEP)
syncTest_DBD += xRecord.dbd
TESTPROD_HOST += syncTest
syncTest_SRCS += syncTest.c
syncTest_SRCS += syncTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += syncTest.c
testHarness_SRCS += syncTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/syncTest.dbd
TESTS += syncTest

# epicsRunFilterTests runs all the test programs in a known working order.
testHarness_SRCS += epicsRunFilterTests.c

filterTestHarness_SRCS += $(testHarness_SRCS)
filterTestHarness_SRCS_RTEMS += rtemsTestHarness.c

PROD_vxWorks = filterTestHarness
PROD_RTEMS = filterTestHarness

TESTSPEC_vxWorks = filterTestHarness.munch; epicsRunFilterTests
TESTSPEC_RTEMS = filterTestHarness.boot; epicsRunFilterTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

xRecord$(DEP): $(COMMON_DIR)/xRecord.h
tsTest$(DEP): $(COMMON_DIR)/xRecord.h
dbndTest$(DEP): $(COMMON_DIR)/xRecord.h
syncTest$(DEP): $(COMMON_DIR)/xRecord.h
arrRecord$(DEP): $(COMMON_DIR)/arrRecord.h
arrTest$(DEP): $(COMMON_DIR)/arrRecord.h
