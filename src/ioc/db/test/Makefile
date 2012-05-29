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

PROD_vxWorks += dbTestHarness
PROD_RTEMS += dbTestHarness

PROD_LIBS = dbCore ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
dbTestHarness_SRCS += callbackTest.c
TESTS += callbackTest


dbTestHarness_SRCS += epicsRunDbTests.c
dbTestHarness_SRCS_RTEMS = rtemsTestHarness.c

TESTSPEC_vxWorks = dbTestHarness.munch; epicsRunDbTests
TESTSPEC_RTEMS = dbTestHarness.boot; epicsRunDbTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

