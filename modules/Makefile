#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG
include CONFIG_SITE.local

DIRS += libcom

DIRS += ca
ca_DEPEND_DIRS = libcom

DIRS += database
database_DEPEND_DIRS = ca

# Submodules for bundle build
SUBMODULES += pvData
pvData_DEPEND_DIRS = libcom

SUBMODULES += pvAccess
pvAccess_DEPEND_DIRS = pvData database

SUBMODULES += normativeTypes
normativeTypes_DEPEND_DIRS = pvData

SUBMODULES += pvaClient
pvaClient_DEPEND_DIRS = pvAccess normativeTypes

SUBMODULES += pvDatabase
pvDatabase_DEPEND_DIRS = pvAccess

SUBMODULES += pva2pva
pva2pva_DEPEND_DIRS = pvAccess

# Allow sites to add extra submodules
-include Makefile.local

include $(TOP)/configure/RULES_MODULES
