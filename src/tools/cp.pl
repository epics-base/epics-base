#!/usr/bin/perl
#
#	UNIX-cp in Perl

use File::Copy;

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
		copy ($file, "$target/$file");
	}
}
else
{
	Usage("Cannot copy more than one source into a single target")
		if ($#sources != 0);
	copy ($sources[0], $target);
}

# EOF cp.pl
