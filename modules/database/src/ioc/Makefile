#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
IOCDIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
TOP = ../../../..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET

# Shared library ABI version.
SHRLIB_VERSION = $(EPICS_DATABASE_MAJOR_VERSION).$(EPICS_DATABASE_MINOR_VERSION).$(EPICS_DATABASE_MAINTENANCE_VERSION)

# dbCoreAPI.h generated earlier during libcom/
dbCore_API = dbCore

LIBRARY_IOC += dbCore
dbCore_LIBS += ca Com
dbCore_SYS_LIBS_WIN32 += ws2_32

dbCore_RCS += dbCore.rc

EXPAND_COMMON += databaseVersion.h@

EXPAND_ME += EPICS_DATABASE_MAJOR_VERSION
EXPAND_ME += EPICS_DATABASE_MINOR_VERSION
EXPAND_ME += EPICS_DATABASE_MAINTENANCE_VERSION
EXPAND_ME += EPICS_DATABASE_DEVELOPMENT_FLAG

INC += databaseVersion.h

PROD_LIBS = Com

include $(IOCDIR)/as/Makefile
include $(IOCDIR)/bpt/Makefile
include $(IOCDIR)/db/Makefile
include $(IOCDIR)/dbStatic/Makefile
include $(IOCDIR)/dbtemplate/Makefile
include $(IOCDIR)/misc/Makefile
include $(IOCDIR)/registry/Makefile
include $(IOCDIR)/rsrv/Makefile

GENVERSION = epicsVCS.h
GENVERSIONMACRO = EPICS_VCS_VERSION

include $(TOP)/configure/RULES

include $(IOCDIR)/dbStatic/RULES
include $(IOCDIR)/bpt/RULES
include $(IOCDIR)/db/RULES
include $(IOCDIR)/dbtemplate/RULES

epicsRelease$(DEP): $(COMMON_DIR)/$(GENVERSION)
