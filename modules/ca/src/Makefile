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

# Channel Access Client

DIRS += client

DIRS += tools
tools_DEPEND_DIRS = client

ifneq ($(PERL_BINDINGS),NO)
  DIRS += perl
  perl_DEPEND_DIRS = client
endif

DIRS += template

include $(TOP)/configure/RULES_DIRS
