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

TESTLIBRARY = dbTestIoc

dbTestIoc_SRCS += arrRecord.c
dbTestIoc_SRCS += xRecord.c
dbTestIoc_SRCS += dbLinkdset.c
dbTestIoc_LIBS = dbCore ca Com

TARGETS += $(COMMON_DIR)/dbTestIoc.dbd
DBDDEPENDS_FILES += dbTestIoc.dbd$(DEP)
dbTestIoc_DBD += menuGlobal.dbd
dbTestIoc_DBD += menuConvert.dbd
dbTestIoc_DBD += menuScan.dbd
#dbTestIoc_DBD += arrRecord.dbd
dbTestIoc_DBD += xRecord.dbd
dbTestIoc_DBD += dbLinkdset.dbd
TESTFILES += $(COMMON_DIR)/dbTestIoc.dbd ../xRecord.db

testHarness_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp

PROD_LIBS = dbTestIoc dbCore ca Com

TESTPROD_HOST += dbShutdownTest
dbShutdownTest_SRCS += dbShutdownTest.c
dbShutdownTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbShutdownTest.c
TESTS += dbShutdownTest

TESTPROD_HOST += dbPutLinkTest
dbPutLinkTest_SRCS += dbPutLinkTest.c
dbPutLinkTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbPutLinkTest.c
TESTS += dbPutLinkTest
TESTFILES += ../dbPutLinkTest.db

TESTPROD_HOST += dbLockTest
dbLockTest_SRCS += dbLockTest.c
dbLockTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbLockTest.c
TESTS += dbLockTest
TESTFILES += ../dbLockTest.db

TESTPROD_HOST += testdbConvert
testdbConvert_SRCS += testdbConvert.c
testHarness_SRCS += testdbConvert.c
TESTS += testdbConvert

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
testHarness_SRCS += callbackTest.c
TESTS += callbackTest

TESTPROD_HOST += callbackParallelTest
callbackParallelTest_SRCS += callbackParallelTest.c
testHarness_SRCS += callbackParallelTest.c
TESTS += callbackParallelTest

TESTPROD_HOST += dbStateTest
dbStateTest_SRCS += dbStateTest.c
testHarness_SRCS += dbStateTest.c
TESTS += dbStateTest

TESTPROD_HOST += dbCaStatsTest
dbCaStatsTest_SRCS += dbCaStatsTest.c
dbCaStatsTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbCaStatsTest.c
TESTS += dbCaStatsTest
TESTFILES += ../dbCaStatsTest.db

TARGETS += $(COMMON_DIR)/scanIoTest.dbd
DBDDEPENDS_FILES += scanIoTest.dbd$(DEP)
scanIoTest_DBD += menuGlobal.dbd
scanIoTest_DBD += menuConvert.dbd
scanIoTest_DBD += menuScan.dbd
scanIoTest_DBD += yRecord.dbd
TESTPROD_HOST += scanIoTest
scanIoTest_SRCS += scanIoTest.c
scanIoTest_REGRDDFLAGS = -l
scanIoTest_SRCS += scanIoTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += scanIoTest.c
testHarness_SRCS += scanIoTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/scanIoTest.dbd ../scanIoTest.db
TESTS += scanIoTest

TESTPROD_HOST += dbChannelTest
dbChannelTest_SRCS += dbChannelTest.c
dbChannelTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbChannelTest.c
TESTS += dbChannelTest

TARGETS += $(COMMON_DIR)/dbChArrTest.dbd
DBDDEPENDS_FILES += dbChArrTest.dbd$(DEP)
dbChArrTest_DBD += arrRecord.dbd
TESTPROD_HOST += dbChArrTest
dbChArrTest_SRCS += dbChArrTest.cpp
dbChArrTest_SRCS += dbChArrTest_registerRecordDeviceDriver.cpp
testHarness_SRCS += dbChArrTest.cpp
testHarness_SRCS += dbChArrTest_registerRecordDeviceDriver.cpp
TESTFILES += $(COMMON_DIR)/dbChArrTest.dbd ../dbChArrTest.db
TESTS += dbChArrTest

TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += chfPluginTest.c
chfPluginTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += chfPluginTest.c
TESTS += chfPluginTest

TESTPROD_HOST += arrShorthandTest
arrShorthandTest_SRCS += arrShorthandTest.c
arrShorthandTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += arrShorthandTest.c
TESTS += arrShorthandTest

TESTPROD_HOST += benchdbConvert
benchdbConvert_SRCS += benchdbConvert.c

TESTPROD_HOST += recGblCheckDeadbandTest
recGblCheckDeadbandTest_SRCS += recGblCheckDeadbandTest.c
recGblCheckDeadbandTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += recGblCheckDeadbandTest.c
TESTS += recGblCheckDeadbandTest

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

arrRecord$(DEP): $(COMMON_DIR)/arrRecord.h
xRecord$(DEP): $(COMMON_DIR)/xRecord.h
dbPutLinkTest$(DEP): $(COMMON_DIR)/xRecord.h
scanIoTest$(DEP): $(COMMON_DIR)/yRecord.h
