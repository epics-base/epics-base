##########################################################################
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
##########################################################################

TOP=../..

include $(TOP)/configure/CONFIG

PROD_IOC = softIoc

DBD += softIoc.dbd
softIoc_DBD += base.dbd
softIoc_DBD += dlload.dbd
softIoc_DBD += system.dbd

ABS_INSTALL_DIR = $(shell $(PERL) $(TOOLS)/fullPathName.pl -e $(INSTALL_LOCATION))
USR_CXXFLAGS += -DEPICS_BASE=$(ABS_INSTALL_DIR)

softIoc_SRCS += softIoc_registerRecordDeviceDriver.cpp
softIoc_SRCS_DEFAULT += softMain.cpp
softIoc_SRCS_vxWorks = -nil-

softIoc_LIBS = $(EPICS_BASE_IOC_LIBS)

DB += softIocExit.db

include $(TOP)/configure/RULES
