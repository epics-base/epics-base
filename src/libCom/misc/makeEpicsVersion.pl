#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;

my ($ver, $rev, $mod, $patch, $snapshot, $commit_date);

my ($infile, $outdir, $site_ver) = @ARGV;

die "Usage: perl makeEpicsVersion.pl CONFIG_BASE_VERSION outdir siteversion"
    unless ($infile && $outdir);

print "Building epicsVersion.h from $infile\n";

open my $VARS, '<', $infile
    or die "Can't open $infile: $!\n";

while (<$VARS>) {
    chomp;
    next if m/^\s*#/;   # Skip comments
    if (m/^EPICS_VERSION\s*=\s*(\d+)/)              { $ver = $1; }
    if (m/^EPICS_REVISION\s*=\s*(\d+)/)             { $rev = $1; }
    if (m/^EPICS_MODIFICATION\s*=\s*(\d+)/)         { $mod = $1; }
    if (m/^EPICS_PATCH_LEVEL\s*=\s*(\d+)/)          { $patch = $1; }
    if (m/^EPICS_DEV_SNAPSHOT\s*=\s*([-\w]*)/)      { $snapshot = $1; }
    if (m/^COMMIT_DATE\s*=\s*"\\(.*)"/)             { $commit_date = $1; }
}
close $VARS;

map {
    die "Variable missing from $infile" unless defined $_;
} $ver, $rev, $mod, $patch, $snapshot, $commit_date;

my $ver_str = "$ver.$rev.$mod";
$ver_str .= ".$patch" if $patch > 0;
$ver_str .= $snapshot if $snapshot ne '';
$ver_str .= "-$site_ver" if $site_ver;

print "Found EPICS Version $ver_str\n";

my $epicsVersion="$outdir/epicsVersion.h";

mkdir ($outdir, 0777)  unless -d $outdir;

open my $OUT, '>', $epicsVersion
    or die "Cannot create $epicsVersion: $!\n";

print $OUT <<"END_OUTPUT";
/* Generated epicsVersion.h */

#define EPICS_VERSION        $ver
#define EPICS_REVISION       $rev
#define EPICS_MODIFICATION   $mod
#define EPICS_PATCH_LEVEL    $patch
#define EPICS_DEV_SNAPSHOT   "$snapshot"
#define EPICS_SITE_VERSION   "$site_ver"
#define EPICS_VERSION_STRING "EPICS $ver_str"
#define epicsReleaseVersion  "EPICS R$ver_str $commit_date"

/* The following names are deprecated, use the equivalent name above */
#define EPICS_UPDATE_LEVEL   EPICS_PATCH_LEVEL
#define EPICS_CVS_SNAPSHOT   EPICS_DEV_SNAPSHOT

END_OUTPUT

close $OUT;
