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

STDDIR=$(TOP)/src/std

USR_DBDFLAGS += -I ../rec -I ../dev -I ../softIoc

LIBRARY_IOC += dbRecStd
dbRecStd_LIBS = dbCore ca Com

dbRecStd_RCS += dbRecStd.rc

include $(STDDIR)/dev/Makefile
include $(STDDIR)/rec/Makefile
include $(STDDIR)/softIoc/Makefile

include $(TOP)/configure/RULES

include $(STDDIR)/rec/RULES
include $(STDDIR)/softIoc/RULES

