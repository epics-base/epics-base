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
#

print "Building epicsVersion.h from CONFIG_BASE_VERSION\n";

die unless $#ARGV==0;

open VARS, $ARGV[0] or die "Cannot get variables from $ARGV[0]";

while (<VARS>)
{
	s/\s+$//;  # remove trailing white space and carriage return
	if (/EPICS_VERSION=(.*)/)	{ $ver = $1; }
	if (/EPICS_REVISION=(.*)/)	{ $rev = $1; }
	if (/EPICS_MODIFICATION=(.*)/)	{ $mod = $1; }
	if (/EPICS_UPDATE_NAME=(.*)/)	{ $upd_name = $1; }
	if (/EPICS_UPDATE_LEVEL=(.*)/)	{ $upd_level = $1; }
	if (/CVS_DATE="\\(.*)"/)	{ $cvs_date = $1; }
}

$ver_str = "$ver.$rev.$mod";
$ver_str = "$ver_str.$upd_name" if $upd_name;
$ver_str = "$ver_str.$upd_level" if $upd_level;

print "Found EPICS Version $ver_str\n";

open OUT, ">epicsVersion.h";

print OUT "#define BASE_VERSION        $ver\n";
print OUT "#define BASE_REVISION       $rev\n";
print OUT "#define BASE_MODIFICATION   $mod\n";
print OUT "#define BASE_UPDATE_NAME    $upd_name\n";
print OUT "#define BASE_UPDATE_LEVEL   $upd_level\n";
print OUT "#define BASE_VERSION_STRING \"EPICS Version $ver_str\"\n";
print OUT "#define epicsReleaseVersion \"@(#)Version R$ver_str $cvs_date\"\n";

# EPICS_* defs are only for backward compatibility. 
# They will be removed at some future date.
print OUT "#define EPICS_VERSION        $ver\n";
print OUT "#define EPICS_REVISION       $rev\n";
print OUT "#define EPICS_MODIFICATION   $mod\n";
print OUT "#define EPICS_UPDATE_NAME    $upd_name\n";
print OUT "#define EPICS_UPDATE_LEVEL   $upd_level\n";
print OUT "#define EPICS_VERSION_STRING \"EPICS Version $ver_str\"\n";

close OUT;


