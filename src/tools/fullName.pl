#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
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
