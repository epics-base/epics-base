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
#	UNIX-cp in Perl

use File::Copy;
use File::Basename;

sub Usage
{
	my ($txt) = @_;

	print "Usage:\n";
	print "\tcp file1 file2\n";
	print "\tcp file [ file2 file3 ...] directory\n";
	print "\nError: $txt\n" if $txt;

	exit 2;
}

# need at least two args: ARGV[0] and ARGV[1]
Usage("need more args") if $#ARGV < 1;

$target=$ARGV[$#ARGV];
@sources=@ARGV[0..$#ARGV-1];

if (-d $target)
{
	foreach $file ( @sources )
	{
        $base=basename($file);
		copy ($file, "$target/$base");
	}
}
else
{
	Usage("Cannot copy more than one source into a single target")
		if ($#sources != 0);
	copy ($sources[0], $target);
}

# EOF cp.pl
