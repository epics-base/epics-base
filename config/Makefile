#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

INSTALL_CONFIG = $(INSTALL_LOCATION)/config

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

include $(TOP)/configure/RULES

