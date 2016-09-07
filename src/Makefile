#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG

DIRS += tools

DIRS += tools/test
tools/test_DEPEND_DIRS = tools

DIRS += template/base
template/base_DEPEND_DIRS = tools

DIRS += template/ext
template/ext_DEPEND_DIRS = tools

# Common

DIRS += libCom
libCom_DEPEND_DIRS = tools

DIRS += libCom/RTEMS
libCom/RTEMS_DEPEND_DIRS = libCom

DIRS += libCom/test
libCom/test_DEPEND_DIRS = libCom/RTEMS

# Channel Access

DIRS += ca/client
ca/client_DEPEND_DIRS = libCom

DIRS += ca/client/tools
ca/client/tools_DEPEND_DIRS = ca/client

DIRS += ca/legacy/gdd
ca/legacy/gdd_DEPEND_DIRS = ca/client

DIRS += ca/legacy/pcas
ca/legacy/pcas_DEPEND_DIRS = ca/legacy/gdd

DIRS += ca/legacy/pcas/ex
# needs ioc for dbStaticHost
ca/legacy/pcas/ex_DEPEND_DIRS = ca/legacy/pcas libCom ioc

DIRS += ca/client/perl
ca/client/perl_DEPEND_DIRS = ca/client

# PDB Core

DIRS += ioc
ioc_DEPEND_DIRS = libCom ca/client

DIRS += ioc/db/test
ioc/db/test_DEPEND_DIRS = ioc libCom/RTEMS

DIRS += ioc/dbtemplate/test
ioc/dbtemplate/test_DEPEND_DIRS = ioc

# PDB Standard Record Definitions

DIRS += std
std_DEPEND_DIRS = ioc libCom/RTEMS

DIRS += std/filters/test
std/filters/test_DEPEND_DIRS = std

DIRS += std/rec/test
std/rec/test_DEPEND_DIRS = std


include $(TOP)/configure/RULES_DIRS

