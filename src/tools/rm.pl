#!/usr/bin/perl
#
#	UNIX-rm in Perl

use File::Path;
use File::Find;
use Getopt::Std;

getopt();

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
	}
	else
	{
		unlink ($arg) or die "Cannot delete $arg";
	}
}

# EOF rm.pl
