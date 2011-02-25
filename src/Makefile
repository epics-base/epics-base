#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Revision-Id$
#

TOP = ..
include $(TOP)/configure/CONFIG

DIRS += tools

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
ca/legacy/pcas/ex_DEPEND_DIRS = ca/legacy/pcas ioc/as ioc/registry

DIRS += ca/client/perl
ca/client/perl_DEPEND_DIRS = ca/client

# PDB Core

DIRS += ioc/dbStatic
ioc/dbStatic_DEPEND_DIRS = libCom

DIRS += ioc/registry
ioc/registry_DEPEND_DIRS = ioc/dbStatic

DIRS += ioc/bpt
ioc/bpt_DEPEND_DIRS = ioc/dbStatic

DIRS += ioc/db
ioc/db_DEPEND_DIRS = ioc/bpt ca/client

DIRS += ioc/as
ioc/as_DEPEND_DIRS = ioc/db libCom

DIRS += ioc/dbtemplate
ioc/dbtemplate_DEPEND_DIRS = ioc/db

DIRS += ioc/rsrv
ioc/rsrv_DEPEND_DIRS = ioc/as

DIRS += ioc/misc
ioc/misc_DEPEND_DIRS = ioc/dbtemplate ioc/rsrv std/rec

DIRS += ioc/db/test
ioc/db/test_DEPEND_DIRS = ioc/db libCom/RTEMS

# PDB Standard Record Definitions

DIRS += std/rec
std/rec_DEPEND_DIRS = ioc/as ioc/registry

DIRS += std/dev
std/dev_DEPEND_DIRS = std/rec ioc/misc

DIRS += std/softIoc
std/softIoc_DEPEND_DIRS = std/dev ioc/dbtemplate libCom/RTEMS


include $(TOP)/configure/RULES_DIRS

