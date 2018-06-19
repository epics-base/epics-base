#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP = ..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

CFG += CONFIG_LIBCOM_MODULE
CFG += CONFIG_LIBCOM_VERSION

include $(TOP)/configure/RULES
