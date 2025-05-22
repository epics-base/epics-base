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
use version;

my $tool = basename($0);

our ($opt_d, $opt_h, $opt_o, $opt_D, $opt_V);

sub HELP_MESSAGE {
    print STDERR "Usage: $tool -d new-notes -o outfile.md -D " .
        "-V 7.0.10 RELEASE-7.0.9.md ...\n";
    exit 2;
}

HELP_MESSAGE()
    if !getopts('hd:o:DV:') || $opt_h || (!$opt_D && @ARGV > 0);

die "$tool: Directory from '-d' option doesn't exist\n"
    unless -d $opt_d;
die "$tool: Output filename is '$opt_o' but must end with '.md'"
    unless $opt_o =~ m/\.md$/;

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

sub relVers {
    $_ = shift;
    m/RELEASE-([0-9.]+)\.md/;
    return $1;
}

# Reverse sort of the RELEASE-<version>.md filenames
my @OLD_RELS = sort {
    version->parse(relVers($b)) <=> version->parse(relVers($a));
} @ARGV;

print $out <<"__REL_INTRO__";
# Release Notes

This document describes changes that have been included in this release of
EPICS.
Notes from earlier EPICS releases are now provided in separate documents for
each version in the EPICS 7 series to date, linked below.
Release documents are also included for the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but older entries describing changes in those submodules since
version 7.0.5 have been copied into the appropriate release notes files linked
below, and will be added to new EPICS Release Notes published in the future.

__REL_INTRO__

print $out <<"__NEW_INTRO__" if $opt_D && scalar @notes;

## EPICS Release $REL_VERS

__This version of EPICS has not been released yet.__
__When the changes described below get released, the version number used may be
different to the one given above.__

The changes below have been merged into EPICS since the last published release.

__NEW_INTRO__

print $out map { "$_\n" } @notes, "-----\n\n";

print $out "```{toctree}\n",
    map( sprintf("RELEASE-%s.md\n", relVers($_)), @OLD_RELS),
    "```\n\n";

close $out;

