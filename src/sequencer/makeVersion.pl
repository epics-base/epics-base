#!/usr/local/bin/perl 
#
# makeVersion - create the snc version module
#
# Usage: perl makeVersion.pl {Version_file} {Symbol Name}

$version_file = $ARGV[0];
$symbol = $ARGV[1];
$out = "$symbol.c";

open IN, $version_file  or  die "Cannot open $version_file";
$version=<IN>;
chomp $version;
close IN;

$date = localtime();

open OUT, ">$out"  or  die "Cannot create $out";
print OUT "/* $out - version & date */\n";
print OUT "/* Created by makeVersion.pl */\n";
print OUT "char *$symbol = \"\@(#)SNC/SEQ Version $version : $date\";\n";
close OUT;
