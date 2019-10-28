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

USR_CPPFLAGS += -DUSE_TYPED_RSET

SHRLIB_VERSION = 3.17.0

LIBRARY_IOC += dbCore
dbCore_LIBS += ca Com
dbCore_SYS_LIBS_WIN32 += ws2_32

dbCore_RCS += dbCore.rc
dbStaticHost_RCS = dbStaticHost.rc

INC += databaseVersion.h
INC += databaseVersionNum.h

PROD_LIBS = Com

include $(IOCDIR)/as/Makefile
include $(IOCDIR)/bpt/Makefile
include $(IOCDIR)/db/Makefile
include $(IOCDIR)/dbStatic/Makefile
include $(IOCDIR)/dbtemplate/Makefile
include $(IOCDIR)/misc/Makefile
include $(IOCDIR)/registry/Makefile
include $(IOCDIR)/rsrv/Makefile

EXPANDVARS += EPICS_DATABASE_MAJOR_VERSION
EXPANDVARS += EPICS_DATABASE_MINOR_VERSION
EXPANDVARS += EPICS_DATABASE_MAINTENANCE_VERSION
EXPANDVARS += EPICS_DATABASE_DEVELOPMENT_FLAG

EXPANDFLAGS += $(foreach var,$(EXPANDVARS),-D$(var)="$(strip $($(var)))")

include $(TOP)/configure/RULES

include $(IOCDIR)/dbStatic/RULES
include $(IOCDIR)/bpt/RULES
include $(IOCDIR)/db/RULES
include $(IOCDIR)/dbtemplate/RULES

# Can't use EXPAND as generated headers must appear
# in O.Common, but EXPAND emits rules for O.$(T_A)
../O.Common/databaseVersionNum.h: ../databaseVersionNum.h@
	$(MKDIR) $(COMMON_DIR)
	$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@
