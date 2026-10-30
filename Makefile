#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = .
include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

DIRS += configure src
src_DEPEND_DIRS = configure

DIRS += test
test_DEPEND_DIRS = src

DIRS += modules
modules_DEPEND_DIRS = src

include $(TOP)/configure/RULES_TOP

# we need to delete convertRelease.pl first separately as on windows
# going via the RMDIR gives a file in use error
# we need to create out own CONVERTRELEASE_PATH as existing CONVERTRELEASE
# may not point at install location
CONVERTRELEASE_PATH = $(INSTALL_LOCATION_BIN)/$(EPICS_HOST_ARCH)/convertRelease.pl
CONVERTRELEASE_WINPATH = $(subst /,\,$(CONVERTRELEASE_PATH))
preuninstall::
ifneq ($(findstring windows,$(EPICS_HOST_ARCH)),)
	if exist $(CONVERTRELEASE_WINPATH) del /f $(CONVERTRELEASE_WINPATH)
endif
