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
DIRS += libCom
DIRS += libCom/test
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
DIRS += gdd
DIRS += cas

include $(TOP)/configure/RULES_DIRS

