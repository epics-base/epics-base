#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;
use File::Basename;

sub Usage {
    my $txt = shift;

    print "Usage: makeIncludeDbd.pl input file list ... outfile\n";
    print "Error: $txt\n" if $txt;
    exit 2;
}

Usage("No input files specified")
    unless $#ARGV > 1;

my $target = pop @ARGV;
my @inputs = map { basename($_); } @ARGV;

open(my $OUT, '>', $target)
    or die "$0: Can't create $target, $!\n";

print $OUT "# Generated file $target\n\n";
print $OUT map { "include \"$_\"\n"; } @inputs;

close $OUT;
