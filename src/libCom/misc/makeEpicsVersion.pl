#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;

use Getopt::Std;
use File::Basename;

my $tool = basename($0);

our ($opt_h, $opt_q, $opt_v);
our $opt_o = 'epicsVersion.h';

$Getopt::Std::OUTPUT_HELP_VERSION = 1;

HELP_MESSAGE() unless getopts('ho:qv:') && @ARGV == 1;
HELP_MESSAGE() if $opt_h;

my ($infile) = @ARGV;

print "Building $opt_o from $infile\n" unless $opt_q;

open my $VARS, '<', $infile
    or die "$tool: Can't open $infile: $!\n";

my ($ver, $rev, $mod, $patch, $snapshot);
while (<$VARS>) {
    chomp;
    next if m/^\s*#/;   # Skip comments
    if (m/^EPICS_VERSION\s*=\s*(\d+)/)              { $ver = $1; }
    if (m/^EPICS_REVISION\s*=\s*(\d+)/)             { $rev = $1; }
    if (m/^EPICS_MODIFICATION\s*=\s*(\d+)/)         { $mod = $1; }
    if (m/^EPICS_PATCH_LEVEL\s*=\s*(\d+)/)          { $patch = $1; }
    if (m/^EPICS_DEV_SNAPSHOT\s*=\s*([-\w]*)/)      { $snapshot = $1; }
}
close $VARS;

map {
    die "$tool: Variable missing from $infile" unless defined $_;
} $ver, $rev, $mod, $patch, $snapshot;

my $ver_str = "$ver.$rev.$mod";
$ver_str .= ".$patch" if $patch > 0;
my $ver_short = $ver_str;
$ver_str .= $snapshot if $snapshot ne '';
$ver_str .= "-$opt_v" if $opt_v;

print "Found EPICS Version $ver_str\n" unless $opt_q;

open my $OUT, '>', $opt_o
    or die "$tool: Can't create $opt_o: $!\n";

my $obase = basename($opt_o, '.h');

print $OUT <<"END";
/* Generated file $opt_o */

#ifndef INC_${obase}_H
#define INC_${obase}_H

#define EPICS_VERSION        $ver
#define EPICS_REVISION       $rev
#define EPICS_MODIFICATION   $mod
#define EPICS_PATCH_LEVEL    $patch
#define EPICS_DEV_SNAPSHOT   "$snapshot"
#define EPICS_SITE_VERSION   "$opt_v"

#define EPICS_VERSION_SHORT  "$ver_short"
#define EPICS_VERSION_FULL   "$ver_str"
#define EPICS_VERSION_STRING "EPICS $ver_str"
#define epicsReleaseVersion  "EPICS R$ver_str"

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif
#define EPICS_VERSION_INT VERSION_INT($ver, $rev, $mod, $patch)

#endif /* INC_${obase}_H */
END

close $OUT;

sub HELP_MESSAGE {
    print STDERR "Usage: $tool [options] CONFIG_BASE_VERSION\n",
        "  -h       Help: Print this message\n",
        "  -q       Quiet: Only print errors\n",
        "  -o file  Output filename, default is $opt_o\n",
        "  -v vers  Site-specific version string\n",
        "\n";
    exit 1;
}
