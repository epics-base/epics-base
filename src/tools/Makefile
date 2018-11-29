#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../..
include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

PERL_MODULES += EPICS/Copy.pm
PERL_MODULES += EPICS/Path.pm
PERL_MODULES += EPICS/Release.pm
PERL_MODULES += EPICS/Readfile.pm
PERL_MODULES += EPICS/Getopts.pm

# This goes into lib/perl, not bin/<host>
PERL_MODULES += EpicsHostArch.pl

PERL_SCRIPTS += assembleSnippets.pl
PERL_SCRIPTS += convertRelease.pl
PERL_SCRIPTS += cvsclean.pl
PERL_SCRIPTS += depclean.pl
PERL_SCRIPTS += dos2unix.pl
PERL_SCRIPTS += expandVars.pl
PERL_SCRIPTS += fullPathName.pl
PERL_SCRIPTS += installEpics.pl
PERL_SCRIPTS += makeMakefile.pl
PERL_SCRIPTS += makeTestfile.pl
PERL_SCRIPTS += mkmf.pl
PERL_SCRIPTS += munch.pl
PERL_SCRIPTS += podToHtml.pl
PERL_SCRIPTS += podRemove.pl
PERL_SCRIPTS += replaceVAR.pl
PERL_SCRIPTS += tap-to-junit-xml.pl
PERL_SCRIPTS += useManifestTool.pl
PERL_SCRIPTS += genVersionHeader.pl

HTMLS = style.css
HTMLS += EPICS/Getopts.html
HTMLS += EPICS/Path.html
HTMLS += EPICS/Readfile.html
HTMLS += fullPathName.html
HTMLS += munch.html
HTMLS += podToHtml.html
HTMLS += podRemove.html

include $(TOP)/configure/RULES
