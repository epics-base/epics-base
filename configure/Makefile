#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../os/CONFIG*))

CONFIGS += $(subst ../,,$(wildcard ../RELEASE*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

CONFIGS += $(subst ../,,$(wildcard ../tools/*.pl))
CONFIGS += tools/installEpics

#####CONFIGS += tools/getrel

include $(TOP)/configure/RULES

