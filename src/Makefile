#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Id$
#

TOP = ..
include $(TOP)/configure/CONFIG

DIRS += tools
DIRS += libCom
DIRS += ca
DIRS += RTEMS
DIRS += libCom/test
DIRS += toolsComm
DIRS += dbStatic
DIRS += registry
DIRS += bpt
DIRS += db
DIRS += db/test
DIRS += as
DIRS += util
DIRS += dbtools
DIRS += catools
DIRS += rsrv
DIRS += rec
DIRS += misc
DIRS += dev
DIRS += vxWorks
DIRS += softIoc
DIRS += gdd
DIRS += cas
DIRS += makeBaseApp
DIRS += excas
DIRS += makeBaseExt
DIRS += cap5

libCom_DEPEND_DIRS = tools
ca_DEPEND_DIRS = libCom
RTEMS_DEPEND_DIRS = libCom
libCom/test_DEPEND_DIRS = libCom RTEMS
toolsComm_DEPEND_DIRS = libCom
dbStatic_DEPEND_DIRS = toolsComm
registry_DEPEND_DIRS = dbStatic
bpt_DEPEND_DIRS = dbStatic
db_DEPEND_DIRS = bpt ca
db/test_DEPEND_DIRS = db
as_DEPEND_DIRS = db
util_DEPEND_DIRS = ca
dbtools_DEPEND_DIRS = db
catools_DEPEND_DIRS = ca dbStatic
rsrv_DEPEND_DIRS = as
rec_DEPEND_DIRS = as registry
misc_DEPEND_DIRS = rsrv rec
dev_DEPEND_DIRS = rec misc
vxWorks_DEPEND_DIRS = ca
softIoc_DEPEND_DIRS = dev dbtools
gdd_DEPEND_DIRS = libCom
cas_DEPEND_DIRS = ca gdd
excas_DEPEND_DIRS = cas as registry
makeBaseApp_DEPEND_DIRS = tools
makeBaseExt_DEPEND_DIRS = tools
cap5_DEPEND_DIRS = ca dbStatic


include $(TOP)/configure/RULES_DIRS

