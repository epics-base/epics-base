#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

CONFIGS += CONFIG
CONFIGS += CONFIG_BASE
CONFIGS += CONFIG_BASE_VERSION
CONFIGS += CONFIG_COMMON
CONFIGS += CONFIG_ENV

CONFIGS += CONFIG.CrossCommon
CONFIGS += $(subst ../,,$(wildcard ../CONFIG.Host.*))
CONFIGS += $(subst ../,,$(wildcard ../CONFIG.Target.*))

CONFIGS += $(subst ../,,$(wildcard ../CONFIG_SITE*))

CONFIGS += RELEASE
CONFIGS += $(subst ../,,$(wildcard ../RELEASE.*))
CONFIGS += RULES_ARCHS
CONFIGS += RULES_BUILD
CONFIGS += RULES_DIRS
CONFIGS += RULES_TOP

CONFIGS += $(subst ../,,$(wildcard ../tools/*.pl))
CONFIGS += tools/installEpics

include $(TOP)/configure/RULES_BUILD

