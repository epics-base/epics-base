# Makefile

TOP=..

include $(TOP)/configure/CONFIG

TARGETS = CONFIG_APP_INCLUDE

include $(TOP)/configure/RULES

ifeq ($(wildcard $(TOP)/configure/RELEASE.$(EPICS_HOST_ARCH)),$(TOP)/configure/RELEASE.$(HOST_ARCH))
CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE.$(EPICS_HOST_ARCH)
endif

CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE $(TOP)/configure/CONFIG_APP
	@$(RM) $@
	@$(PERL) $(TOOLS)/makeConfigAppInclude.pl $(EPICS_HOST_ARCH) $(T_A) $@ $(TOP)

depends: install
