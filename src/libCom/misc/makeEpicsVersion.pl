#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

($infile, $outdir, $site_ver) = @ARGV;

die "Usage: perl makeEpicsVersion.pl CONFIG_BASE_VERSION outdir siteversion"
    unless ($infile && $outdir);

print "Building epicsVersion.h from $infile\n";

open VARS, $infile or die "Can't open $infile: $!\n";

while (<VARS>)
{
	chomp;
	next if m/^#/;	# Skip comments
	if (m/^EPICS_VERSION\s*=\s*(\d+)/)		{ $ver = $1; }
	if (m/^EPICS_REVISION\s*=\s*(\d+)/)		{ $rev = $1; }
	if (m/^EPICS_MODIFICATION\s*=\s*([0-9a-z]+)/)	{ $mod = $1; }
	if (m/^EPICS_PATCH_LEVEL\s*=\s*(\d+)/)		{ $patch = $1; }
	if (m/^EPICS_CVS_SNAPSHOT\s*=\s*([CVS-]+)/)	{ $snapshot = $1; }
	if (m/^CVS_DATE\s*=\s*"\\(.*)"/)		{ $cvs_date = $1; }
	if (m/^CVS_TAG\s*=\s*"\\(.*)"/)			{ $cvs_tag = $1; }
}

$ver_str = "$ver.$rev.$mod";
$ver_str .= ".$patch" if $patch > 0;
$ver_str .= $snapshot if $snapshot ne '';
$ver_str .= "-$site_ver" if $site_ver;

print "Found EPICS Version $ver_str\n";

$epicsVersion="$outdir/epicsVersion.h";

mkdir ($outdir, 0777)  unless -d $outdir;

open OUT, "> $epicsVersion" or die "Cannot create $epicsVersion: $!\n";

print OUT "#define EPICS_VERSION        $ver\n";
print OUT "#define EPICS_REVISION       $rev\n";
print OUT "#define EPICS_MODIFICATION   $mod\n";
print OUT "#define EPICS_PATCH_LEVEL    $patch\n";
print OUT "#define EPICS_CVS_SNAPSHOT   \"$snapshot\"\n";
print OUT "#define EPICS_SITE_VERSION   \"$site_ver\"\n";
print OUT "#define EPICS_VERSION_STRING \"EPICS $ver_str\"\n";
print OUT "#define epicsReleaseVersion \"EPICS R$ver_str $cvs_tag $cvs_date\"\n";
print OUT "\n";
print OUT "/* EPICS_UPDATE_LEVEL is deprecated, use EPICS_PATCH_LEVEL instead */\n";
print OUT "#define EPICS_UPDATE_LEVEL    $patch\n";

close OUT;

