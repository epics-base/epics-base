#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

PERL_MODULES += EPICS/Copy.pm
PERL_MODULES += EPICS/Path.pm
PERL_MODULES += EPICS/Release.pm
PERL_MODULES += EPICS/Readfile.pm
PERL_MODULES += EPICS/Getopts.pm
PERL_MODULES += EPICS/macLib.pm

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

PERL_SCRIPTS += assembleSnippets.pl
PERL_SCRIPTS += convertRelease.pl
PERL_SCRIPTS += cvsclean.pl
PERL_SCRIPTS += dos2unix.pl
PERL_SCRIPTS += expandVars.pl
PERL_SCRIPTS += fullPathName.pl
PERL_SCRIPTS += installEpics.pl
PERL_SCRIPTS += makeIncludeDbd.pl
PERL_SCRIPTS += makeMakefile.pl
PERL_SCRIPTS += makeTestfile.pl
PERL_SCRIPTS += mkmf.pl
PERL_SCRIPTS += munch.pl
PERL_SCRIPTS += replaceVAR.pl
PERL_SCRIPTS += tap-to-junit-xml.pl
PERL_SCRIPTS += useManifestTool.pl
PERL_SCRIPTS += genVersionHeader.pl

PERL_SCRIPTS += dbdToMenuH.pl
PERL_SCRIPTS += dbdToRecordtypeH.pl
PERL_SCRIPTS += dbdExpand.pl
PERL_SCRIPTS += dbExpand.pl
PERL_SCRIPTS += dbdToHtml.pl
PERL_SCRIPTS += podToHtml.pl
PERL_SCRIPTS += podRemove.pl
PERL_SCRIPTS += registerRecordDeviceDriver.pl

HTMLS = style.css
HTMLS += EPICS/Getopts.html
HTMLS += EPICS/Path.html
HTMLS += EPICS/Readfile.html
HTMLS += fullPathName.html
HTMLS += podToHtml.html
HTMLS += podRemove.html
HTMLS += munch.html

# Build Package Config Files

FINAL_LOCATION ?= $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))
C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CPPFLAGS))))
C_CFLAGS += $(filter-out -g,$(filter-out -O%,$(filter-out -W%,$(CFLAGS))))
PKGVARS += FINAL_LOCATION OS_CLASS CMPLR_CLASS C_CFLAGS LDFLAGS LDLIBS
PKGVARS += EPICS_VERSION EPICS_REVISION EPICS_MODIFICATION EPICS_PATCH_LEVEL
PKGVARS += CC CCC CPP AR LD
PKGVARS += EPICS_BASE_IOC_LIBS

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

