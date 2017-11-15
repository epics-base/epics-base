#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG

# Submodules for bundle build
SUBMODULES += libcom

SUBMODULES += ca
ca_DEPEND_DIRS = libcom

SUBMODULES += database
database_DEPEND_DIRS = ca

SUBMODULES += pvData
pvData_DEPEND_DIRS = libcom

SUBMODULES += pvAccess
pvAccess_DEPEND_DIRS = pvData database

SUBMODULES += normativeTypes
normativeTypes_DEPEND_DIRS = pvData

SUBMODULES += pvaClient
pvaClient_DEPEND_DIRS = pvAccess normativeTypes

SUBMODULES += pvDatabase
pvDatabase_DEPEND_DIRS = pvAccess

SUBMODULES += pva2pva
pva2pva_DEPEND_DIRS = pvAccess

SUBMODULES += example
example_DEPEND_DIRS = pva2pva pvaClient

# Add any checked-out submodules to DIRS
DIRS += $(subst /,,$(dir $(wildcard $(addsuffix /Makefile, $(SUBMODULES)))))

include $(TOP)/configure/RULES_DIRS

# Ensure that RELEASE.local exists before doing anything else
all host $(DIRS) $(ARCHS) $(ACTIONS) $(dirActionTargets) $(dirArchTargets) \
    $(dirActionArchTargets) $(actionArchTargets): | RELEASE.local

RELEASE.local:
	@echo EPICS_BASE = $(abspath $(INSTALL_LOCATION))> $@
realclean:
	$(RM) RELEASE.local
