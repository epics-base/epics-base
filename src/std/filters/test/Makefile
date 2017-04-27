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

DBDDEPENDS_FILES += filterTest.dbd$(DEP)
TARGETS += $(COMMON_DIR)/filterTest.dbd
filterTest_DBD += menuGlobal.dbd
filterTest_DBD += menuConvert.dbd
filterTest_DBD += menuScan.dbd
filterTest_DBD += filters.dbd
filterTest_DBD += xRecord.dbd
filterTest_DBD += arrRecord.dbd
TESTFILES += $(COMMON_DIR)/filterTest.dbd

testHarness_SRCS += filterTest_registerRecordDeviceDriver.cpp

TESTPROD_HOST += tsTest
tsTest_SRCS += tsTest.c
tsTest_SRCS += filterTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += tsTest.c
TESTFILES += ../xRecord.db
TESTS += tsTest

TESTPROD_HOST += dbndTest
dbndTest_SRCS += dbndTest.c
dbndTest_SRCS += filterTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbndTest.c
TESTS += dbndTest

TESTPROD_HOST += arrTest
arrTest_SRCS += arrTest.cpp
arrTest_SRCS += filterTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += arrTest.cpp
TESTFILES += ../arrTest.db
TESTS += arrTest

TESTPROD_HOST += syncTest
syncTest_SRCS += syncTest.c
syncTest_SRCS += filterTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += syncTest.c
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
