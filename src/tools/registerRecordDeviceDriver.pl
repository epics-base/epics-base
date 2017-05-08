#!/usr/bin/env perl
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

our ($opt_D, @opt_I, $opt_o, $opt_l);

getopts('Dlo:I@') or
    die "Usage: registerRecordDeviceDriver [-D] [-l] [-o out.c] [-I dir] in.dbd subname [TOP]";

my @path = map { split /[:;]/ } @opt_I; # FIXME: Broken on Win32?

my ($file, $subname, $bldTop) = @ARGV;

my $dbd = DBD->new();
ParseDBD($dbd, Readfile($file, "", \@path));

if ($opt_D) {   # Output dependencies only
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
    exit 0;
}

$Text::Wrap::columns = 75;

# Eliminate chars not allowed in C symbol names
my $c_bad_ident_chars = '[^0-9A-Za-z_]';
$subname =~ s/$c_bad_ident_chars/_/g;

# Process bldTop like convertRelease.pl does
$bldTop = LocalPath(UnixPath($bldTop));
$bldTop =~ s/([\\"])/\\\1/g; # escape back-slashes and double-quotes

# Create output file
my $out;
if ($opt_o) {
    open $out, '>', $opt_o or die "Can't create $opt_o: $!\n";
} else {
    $out = *STDOUT;
}

print $out (<< "END");
/* THIS IS A GENERATED FILE. DO NOT EDIT! */
/* Generated from $file */

#include <string.h>
#ifndef USE_TYPED_RSET
#  define USE_TYPED_RSET
#endif
#include "compilerDependencies.h"
#include "epicsStdlib.h"
#include "iocsh.h"
#include "iocshRegisterCommon.h"
#include "registryCommon.h"
#include "recSup.h"

END

print $out (<< "END") if $opt_l;
#define epicsExportSharedSymbols
#include "shareLib.h"

END

print $out (<< "END");
extern "C" {

END

my %rectypes = %{$dbd->recordtypes};
my @rtypnames;
my @dsets;
if (%rectypes) {
    my @allrtypnames = sort keys %rectypes;
    # Record types with no fields defined are declarations,
    # for building shared libraries containing device support.
    @rtypnames = grep { scalar $rectypes{$_}->fields } @allrtypnames;

    if (@rtypnames) {
        # Declare the record support entry tables
        print $out wrap('epicsShareExtern typed_rset ', '    ',
            join(', ', map {"*pvar_rset_${_}RSET"} @rtypnames)), ";\n\n";

        # Declare the RecordSizeOffset functions
        print $out "typedef int (*rso_func)(dbRecordType *pdbRecordType);\n";
        print $out wrap('epicsShareExtern rso_func ', '    ',
            join(', ', map {"pvar_func_${_}RecordSizeOffset"} @rtypnames)), ";\n\n";

        # List of record type names
        print $out "static const char * const recordTypeNames[] = {\n";
        print $out wrap('    ', '    ', join(', ', map {"\"$_\""} @rtypnames));
        print $out "\n};\n\n";

        # List of pointers to each RSET and RecordSizeOffset function
        print $out "static const recordTypeLocation rtl[] = {\n";
        print $out join(",\n", map {
                "    {(struct typed_rset *)pvar_rset_${_}RSET, pvar_func_${_}RecordSizeOffset}"
            } @rtypnames);
        print $out "\n};\n\n";
    }

    for my $rtype (@allrtypnames) {
        my @devices = $rectypes{$rtype}->devices;
        for my $dtype (@devices) {
            my $dset = $dtype->name;
            push @dsets, $dset;
        }
    }

    if (@dsets) {
        # Declare the device support entry tables
        print $out wrap('epicsShareExtern dset ', '    ',
            join(', ', map {"*pvar_dset_$_"} @dsets)), ";\n\n";

        # List of dset names
        print $out "static const char * const deviceSupportNames[] = {\n";
        print $out wrap('    ', '    ', join(', ', map {"\"$_\""} @dsets));
        print $out "\n};\n\n";

        # List of pointers to each dset
        print $out "static const dset * const devsl[] = {\n";
        print $out wrap('    ', '    ', join(", ", map {"pvar_dset_$_"} @dsets));
        print $out "\n};\n\n";
    }
}

my %drivers = %{$dbd->drivers};
if (%drivers) {
    my @drivers = sort keys %drivers;

    # Declare the driver entry tables
    print $out wrap('epicsShareExtern drvet ', '    ',
        join(', ', map {"*pvar_drvet_$_"} @drivers)), ";\n\n";

    # List of drvet names
    print $out "static const char *driverSupportNames[] = {\n";
    print $out wrap('    ', '    ', join(', ', map {"\"$_\""} @drivers));
    print $out "};\n\n";

    # List of pointers to each drvet
    print $out "static struct drvet *drvsl[] = {\n";
    print $out join(",\n", map {"    pvar_drvet_$_"} @drivers);
    print $out "};\n\n";
}

my %links = %{$dbd->links};
if (%links) {
    my @links = sort keys %links;

    # Declare the link interfaces
    print $out wrap('epicsShareExtern jlif ', '    ',
        join(', ', map {"*pvar_jlif_$_"} @links)), ";\n\n";

    # List of pointers to each link interface
    print $out "static struct jlif *jlifsl[] = {\n";
    print $out join(",\n", map {"    pvar_jlif_$_"} @links);
    print $out "};\n\n";
}

my @registrars = sort keys %{$dbd->registrars};
my @functions = sort keys %{$dbd->functions};
push @registrars, map {"register_func_$_"} @functions;
if (@registrars) {
    # Declare the registrar functions
    print $out "typedef void (*reg_func)(void);\n";
    print $out wrap('epicsShareExtern reg_func ', '    ',
        join(', ', map {"pvar_func_$_"} @registrars)), ";\n\n";
}

my %variables = %{$dbd->variables};
if (%variables) {
    my @varnames = sort keys %variables;

    # Declare the variables
    for my $var (@varnames) {
        my $vtype = $variables{$var}->var_type;
        print $out "epicsShareExtern $vtype * const pvar_${vtype}_$var;\n";
    }

    # Generate the structure for registering variables with iocsh
    print $out "\nstatic struct iocshVarDef vardefs[] = {\n";
    for my $var (@varnames) {
        my $vtype = $variables{$var}->var_type;
        my $itype = $variables{$var}->iocshArg_type;
        print $out "    {\"$var\", $itype, pvar_${vtype}_$var},\n";
    }
    print $out "    {NULL, iocshArgInt, NULL}\n};\n\n";
}

# Now for actual registration routine

print $out (<< "END");
int $subname(DBBASE *pbase)
{
    static int executed = 0;
END

print $out (<< "END") if $bldTop ne '';
    const char *bldTop = "$bldTop";
    const char *envTop = getenv("TOP");

    if (envTop && strcmp(envTop, bldTop)) {
        printf("Warning: IOC is booting with TOP = \\"%s\\"\\n"
               "          but was built with TOP = \\"%s\\"\\n",
               envTop, bldTop);
    }

END

print $out (<< 'END');
    if (!pbase) {
        printf("pdbbase is NULL; you must load a DBD file first.\n");
        return -1;
    }

    if (executed) {
        printf("Warning: Registration already done.\n");
    }
    executed = 1;

END

print $out (<< 'END') if %rectypes && @rtypnames;
    registerRecordTypes(pbase, NELEMENTS(rtl), recordTypeNames, rtl);
END

print $out (<< 'END') if @dsets;
    registerDevices(pbase, NELEMENTS(devsl), deviceSupportNames, devsl);
END

print $out (<< 'END') if %drivers;
    registerDrivers(pbase, NELEMENTS(drvsl), driverSupportNames, drvsl);
END

print $out (<< 'END') if %links;
    registerJLinks(pbase, NELEMENTS(jlifsl), jlifsl);
END

print $out (<< "END") for @registrars;
    pvar_func_$_();
END

print $out (<< 'END') if %variables;
    iocshRegisterVariable(vardefs);
END

print $out (<< "END");
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

static int done EPICS_UNUSED = Registration();
END

if ($opt_o) {
    close $out or die "Closing $opt_o failed: $!\n";
}
exit 0;
