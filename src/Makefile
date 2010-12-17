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

DIRS += libCom
libCom_DEPEND_DIRS = tools

DIRS += libCom/tools
libCom/tools_DEPEND_DIRS = libCom

DIRS += libCom/as
libCom/as_DEPEND_DIRS = libCom libCom/tools

DIRS += ca/client
ca/client_DEPEND_DIRS = libCom

DIRS += ioc/dbStatic
ioc/dbStatic_DEPEND_DIRS = libCom/tools

DIRS += ioc/registry
ioc/registry_DEPEND_DIRS = ioc/dbStatic

DIRS += ioc/bpt
ioc/bpt_DEPEND_DIRS = ioc/dbStatic

DIRS += ioc/db
ioc/db_DEPEND_DIRS = ioc/bpt ca/client

DIRS += ioc/as
ioc/as_DEPEND_DIRS = ioc/db libCom/as

DIRS += util
util_DEPEND_DIRS = ca/client

DIRS += ioc/dbtemplate
ioc/dbtemplate_DEPEND_DIRS = ioc/db

DIRS += ca/client/tools
ca/client/tools_DEPEND_DIRS = ca/client ioc/dbStatic

DIRS += ioc/rsrv
ioc/rsrv_DEPEND_DIRS = ioc/as

DIRS += std/rec
std/rec_DEPEND_DIRS = ioc/as ioc/registry

DIRS += ioc/misc
ioc/misc_DEPEND_DIRS = ioc/dbtemplate ioc/rsrv std/rec

DIRS += std/dev
std/dev_DEPEND_DIRS = std/rec ioc/misc

DIRS += libCom/RTEMS
libCom/RTEMS_DEPEND_DIRS = libCom

DIRS += libCom/test
libCom/test_DEPEND_DIRS = ca/client libCom/RTEMS

DIRS += ioc/db/test
ioc/db/test_DEPEND_DIRS = ioc/db libCom/RTEMS

DIRS += std/softIoc
std/softIoc_DEPEND_DIRS = std/dev ioc/dbtemplate libCom/RTEMS

DIRS += ca/legacy/gdd
ca/legacy/gdd_DEPEND_DIRS = ca/client

DIRS += ca/legacy/pcas
ca/legacy/pcas_DEPEND_DIRS = ca/legacy/gdd ioc/dbStatic

DIRS += ca/legacy/pcas/ex
ca/legacy/pcas/ex_DEPEND_DIRS = ca/legacy/pcas ioc/as ioc/registry

DIRS += ca/client/perl
ca/client/perl_DEPEND_DIRS = ca/client ioc/dbStatic


include $(TOP)/configure/RULES_DIRS

