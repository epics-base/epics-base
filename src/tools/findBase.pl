#!/usr/local/bin/perl
 
use Cwd;
 
$dir=cwd();
#   make sure $dir ends with '/'
#
$dir="$dir/" unless ($dir =~ m'/$');
 
if ($dir =~ m'(.*(/|\\)base)(/|\\)')
{
    print "$1";
}
