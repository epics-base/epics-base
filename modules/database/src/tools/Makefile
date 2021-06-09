#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP=../../../..

include $(TOP)/configure/CONFIG

PERL_MODULES += DBD.pm
PERL_MODULES += DBD/Base.pm
PERL_MODULES += DBD/Breaktable.pm
PERL_MODULES += DBD/Device.pm
PERL_MODULES += DBD/Driver.pm
PERL_MODULES += DBD/Link.pm
PERL_MODULES += DBD/Function.pm
PERL_MODULES += DBD/Menu.pm
PERL_MODULES += DBD/Output.pm
PERL_MODULES += DBD/Parser.pm
PERL_MODULES += DBD/Recfield.pm
PERL_MODULES += DBD/Recordtype.pm
PERL_MODULES += DBD/Record.pm
PERL_MODULES += DBD/Registrar.pm
PERL_MODULES += DBD/Variable.pm

PERL_MODULES += EPICS/IOC.pm
HTMLS += EPICS/IOC.html

PERL_SCRIPTS += makeIncludeDbd.pl

PERL_SCRIPTS += dbdToMenuH.pl
PERL_SCRIPTS += dbdToRecordtypeH.pl
PERL_SCRIPTS += dbdExpand.pl
PERL_SCRIPTS += dbExpand.pl
PERL_SCRIPTS += dbdToHtml.pl
PERL_SCRIPTS += registerRecordDeviceDriver.pl

HTMLS += dbdToHtml.html

# Build Package Config Files

C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CPPFLAGS))))
C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CFLAGS))))
PKGVARS += FINAL_LOCATION OS_CLASS CMPLR_CLASS C_CFLAGS LDFLAGS LDLIBS
PKGVARS += EPICS_VERSION EPICS_REVISION EPICS_MODIFICATION EPICS_PATCH_LEVEL
PKGVARS += CC CCC CPP AR LD
PKGVARS += EPICS_BASE_HOST_LIBS EPICS_BASE_IOC_LIBS

EXPANDFLAGS += $(foreach var,$(PKGVARS),-D$(var)="$(strip $($(var)))")
PKGCONFIG += epics-base-$(T_A).pc
ifeq ($(T_A),$(EPICS_HOST_ARCH))
PKGCONFIG += epics-base.pc
endif

EXPAND += $(PKGCONFIG:%=%@)
CLEANS += epics-base-$(T_A).pc@

include $(TOP)/configure/RULES

epics-base-$(T_A).pc@: ../epics-base-arch.pc@
	@$(RM) $@
	@$(CP) $< $@
