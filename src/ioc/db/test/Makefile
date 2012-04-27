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
dbChannelTest_SRCS += dbChannelTest.c
OBJS_IOC_vxWorks += dbChannelTest
TESTS += dbChannelTest

TESTPROD_HOST += chfPluginTest
chfPluginTest_SRCS += chfPluginTest.c
OBJS_IOC_vxWorks += chfPluginTest
TESTS += chfPluginTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

