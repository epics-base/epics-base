#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
#     Synchrotronstrahlung.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

SHARED_LIBRARIES = NO
#STATIC_BUILD=YES

LIBRARY_HOST += catools

catools_SRCS += tool_lib.c

catools_LIBS += ca Com

#Remove LIBNAME from the library names to be installed (make it a local library)
INSTALL_LIBS= $(addprefix $(INSTALL_LIB)/,$(filterout $(TESTLIBNAME),$(LIBNAME)))

PROD_HOST += caget camonitor cainfo caput

PROD_LIBS += catools ca Com

catools_DIR=.

include $(TOP)/configure/RULES
