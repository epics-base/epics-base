#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Id$
#

TOP = ..
include $(TOP)/configure/CONFIG

#
# for c++ build add the dirs gdd and cas after ca
# (and uncomment the c++ files in src/lbCom/Makefile.Unix)
#
DIRS += toolsComm
DIRS += makeBaseApp
DIRS += makeBaseExt
DIRS += libCom
DIRS += ca
DIRS += dbStatic
DIRS += registry
DIRS += bpt
DIRS += db
DIRS += db/test
DIRS += as
DIRS += util
DIRS += dbtools
DIRS += rsrv
DIRS += rec
DIRS += dev
DIRS += misc
DIRS += iocsh
#Following are optional
ifeq ($(findstring vxWorks,$(BUILD_ARCHS)),vxWorks)
DIRS += vxWorks
endif
ifneq (,$(findstring RTEMS,$(BUILD_ARCHS)))
DIRS += RTEMS
endif
DIRS += softIoc
DIRS += excas
DIRS += libCom/test
DIRS += gdd
DIRS += cas

include $(TOP)/configure/RULES_DIRS

