#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

INSTALL_CONFIG = $(INSTALL_LOCATION)/config

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

BIN_INSTALLS += $(wildcard ../tools/*.pl)
BIN_INSTALLS += ../tools/installEpics

include $(TOP)/configure/RULES

