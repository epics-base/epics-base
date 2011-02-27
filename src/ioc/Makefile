#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

SRC = $(TOP)/src
IOCDIR = $(SRC)/ioc

LIBRARY_IOC += dbCore
dbCore_LIBS += ca Com
dbCore_SYS_LIBS_WIN32 += ws2_32

dbCore_RCS += dbCore.rc
dbStaticHost_RCS = dbStaticHost.rc

PROD_LIBS = Com

include $(IOCDIR)/as/Makefile
include $(IOCDIR)/bpt/Makefile
include $(IOCDIR)/db/Makefile
include $(IOCDIR)/dbStatic/Makefile
include $(IOCDIR)/dbtemplate/Makefile
include $(IOCDIR)/misc/Makefile
include $(IOCDIR)/registry/Makefile
include $(IOCDIR)/rsrv/Makefile

include $(TOP)/configure/RULES

include $(IOCDIR)/dbStatic/RULES
include $(IOCDIR)/bpt/RULES
include $(IOCDIR)/db/RULES
include $(IOCDIR)/dbtemplate/RULES

