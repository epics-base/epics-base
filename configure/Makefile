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

include $(TOP)/configure/RULES
