#*************************************************************************
# Copyright (c) 2017 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2017 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG

# Submodules for bundle build
SUBMODULES += libcom ca database

ca_DEPEND_DIRS = libcom
database_DEPEND_DIRS = ca

# Add any checked-out submodules to DIRS
define SUBMODULE_template
ifneq ($(wildcard $1/Makefile),)
DIRS += $1
endif
endef
$(foreach mod, $(SUBMODULES), \
    $(eval $(call SUBMODULE_template,$(strip $(mod)))))

include $(TOP)/configure/RULES_DIRS
