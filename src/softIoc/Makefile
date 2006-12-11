##########################################################################
# Copyright (c) 2003 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2003 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7 and higher are distributed subject to the
# Software License Agreement found in the file LICENSE that is included
# with this distribution.
##########################################################################

TOP=../..

include $(TOP)/configure/CONFIG

PROD_IOC = softIoc

DBD += softIoc.dbd
softIoc_DBD += base.dbd

ABS_INSTALL_DIR = $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))
USR_CXXFLAGS += -DEPICS_BASE=$(ABS_INSTALL_DIR)

softIoc_SRCS += softIoc_registerRecordDeviceDriver.cpp
softIoc_SRCS += softMain.cpp

softIoc_LIBS = $(EPICS_BASE_IOC_LIBS)

DB += softIocExit.db

include $(TOP)/configure/RULES
