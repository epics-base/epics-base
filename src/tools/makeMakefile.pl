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
#	makeMakefile.pl
#
#	called from RULES_ARCHS
#
#
#	Usage: perl makeMakefile.pl O.*-dir Makefile-Type

$dir = $ARGV[0];
$type= $ARGV[1];
$makefile="$dir/Makefile";

if ($dir =~ m'O.(.+)')
{
	$t_a = $1;
}
else
{
	die "Cannot extract T_A from $dir";
}

mkdir ($dir, 0777)  unless -d $dir;

open OUT, "> $makefile"  or die "Cannot create $makefile";
print OUT "T_A=$t_a\n";
print OUT "BUILD_TYPE=$type\n";
print OUT "include ../Makefile.$type\n";
close OUT;

#	EOF makeMakefile.pl
