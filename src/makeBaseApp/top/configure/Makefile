#
# $Id$
#

TOP=..

include $(TOP)/configure/CONFIG

TARGETS = CONFIG_APP_INCLUDE

include $(TOP)/configure/RULES

ifeq ($(wildcard $(TOP)/configure/RELEASE.$(HOST_ARCH)),$(TOP)/configure/RELEASE.$(HOST_ARCH))
CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE.$(HOST_ARCH)
endif

CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE $(TOP)/configure/CONFIG_APP
	$(RM) $@
	@$(PERL) $(TOOLS)/makeConfigAppInclude.pl $(T_A) $@ $(TOP)

