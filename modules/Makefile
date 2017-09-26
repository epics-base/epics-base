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

# Add any checked-out submodules to DIRS
DIRS += $(subst /,,$(dir $(wildcard $(addsuffix /Makefile, $(SUBMODULES)))))

include $(TOP)/configure/RULES_DIRS
