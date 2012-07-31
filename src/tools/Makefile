#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
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
PERL_MODULES += EPICS/Getopts.pm

PERL_SCRIPTS += convertRelease.pl
PERL_SCRIPTS += cvsclean.pl
PERL_SCRIPTS += dos2unix.pl
PERL_SCRIPTS += expandVars.pl
PERL_SCRIPTS += filterWarnings.pl
PERL_SCRIPTS += fullPathName.pl
PERL_SCRIPTS += installEpics.pl
PERL_SCRIPTS += makeIncludeDbd.pl
PERL_SCRIPTS += makeMakefile.pl
PERL_SCRIPTS += makeTestfile.pl
PERL_SCRIPTS += mkmf.pl
PERL_SCRIPTS += munch.pl
PERL_SCRIPTS += replaceVAR.pl
PERL_SCRIPTS += useManifestTool.pl

include $(TOP)/configure/RULES
 
