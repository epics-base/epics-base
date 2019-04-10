#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG
include CONFIG_SITE.local

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

# Allow sites to add extra submodules
-include Makefile.local

# Add only checked-out submodules to DIRS
DIRS += $(subst /Makefile,,$(wildcard $(addsuffix /Makefile, $(SUBMODULES))))

include $(TOP)/configure/RULES_DIRS

INSTALL_LOCATION_ABS := $(abspath $(INSTALL_LOCATION))
RELEASE_LOCAL := RELEASE.$(EPICS_HOST_ARCH).local

# Ensure that RELEASE.<host>.local exists before doing anything else
all host $(DIRS) $(ARCHS) $(ACTIONS) $(dirActionTargets) $(dirArchTargets) \
    $(dirActionArchTargets) $(actionArchTargets): | $(RELEASE_LOCAL)

# Convenience target
RELEASE.host: $(RELEASE_LOCAL)

$(RELEASE_LOCAL): Makefile CONFIG_SITE.local
	$(ECHO) Creating $@ with
	$(ECHO) "    $(PARENT_MODULE) = $(INSTALL_LOCATION_ABS)"
	@echo $(PARENT_MODULE) = $(INSTALL_LOCATION_ABS)> $@
realclean:
	$(RM) $(wildcard RELEASE.*.local)

.PHONY: RELEASE.host realclean
