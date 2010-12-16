#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

callbackTest_LIBS = dbIoc
callbackTest_LIBS_DEFAULT = dbStaticHost
callbackTest_LIBS_vxWorks = dbStaticIoc
callbackTest_LIBS_RTEMS   = dbStaticIoc
PROD_LIBS = ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
PROD_vxWorks += callbackTest
TESTS += callbackTest

# When we add more test programs here, this must become a vxTestHarness
TESTSPEC_vxWorks = callbackTest.munch; callbackTest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

