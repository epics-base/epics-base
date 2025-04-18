#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2025 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Tool to combine release note entries into a single markdown file.

use strict;
use warnings;

use File::Basename;
use Getopt::Std;

my $tool = basename($0);

our ($opt_d, $opt_h, $opt_o, $opt_D, $opt_V);

sub HELP_MESSAGE {
    print STDERR "Usage: $tool -d new-notes -o outfile.md -D -V 7.0.10\n";
    exit 2;
}

HELP_MESSAGE()
    if !getopts('hd:o:DV:') || $opt_h || @ARGV > 0;

die "$tool: Directory from '-d' option doesn't exist\n"
    unless -d $opt_d;

open my $out, '>:encoding(UTF-8)', $opt_o or
    die "$tool: Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

my $REL_VERS = $opt_V;
$REL_VERS .= '-DEV' if $opt_D;

# Directory handle for scanning the new-notes directory
opendir my $dh, $opt_d or
    die "$tool: Can't open '-d' directory: $!\n";

my @notes;
foreach my $fn ( sort grep !/^\.\.?$/, readdir $dh ) {
    next if $fn eq 'README.txt';
    die "$tool: Not markdown? File '$fn' lacks '.md' extension\n"
        unless $fn =~ m/\.md$/;
    local $/;
    my $file = "$opt_d/$fn";
    push @notes, do {
        open my $fh, '<:encoding(UTF-8)', $file or
            die "$tool: Can't open file $file: $!\n";
        <$fh>;
    };
}
close $dh;

print $out <<__REL_INTRO__;
# Release Notes

This document describes the changes that have been made in this release of
EPICS.
Notes from earlier EPICS releases are now provided in a separate document for
each version in the EPICS 7 series to date.
Release documents are also included for the older Base 3.15 and 3.16 series.

The external PVA submodules continue to have their own release notes files as
before, but the entries describing changes in those submodules have been copied
into the appropriate EPICS release files since version 7.0.5 and will be added
in future releases.

__REL_INTRO__

print $out "## EPICS Release $REL_VERS\n\n";

print $out <<__NEW_INTRO__ if $opt_D;
__This version of EPICS has not been released yet.__
__The version number shown above may be different to the version number
that these changes eventually get released under.__

## Changes merged since the last release

__NEW_INTRO__

print $out map { "$_\n" } @notes;

close $out;

