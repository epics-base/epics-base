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

use File::Basename;

sub Usage
{
	my ($txt) = @_;

	print "Usage:\n";
	print "\tmakeIncludeDbd.pl infile1 [ infile2 infile3 ...] outfile\n";
	print "\nError: $txt\n" if $txt;

	exit 2;
}

# need at least two args: ARGV[0] and ARGV[1]
Usage("\"makeIncludeDbd.pl @ARGV\": No input files specified") if $#ARGV < 1;

$target=$ARGV[$#ARGV];
@sources=@ARGV[0..$#ARGV-1];

open(OUT, "> $target") or die "Cannot create $target\n";;
foreach $file ( @sources )
{
	$base=basename($file);
	print OUT "include \"$base\"\n";
}

close OUT;

#   EOF makeIncludeDbd.pl

