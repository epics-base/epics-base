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
