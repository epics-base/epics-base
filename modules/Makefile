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

inc: RELEASE.base

# Submodules for bundle build
SUBMODULES += libcom

SUBMODULES += ca
ca_DEPEND_DIRS = libcom

SUBMODULES += database
database_DEPEND_DIRS = ca

SUBMODULES += pvData
pvData_DEPEND_DIRS = libcom

SUBMODULES += pvAccess
pvAccess_DEPEND_DIRS = pvData ca
# FIXME: dependency needed for the pvAccessIOC library
pvAccess_DEPEND_DIRS += database

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

RELEASE.base:
	@echo "EPICS_BASE = $(dir $(abspath $(lastword $(MAKEFILE_LIST))/..))" > $@
realclean:
	$(RM) RELEASE.base
