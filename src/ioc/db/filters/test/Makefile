#*************************************************************************
# Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += dbIoc miscIoc registryIoc ca Com

TESTPROD_HOST += tsTest
tsTest_SRCS += tsTest.c
tsTest_LIBS += dbStaticHost
OBJS_IOC_vxWorks += tsTest
TESTS += tsTest

TESTPROD_HOST += dbndTest
dbndTest_SRCS += dbndTest.c
dbndTest_LIBS += dbStaticHost
OBJS_IOC_vxWorks += dbndTest
TESTS += dbndTest

DBDINC += arrRecord
TARGETS += $(COMMON_DIR)/arrTest.dbd
arrTest_DBD += arrRecord.dbd
TESTPROD_HOST += arrTest
arrTest_SRCS += arrTest.cpp arrRecord.c arrRecord_registerRecordDeviceDriver.cpp
arrTest_LIBS += dbStaticIoc
OBJS_IOC_vxWorks += arrTest
TESTS += arrTest

TESTPROD_HOST += syncTest
syncTest_SRCS += syncTest.c
syncTest_LIBS += dbStaticHost
OBJS_IOC_vxWorks += syncTest
TESTS += syncTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

