#!/usr/bin/perl
 
use Cwd;
use File::Basename;

sub Usage
{
	my ($txt) = @_;

	print "\nError: $txt\n" if $txt;

	exit 2;
}

# need at least one args: ARGV[0]
Usage("need more args") if $#ARGV < 0;

$target=$ARGV[0];

$basename = basename($target,"");
$dirname = dirname($target,"");
if ($dirname eq ".") {
  print "$basename";
} else {
  $dir=cwd();
  chdir $dirname;
  $nativename=cwd();
  chdir $dir;
  $nativename="$nativename/$basename";
  print "$nativename";
}
