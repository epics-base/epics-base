#*************************************************************************
# Copyright (c) 2002 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=..

include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../os/CONFIG.*))
CONFIGS += $(subst ../,,$(wildcard ../os/CONFIG_SITE.*))

CONFIGS += $(subst ../,,$(wildcard ../RELEASE*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

CFG += CONFIG_LIBCOM_MODULE
CFG += CONFIG_LIBCOM_VERSION

CFG += CONFIG_CA_MODULE
CFG += CONFIG_CA_VERSION

CFG += CONFIG_DATABASE_MODULE
CFG += CONFIG_DATABASE_VERSION

CFG += TOOLCHAIN.$(EPICS_HOST_ARCH).$(T_A)

include $(TOP)/configure/RULES

ifeq ($(GNU),YES)
  # Pass compiler flags to preprocessor to enable _FORTIFY_SOURCE
  TOOLCHAIN.$(EPICS_HOST_ARCH).$(T_A): CPPFLAGS += $(CFLAGS)
endif
TOOLCHAIN.$(EPICS_HOST_ARCH).$(T_A): toolchain.c
	$(PREPROCESS.cpp)
