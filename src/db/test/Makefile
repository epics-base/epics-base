#*************************************************************************
# Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += dbIoc dbStaticHost ca Com

TESTPROD_HOST += callbackTest
callbackTest_SRCS += callbackTest.c
OBJS_IOC_vxWorks += callbackTest
TESTS += callbackTest


TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

