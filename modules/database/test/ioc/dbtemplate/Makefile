#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../../../../..

include $(TOP)/configure/CONFIG

TESTS += msi

# msi is a host tool, so its tests must run on the host.
ifeq ($(T_A),$(EPICS_HOST_ARCH))
TESTSCRIPTS_IOC += $(TESTS:%=%.t)
endif

include $(TOP)/configure/RULES
