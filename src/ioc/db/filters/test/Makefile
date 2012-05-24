#*************************************************************************
# Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += dbCore ca Com

TESTPROD_HOST += tsTest
tsTest_SRCS += tsTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += tsTest
TESTS += tsTest

TESTPROD_HOST += dbndTest
dbndTest_SRCS += dbndTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += dbndTest
TESTS += dbndTest

TARGETS += $(COMMON_DIR)/arrTest.dbd
arrTest_DBD += arrRecord.dbd
TESTPROD_HOST += arrTest
arrTest_SRCS += arrTest.cpp arrRecord.c arrRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += arrTest
TESTS += arrTest

TESTPROD_HOST += syncTest
syncTest_SRCS += syncTest.c xRecord_registerRecordDeviceDriver.cpp
OBJS_IOC_vxWorks += syncTest
TESTS += syncTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

$(COMMON_DIR)/xRecord.dbd: ../../../test/xRecord.dbd
	$(INSTALL) -d $< $(@D)

tsTest$(OBJ): $(COMMON_DIR)/xRecord.h
dbndTest$(OBJ): $(COMMON_DIR)/xRecord.h
syncTest$(OBJ): $(COMMON_DIR)/xRecord.h
arrTest$(OBJ): $(COMMON_DIR)/arrRecord.h
