#!/usr/bin/env perl
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
#	Usage: perl makeMakefile.pl O.*-dir top Makefile-Type

$dir = $ARGV[0];
$top= $ARGV[1];
$type = $ARGV[2];
$makefile="$dir/Makefile";
$b_t="";

if ($type ne "")
{
	$b_t = "B_T=$type";
}

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

print OUT "#This Makefile created by makeMakefile.pl\n\n\n";
print OUT "all :\n";
print OUT "	\$(MAKE) -f ../Makefile$type TOP=$top T_A=$t_a $b_t \$@\n\n";
print OUT ".DEFAULT: force\n";
print OUT "	\$(MAKE) -f ../Makefile$type TOP=$top T_A=$t_a $b_t \$@\n\n";
print OUT "force:  ;\n";

close OUT;

#	EOF makeMakefile.pl

