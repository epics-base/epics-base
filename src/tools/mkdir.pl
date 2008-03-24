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
#	UNIX-mkdir in Perl
#
#	-p option generates full path to given dir

use File::Path;
use Getopt::Std;
getopt "";

foreach $dir ( @ARGV )
{
	if ($opt_p)
	{
		mkpath ($dir) or die "Cannot make directory $dir";
	}
	else
	{
		mkdir ($dir, 0777) or die "Cannot make directory $dir";
	}
}

# EOF mkdir.pl
