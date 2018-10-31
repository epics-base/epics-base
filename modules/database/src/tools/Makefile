#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP=../..

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

PERL_SCRIPTS += databaseModuleDirs.pm

PERL_SCRIPTS += makeIncludeDbd.pl

PERL_SCRIPTS += dbdToMenuH.pl
PERL_SCRIPTS += dbdToRecordtypeH.pl
PERL_SCRIPTS += dbdExpand.pl
PERL_SCRIPTS += dbExpand.pl
PERL_SCRIPTS += dbdToHtml.pl
PERL_SCRIPTS += registerRecordDeviceDriver.pl

HTMLS += dbdToHtml.html

include $(TOP)/configure/RULES
