#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

INSTALL_CONFIG = $(INSTALL_LOCATION)/config

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

SCRIPTS_HOST += $(wildcard ../tools/*.pl)
SCRIPTS_HOST += ../tools/installEpics

include $(TOP)/configure/RULES

