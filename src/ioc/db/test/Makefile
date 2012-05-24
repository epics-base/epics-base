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

PROD_LIBS = dbCore ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
PROD_vxWorks += callbackTest
TESTS += callbackTest

# When we add more test programs here, this must become a vxTestHarness
TESTSPEC_vxWorks = callbackTest.munch; callbackTest

TESTPROD_HOST += dbChannelTest
dbChannelTest_SRCS += dbChannelTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += dbChannelTest
TESTS += dbChannelTest

TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += chfPluginTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += chfPluginTest
TESTS += chfPluginTest

TESTPROD_HOST += arrShorthandTest
arrShorthandTest_SRCS += arrShorthandTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += arrShorthandTest
TESTS += arrShorthandTest

TESTPROD_HOST += dbStateTest
dbStateTest_SRCS += dbStateTest.c
OBJS_IOC_vxWorks += dbStateTest
TESTS += dbStateTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

dbChannelTest$(OBJ): $(COMMON_DIR)/xRecord.h
chfPluginTest$(OBJ): $(COMMON_DIR)/xRecord.h
arrShorthandTest$(OBJ): $(COMMON_DIR)/xRecord.h
dbStateTest$(OBJ): $(COMMON_DIR)/xRecord.h
