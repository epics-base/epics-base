#!/usr/local/bin/perl
#
#	UNIX-mv in Perl

use File::Copy;

sub Usage
{
	my ($txt) = @_;

	print "Usage:\n";
	print "\tmv oldname newname\n";
	print "\tmv file [ file2 file3 ...] directory\n";
	print "\nError: $txt\n" if $txt;

	exit 2;
}

sub Move
{
	my ($src, $dest) = @_;

	print "Move($src, $dest)\n";

	copy ($src, $dest) or die "Cannot copy $src to $dest";
	unlink ($src) or die "Cannot remove $src";
}

#	return filename.ext from  Drive:/path/a/b/c/filename.ext
sub Filename
{
	my ($file) = @_;

	$file =~ s'.*[/\\]'';

	return $file;
}

# need at least two args: ARGV[0] and ARGV[1]
Usage("need more args") if $#ARGV < 1;

$target=$ARGV[$#ARGV];
@sources=@ARGV[0..$#ARGV-1];

print "move @sources into $target\n";

#	If target is (already existent) directory,
#	move files into it:
if (-d $target)
{
	foreach $file ( @sources )
	{
		Move ($file, "$target/" . Filename($file));
	}
	exit 0;
}

#	Otherwise the target is a filename.
#	Now 'mv' may be either a 'move' or a 'rename',
#	in any case it requires exactly two args: old and new name.

Usage("Need exactly one source") if $#sources != 0;
$source = @sources[0];

#	Move only if a simple rename 
#	fails (e.g. across file systems):
Move ($source, $target) unless (rename $source, $target);

# EOF mv.pl
