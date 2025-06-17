#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2025 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Tool to combine release note entries

use strict;
use warnings;

use File::Basename;
use Getopt::Std;
use version;

my $tool = basename($0);

our ($opt_d, $opt_h, $opt_o, $opt_D, $opt_V);

sub HELP_MESSAGE {
    print STDERR "Usage: $tool -o outfile.md -V 7.0.10 [-d new-notes] [-D] " .
        "RELEASE-*.md\n";
    exit 2;
}

HELP_MESSAGE()
    if !getopts('hd:o:DV:') || $opt_h;

die "$tool: Release version '-V' option required\n"
    unless defined $opt_V;
die "$tool: Version string '$opt_V' not legal\n"
    unless $opt_V =~ m/^ (0 | [1-9][0-9]*) (\. [0-9]{1,3}){0,3} $/x;
die "$tool: Output filename '-o' required\n"
    unless defined $opt_o;
die "$tool: Output filename '-o $opt_o' must end with '.md'\n"
    unless $opt_o =~ m/\.md $/x;
die "$tool: New notes directory '-d $opt_d' doesn't exist\n"
    if defined $opt_d && !-d $opt_d;

open my $out, '>:encoding(UTF-8)', $opt_o or
    die "$tool: Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

my $REL_VERS = $opt_V;
$REL_VERS .= '-DEV' if $opt_D;

my @notes;
if ($opt_d) {
    # Directory handle for scanning the new-notes directory
    opendir my $dh, $opt_d or
        die "$tool: Can't open '-d' directory: $!\n";

    foreach my $fn (sort grep !m/^ \. \.? $/x, readdir $dh) {
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
}

sub relVers {
    $_ = shift;
    m/RELEASE-([0-9.]+)\.md/;
    return $1;
}

# Reverse sort of the RELEASE-<version>.md filenames
my @OLD_REL_FILES = sort {
    version->parse(relVers($b)) <=> version->parse(relVers($a));
} @ARGV;

print $out <<"__REL_INTRO__";
# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but the entries describing changes in those submodules since version
7.0.5 have been copied into the associated EPICS Release Notes files; they will
also be manually added to new EPICS Release Notes published in the future.


## EPICS Release $REL_VERS

__REL_INTRO__

print $out <<"__NEW_INTRO__" if $opt_D && scalar @notes;
__This version of EPICS has not been released yet.__
__When the changes described below get released, the version number used may be
different to the one given above.__

The changes below have been merged into EPICS since the last published release.

__NEW_INTRO__

# Include the text from all the new entries
print $out map { "$_\n" } @notes;

# Add myst include directives to incorporate older release files
foreach my $rfile (@OLD_REL_FILES) {
    my $ver = relVers($rfile);
    if ($ver =~ m/^7\./) {
        print $out <<"__OLD_RELEASE_7__";
-----

## EPICS Release $ver

```{include} ../$rfile
:start-after: EPICS Release $ver
```

__OLD_RELEASE_7__
    }
    elsif ($ver =~ m/^3\.1[56]/) {
        print $out <<"__OLD_RELEASE_3__";
-----

```{include} ../$rfile
:heading-offset: 1
```

__OLD_RELEASE_3__
    }
}

close $out;

