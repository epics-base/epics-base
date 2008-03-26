#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

# Bootstrap resolution: expandVars.pl needs to be run on itself!
EXPAND_VARS = $(PERL) ../expandVars.pl@

EXPAND += expandVars.pl@

PERL_MODULES += Ctlsys/Utils.pm
PERL_MODULES += Ctlsys/Getopts.pm

PERL_SCRIPTS += convertRelease.pl*
PERL_SCRIPTS += cvsclean.pl*
PERL_SCRIPTS += dos2unix.pl
PERL_SCRIPTS += expandVars.pl
PERL_SCRIPTS += filterWarnings.pl*
PERL_SCRIPTS += fullPathName.pl*
PERL_SCRIPTS += installEpics.pl*
PERL_SCRIPTS += makeDbDepends.pl
PERL_SCRIPTS += makeIncludeDbd.pl
PERL_SCRIPTS += makeMakefile.pl*
PERL_SCRIPTS += makeMakefileInclude.pl*
PERL_SCRIPTS += mkmf.pl*
PERL_SCRIPTS += munch.pl*
PERL_SCRIPTS += replaceVAR.pl*
PERL_SCRIPTS += useManifestTool.pl*

include $(TOP)/configure/RULES
 
