#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../../../..

include $(TOP)/configure/CONFIG

resourceLibTest_SRCS += resourceLibTest.cc
PROD_HOST += resourceLibTest
OBJS_IOC += resourceLibTest

tsDLListBench_SRCS += tsDLListBench.cc
PROD_HOST += tsDLListBench
OBJS_IOC += tsDLListBench

tsDLListTest_SRCS += tsDLListTest.cc
PROD_HOST += tsDLListTest
OBJS_IOC += tsDLListTest

tsSLListBench_SRCS += tsSLListBench.cc
PROD_HOST += tsSLListBench
OBJS_IOC += tsSLListBench

tsSLListTest_SRCS += tsSLListTest.cc
PROD_HOST += tsSLListTest
OBJS_IOC += tsSLListTest

minmaxTest_SRCS += minmaxTest.cc
PROD_HOST += minmaxTest
OBJS_IOC += minmaxTest

tsBTreeTest_SRCS += tsBTreeTest.cc
PROD_HOST += tsBTreeTest
OBJS_IOC += tsBTreeTest

tsBTreeBench_SRCS += tsBTreeBench.cc
PROD_HOST += tsBTreeBench
OBJS_IOC += tsBTreeBench

PROD_LIBS = Com

include $(TOP)/configure/RULES

