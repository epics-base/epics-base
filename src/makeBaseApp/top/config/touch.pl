#!/usr/bin/perl
#
#	unix touch in Perl

use File::Copy;
use File::Basename;

sub Usage
{
	my ($txt) = @_;

	print "Usage:\n";
	print "\ttouch file [ file2 file3 ...]\n";
	print "\nError: $txt\n" if $txt;

	exit 2;
}

# need at least one arg: ARGV[0]
Usage("need more args") if $#ARGV < 0;

@targets=@ARGV[0..$#ARGV];

foreach $file ( @targets )
{
    open(OUT,">$file") or die "$! creating $file";
    #print OUT "NUL\n";
    close OUT;
}

# EOF touch.pl
