#!/usr/bin/perl
#
#	UNIX-mkdir in Perl
#
#	-p option generates full path to given dir

use File::Path;
use Getopt::Std;
getopt('');

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
