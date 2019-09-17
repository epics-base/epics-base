#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../../..
include $(TOP)/configure/CONFIG

# PDB Tools

DIRS += tools

# PDB Core

DIRS += ioc
ioc_DEPEND_DIRS = tools

# PDB Standard Record Definitions

DIRS += std
std_DEPEND_DIRS = ioc

# Templates

DIRS += template

include $(TOP)/configure/RULES_DIRS
