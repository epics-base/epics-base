##########################################################################
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
##########################################################################

TOP=../..

include $(TOP)/configure/CONFIG

PROD_IOC_DEFAULT = softIoc
PROD_IOC_iOS = -nil-

DBD += softIoc.dbd
softIoc_DBD += base.dbd
softIoc_DBD += dlload.dbd
softIoc_DBD += system.dbd

softIoc_SRCS += softIoc_registerRecordDeviceDriver.cpp
softIoc_SRCS_DEFAULT += softMain.cpp
softIoc_SRCS_vxWorks = -nil-

softIoc_LIBS = $(EPICS_BASE_IOC_LIBS)

DB += softIocExit.db

FINAL_LOCATION ?= $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))

include $(TOP)/configure/RULES

softMain$(OBJ) : epicsInstallDir.h

epicsInstallDir.h:
	$(ECHO) "FINAL_LOCATION=$(FINAL_LOCATION)"
	$(PERL) ../makeInstallDir.pl '$(FINAL_LOCATION)' > $@

clean::
	@$(RM) epicsInstallDir.h

