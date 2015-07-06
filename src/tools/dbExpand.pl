#!/usr/bin/env perl

#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use DBD::Output;
use EPICS::Getopts;
use EPICS::Readfile;
use EPICS::macLib;

our ($opt_D, @opt_I, @opt_S, $opt_o, $opt_V);

getopts('DI@S@o:V') or
    die "Usage: dbExpand [-D] [-I dir] [-S macro=val] [-o out.db] in.dbd in.db ...";

my @path = map { split /[:;]/ } @opt_I; # FIXME: Broken on Win32?
my $macros = EPICS::macLib->new(@opt_S);
my $dbd = DBD->new();

$macros->suppressWarning(!$opt_V);
$DBD::Record::macrosOk = !$opt_V;

# Calculate filename for the dependency warning message below
my $dep = $opt_o;
my $dot_d = '';
if ($opt_D) {
    $dep =~ s{\.\./O\.Common/(.*)}{$1\$\(DEP\)};
    $dot_d = '.d';
} else {
    $dep = "\$(COMMON_DIR)/$dep";
}

die "dbExpand.pl: No input files for $opt_o\n" if !@ARGV;

my $errors = 0;

while (@ARGV) {
    my $file = shift @ARGV;
    eval {
        &ParseDBD($dbd, &Readfile($file, $macros, \@opt_I));
    };
    if ($@) {
        warn "dbExpand.pl: $@";
        my $outfile = $opt_o ? " to create '$opt_o$dot_d'" : '';
        warn "  while reading '$file'$outfile\n";
        warn "  Your Makefile may need this dependency rule:\n",
            "    $dep: \$(COMMON_DIR)/$file\n"
            if $@ =~ m/Can't find file '$file'/;
        ++$errors;
    }
}

if ($opt_D) {   # Output dependencies only, ignore errors
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
    exit 0;
}

die "dbExpand.pl: Exiting due to errors\n" if $errors;

my $out;
if ($opt_o) {
    open $out, '>', $opt_o or die "Can't create $opt_o: $!\n";
} else {
    $out = *STDOUT;
}

&OutputDB($out, $dbd);

if ($opt_o) {
    close $out or die "Closing $opt_o failed: $!\n";
}
exit 0;
