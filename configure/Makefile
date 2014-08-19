#*************************************************************************
# Copyright (c) 2002 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Revision-Id$
#

TOP=..

include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../os/CONFIG*))

CONFIGS += $(subst ../,,$(wildcard ../RELEASE*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

ifdef T_A
ifeq ($(BUILD_CLASS)$(POSIX),HOSTYES)

FINAL_LOCATION ?= $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))

INSTALL_PC ?= $(INSTALL_LOCATION)/lib/pkgconfig

DIRECTORY_TARGETS += $(INSTALL_PC)

C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CPPFLAGS))))
C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CFLAGS))))

MVARS += FINAL_LOCATION OS_CLASS CMPLR_CLASS C_CFLAGS LDFLAGS LDLIBS
MVARS += EPICS_VERSION EPICS_REVISION EPICS_MODIFICATION EPICS_MODIFICATION

EXPANDFLAGS += $(foreach var,$(MVARS),-D$(var)="$(strip $($(var)))")

endif
endif

include $(TOP)/configure/RULES

ifdef T_A
ifeq ($(BUILD_CLASS)$(POSIX),HOSTYES)

epics-base-$(T_A).pc: ../epics-base-arch.pc.in
epics-base.pc: ../epics-base.pc.in

epics-base-$(T_A).pc epics-base.pc:
	$(ECHO) "Expanding $< to $@"
	@$(RM) $@
	@$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@

$(INSTALL_PC)/%: %
	$(ECHO) "Installing pkgconfig file $@"
	@$(INSTALL) -d -m $(INSTALL_PERMISSIONS) $< $(@D)

buildInstall: $(INSTALL_PC)/epics-base-$(T_A).pc

ifeq ($(T_A),$(EPICS_HOST_ARCH))
buildInstall: $(INSTALL_PC)/epics-base.pc
endif

endif
endif
