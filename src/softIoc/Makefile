##########################################################################
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
##########################################################################

TOP=../..

include $(TOP)/configure/CONFIG

PERL_SCRIPTS += makeInstallDir.pl

PROD_IOC = softIoc

INC += epicsInstallDir.h

DBD += softIoc.dbd
softIoc_DBD += base.dbd
softIoc_DBD += dlload.dbd
softIoc_DBD += system.dbd

softIoc_SRCS += softIoc_registerRecordDeviceDriver.cpp
softIoc_SRCS_DEFAULT += softMain.cpp
softIoc_SRCS_vxWorks = -nil-

softIoc_LIBS = $(EPICS_BASE_IOC_LIBS)

DB += softIocExit.db

include $(TOP)/configure/RULES

softMain$(OBJ) : $(COMMON_DIR)/epicsInstallDir.h

$(COMMON_DIR)/epicsInstallDir.h: $(INSTALL_BIN)/makeInstallDir.pl
	$(PERL) $< $(INSTALL_LOCATION) > $@
