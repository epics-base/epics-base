#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

use Getopt::Std;

our ($opt_o);

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
&HELP_MESSAGE if !getopts('o:') || @ARGV != 1;

my $infile = shift @ARGV;

if (!$opt_o) {
    ($opt_o = $infile) =~ s/\.pod$//;
    $opt_o =~ s/^.*\///;
}

open my $inp, '<', $infile or
    die "podRemove.pl: Can't open $infile: $!\n";
open my $out, '>', $opt_o or
    die "podRemove.pl: Can't create $opt_o: $!\n";

my $inPod = 0;
while (<$inp>) {
    if (m/\A=[a-zA-Z]/) {
        $inPod = !m/\A=cut/;
    }
    else {
        print $out $_ unless $inPod;
    }
}

close $out;
close $inp;

sub HELP_MESSAGE {
    print STDERR "Usage: podRemove.pl [-o file] file.pod\n";
    exit 2;
}
