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
#	UNIX-rm in Perl

use File::Path;
use File::Find;
use Getopt::Std;

getopt "";

foreach $arg ( @ARGV )
{
	next unless -e $arg;

	if (-d $arg)
	{
		if ($opt_r and $opt_f)
		{
			rmtree $arg;
		}
		else
		{
			rmdir ($arg) or die "Cannot delete $arg";
		}
        if (-d $arg)
        {
            die "Failed to delete $arg";
        }
	}
	else
	{
		unlink ($arg) or die "Cannot delete $arg";
	}
}

# EOF rm.pl
