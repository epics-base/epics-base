eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # registerRecordDeviceDriver 
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use EPICS::Readfile;
use EPICS::Path;
use EPICS::Getopts;
use Text::Wrap;

getopts('I@') or
    die "Usage: registerRecordDeviceDriver [-I dir] in.dbd subroutinename [TOP]";

my @path = map { split /[:;]/ } @EPICS::Getopts::opt_I; # FIXME: Broken on Win32?

my ($file, $subname, $bldTop) = @ARGV;

my $dbd = DBD->new();
&ParseDBD($dbd, &Readfile($file, "", \@path));

$Text::Wrap::columns = 75;

# Eliminate chars not allowed in C symbol names
my $c_bad_ident_chars = '[^0-9A-Za-z_]';
$subname =~ s/$c_bad_ident_chars/_/g;

# Process bldTop like convertRelease.pl does
$bldTop = LocalPath(UnixPath($bldTop));
$bldTop =~ s/([\\"])/\\\1/g; # escape back-slashes and double-quotes


# Start of generated file

print << "END" ;
/* THIS IS A GENERATED FILE. DO NOT EDIT! */
/* Generated from $file */

#include <string.h>

#include "epicsStdlib.h"
#include "iocsh.h"
#include "iocshRegisterCommon.h"
#include "registryCommon.h"

extern "C" {

END

my %rectypes = %{$dbd->recordtypes};
my @dsets;
if (%rectypes) {
    my @rtypnames = sort keys %rectypes;

    # Declare the record support entry tables
    print wrap('epicsShareExtern rset ', '    ',
        join(', ', map {"*pvar_rset_${_}RSET"} @rtypnames)), ";\n\n";

    # Declare the RecordSizeOffset functions
    print "typedef int (*rso_func)(dbRecordType *pdbRecordType);\n";
    print wrap('epicsShareExtern rso_func ', '    ',
        join(', ', map {"pvar_func_${_}RecordSizeOffset"} @rtypnames)), ";\n\n";

    # List of record type names
    print "static const char * const recordTypeNames[] = {\n";
    print wrap('    ', '    ', join(', ', map {"\"$_\""} @rtypnames));
    print "\n};\n\n";

    # List of pointers to each RSET and RecordSizeOffset function
    print "static const recordTypeLocation rtl[] = {\n";
    print join(",\n", map {
            "    {pvar_rset_${_}RSET, pvar_func_${_}RecordSizeOffset}"
        } @rtypnames);
    print "\n};\n\n";

    for my $rtype (@rtypnames) {
        my @devices = $rectypes{$rtype}->devices;
        for my $dtype (@devices) {
            my $dset = $dtype->name;
            push @dsets, $dset;
        }
    }

    if (@dsets) {
        # Declare the device support entry tables
        print wrap('epicsShareExtern dset ', '    ',
            join(', ', map {"*pvar_dset_$_"} @dsets)), ";\n\n";

        # List of dset names
        print "static const char * const deviceSupportNames[] = {\n";
        print wrap('    ', '    ', join(', ', map {"\"$_\""} @dsets));
        print "\n};\n\n";

        # List of pointers to each dset
        print "static const dset * const devsl[] = {\n";
        print wrap('    ', '    ', join(", ", map {"pvar_dset_$_"} @dsets));
        print "\n};\n\n";
    }
}

my %drivers = %{$dbd->drivers};
if (%drivers) {
    my @drivers = sort keys %drivers;

    # Declare the driver entry tables
    print wrap('epicsShareExtern drvet ', '    ',
        join(', ', map {"*pvar_drvet_$_"} @drivers)), ";\n\n";

    # List of drvet names
    print "static const char *driverSupportNames[] = {\n";
    print wrap('    ', '    ', join(', ', map {"\"$_\""} @drivers));
    print "};\n\n";

    # List of pointers to each drvet
    print "static struct drvet *drvsl[] = {\n";
    print join(",\n", map {"    pvar_drvet_$_"} @drivers);
    print "};\n\n";
}

my @registrars = sort keys %{$dbd->registrars};
my @functions = sort keys %{$dbd->functions};
push @registrars, map {"register_func_$_"} @functions;
if (@registrars) {
    # Declare the registrar functions
    print "typedef void (*reg_func)(void);\n";
    print wrap('epicsShareExtern reg_func ', '    ',
        join(', ', map {"pvar_func_$_"} @registrars)), ";\n\n";
}

my %variables = %{$dbd->variables};
if (%variables) {
    my @varnames = sort keys %variables;

    # Declare the variables
    for my $var (@varnames) {
        my $vtype = $variables{$var}->var_type;
        print "epicsShareExtern $vtype * const pvar_${vtype}_$var;\n";
    }

    # Generate the structure for registering variables with iocsh
    print "\nstatic struct iocshVarDef vardefs[] = {\n";
    for my $var (@varnames) {
        my $vtype = $variables{$var}->var_type;
        my $itype = $variables{$var}->iocshArg_type;
        print "    {\"$var\", $itype, pvar_${vtype}_$var},\n";
    }
    print "    {NULL, iocshArgInt, NULL}\n};\n\n";
}

# Now for actual registration routine

print << "END";
int $subname(DBBASE *pbase)
{
    static int executed = 0;
END

print << "END" if $bldTop ne '';
    const char *bldTop = "$bldTop";
    const char *envTop = getenv("TOP");

    if (envTop && strcmp(envTop, bldTop)) {
        printf("Warning: IOC is booting with TOP = \\"%s\\"\\n"
               "          but was built with TOP = \\"%s\\"\\n",
               envTop, bldTop);
    }

END

print << 'END';
    if (!pbase) {
        printf("pdbbase is NULL; you must load a DBD file first.\n");
        return -1;
    }

    if (executed) {
        printf("Registration already done.\n");
        return 0;
    }
    executed = 1;

END

print << 'END' if %rectypes;
    registerRecordTypes(pbase, NELEMENTS(rtl), recordTypeNames, rtl);
END

print << 'END' if @dsets;
    registerDevices(pbase, NELEMENTS(devsl), deviceSupportNames, devsl);
END

print << 'END' if %drivers;
    registerDrivers(pbase, NELEMENTS(drvsl), driverSupportNames, drvsl);
END

print << "END" for @registrars;
    pvar_func_$_();
END

print << 'END' if %variables;
    iocshRegisterVariable(vardefs);
END

print << "END";
    return 0;
}

/* $subname */
static const iocshArg rrddArg0 = {"pdbbase", iocshArgPdbbase};
static const iocshArg *rrddArgs[] = {&rrddArg0};
static const iocshFuncDef rrddFuncDef =
    {"$subname", 1, rrddArgs};
static void rrddCallFunc(const iocshArgBuf *)
{
    $subname(*iocshPpdbbase);
}

} // extern "C"

/*
 * Register commands on application startup
 */
static int Registration() {
    iocshRegisterCommon();
    iocshRegister(&rrddFuncDef, rrddCallFunc);
    return 0;
}

static int done = Registration();
END
