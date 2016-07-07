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

DIRS += makeBaseApp
makeBaseApp_DEPEND_DIRS = tools

DIRS += makeBaseExt
makeBaseExt_DEPEND_DIRS = tools

DIRS += libCom
libCom_DEPEND_DIRS = tools

DIRS += toolsComm
toolsComm_DEPEND_DIRS = libCom

DIRS += ca
ca_DEPEND_DIRS = libCom

DIRS += dbStatic
dbStatic_DEPEND_DIRS = toolsComm

DIRS += registry
registry_DEPEND_DIRS = dbStatic

DIRS += bpt
bpt_DEPEND_DIRS = dbStatic

DIRS += db
db_DEPEND_DIRS = bpt ca

DIRS += as
as_DEPEND_DIRS = db

DIRS += util
util_DEPEND_DIRS = ca

DIRS += dbtools
dbtools_DEPEND_DIRS = db

DIRS += catools
catools_DEPEND_DIRS = ca dbStatic

DIRS += rsrv
rsrv_DEPEND_DIRS = as

DIRS += rec
rec_DEPEND_DIRS = as registry

DIRS += misc
misc_DEPEND_DIRS = dbtools rsrv rec

DIRS += dev
dev_DEPEND_DIRS = rec misc

DIRS += vxWorks
vxWorks_DEPEND_DIRS = registry misc dbtools

DIRS += RTEMS
RTEMS_DEPEND_DIRS = libCom

DIRS += libCom/test
libCom/test_DEPEND_DIRS = ca RTEMS

DIRS += db/test
db/test_DEPEND_DIRS = db RTEMS

DIRS += softIoc
softIoc_DEPEND_DIRS = dev dbtools RTEMS

DIRS += gdd
gdd_DEPEND_DIRS = ca

DIRS += cas
cas_DEPEND_DIRS = gdd dbStatic

DIRS += excas
excas_DEPEND_DIRS = cas as registry

DIRS += cap5
cap5_DEPEND_DIRS = ca dbStatic


include $(TOP)/configure/RULES_DIRS

