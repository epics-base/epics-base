#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../../../..

include $(TOP)/configure/CONFIG

resourceLibTest_SRCS += resourceLibTest.cc
TESTPROD_HOST += resourceLibTest

tsDLListBench_SRCS += tsDLListBench.cc
TESTPROD_HOST += tsDLListBench

tsDLListTest_SRCS += tsDLListTest.cc
TESTPROD_HOST += tsDLListTest

tsSLListBench_SRCS += tsSLListBench.cc
TESTPROD_HOST += tsSLListBench

tsSLListTest_SRCS += tsSLListTest.cc
TESTPROD_HOST += tsSLListTest

minmaxTest_SRCS += minmaxTest.cc
TESTPROD_HOST += minmaxTest

PROD_LIBS = Com

include $(TOP)/configure/RULES

