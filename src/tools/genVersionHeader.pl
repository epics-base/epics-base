#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2014 Brookhaven National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
#
# Generate a C header file which
# defines a macro with a string
# describing the VCS revision
#

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Getopts;
use EPICS::Path;
use POSIX qw(strftime);

use strict;

# Make sure that chomp removes all trailing newlines
$/='';

# RFC 8601 date+time w/ zone (eg "2014-08-29T09:42-0700")
my $tfmt = '%Y-%m-%dT%H:%M';
$tfmt .= '%z' unless $^O eq 'MSWin32'; # %z returns zone name on Windows
my $now = strftime($tfmt, localtime);

our ($opt_d, $opt_h, $opt_i, $opt_v, $opt_q);
our $opt_t = '.';
our $opt_N = 'VCSVERSION';
our $opt_V = $now;

our $RevDate = $now;

my $vcs;
my $cv;

getopts('dhivqt:N:V:') && @ARGV == 1
    or HELP_MESSAGE();

my ($outfile) = @ARGV;

if ($opt_d) { exit 0 } # exit if make is run in dry-run mode

# Save abs-path to output filename; chdir <top>
my $absfile = AbsPath($outfile);
chdir $opt_t or
    die "Can't cd to $opt_t: $!\n";

if (-d '_darcs') { # Darcs
    print "== Found <top>/_darcs directory\n" if $opt_v;
    # v1-4-dirty
    # is tag 'v1' plus 4 patches
    # with uncommited modifications
    my @tags = split('\n', `darcs show tags`);
    my $count = `darcs changes --count --from-tag .` - 1;
    my $result = $tags[0] . '-' . $count;
    print "== darcs show tags, changes:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $opt_V = $result;
        $vcs = 'Darcs';
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `darcs whatsnew --repodir="$opt_t" -l`;
        $opt_V .= '-dirty' unless $?;
    }
    $cv = `darcs log --last 1`;
    $cv =~ s/\A .* Date: \s+ ( [^\n]+ ) .* \z/$1/sx;
}
elsif (-d '.hg') { # Mercurial
    print "== Found <top>/.hg directory\n" if $opt_v;
    # v1-4-abcdef-dirty
    # is 4 commits after tag 'v1' with short hash abcdef
    # with uncommited modifications
    my $result = `hg tip --template '{latesttag}-{latesttagdistance}-{node|short}'`;
    print "== hg tip:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $opt_V = $result;
        $vcs = 'Mercurial';
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `hg status -m -a -r -d`;
        chomp $hasmod;
        $opt_V .= '-dirty' if $hasmod ne '';
    }
    $cv = `hg log -l1 --template '{date|isodate}'`
}
elsif (-d '.git') { # Git
    print "== Found <top>/.git directory\n" if $opt_v;
    # v1-4-abcdef-dirty
    # is 4 commits after tag 'v1' with short hash abcdef
    # with uncommited modifications
    my $result = `git describe --always --tags --dirty --abbrev=20`;
    chomp $result;
    print "== git describe:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $opt_V = $result;
        $vcs = 'Git';
    }
    $cv = `git show -s --format=%ci HEAD`;
}
elsif (-d '.svn') { # Subversion
    print "== Found <top>/.svn directory\n" if $opt_v;
    # 12345-dirty
    my $result = `svn info --non-interactive`;
    chomp $result;
    print "== svn info:\n$result\n==\n" if $opt_v;
    if (!$? && $result =~ /^Revision:\s*(\d+)/m) {
        $opt_V = $1;
        $vcs = 'Subversion';
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `svn status --non-interactive`;
        chomp $hasmod;
        $opt_V .= '-dirty' if $hasmod ne '';
    }
    $cv = `svn info --show-item last-changed-date`;
}
elsif (-d '.bzr') { # Bazaar
    print "== Found <top>/.bzr directory\n" if $opt_v;
    # 12444-anj@aps.anl.gov-20131003210403-icfd8mc37g8vctpf-dirty
    my $result = `bzr version-info -q --custom --template="{revno}-{revision_id}-{clean}"`;
    print "== bzr version-info:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $result =~ s/-([01])$/$1 ? '' : '-dirty'/e;
        $opt_V = $result;
        $vcs = 'Bazaar';
    }
    $cv = `bzr version-info -q --custom --template='{date}'`;
}
else {
    print "== No VCS directories\n" if $opt_v;
    if ($opt_V eq '') {
        $vcs = 'build date/time';
        $opt_V = $now;
    }
    else {
        $vcs = 'Makefile';
    }
}

chomp $cv;
$RevDate=$vcs . ': ' . $cv;

my $output = << "__END";
/* Generated file, do not edit! */

/* Version determined from $vcs */

#ifndef $opt_N
  #define $opt_N \"$opt_V\"
#endif
#ifndef ${opt_N}_DATE
  #define ${opt_N}_DATE \"$RevDate\"
#endif
__END

print "== Want:\n$output==\n" if $opt_v;

my $DST;
if (open($DST, '+<', $absfile)) {

    my $actual = join('', <$DST>);
    print "== Current:\n$actual==\n" if $opt_v;

    if ($actual eq $output) {
        close $DST;
        print "Keeping VCS header $outfile\n",
            "    $opt_N = \"$opt_V\"\n"
            unless $opt_q;
        exit 0;
    }

    # This regexp must match the #define in $output above:
    $actual =~ m/#define (\w+) ("[^"]*")\n/;
    if ($opt_i) {
        print "Outdated VCS header $outfile\n",
            "    has:   $1 = $2\n",
            "    needs: $opt_N = \"$opt_V\"\n";
    }
    else {
        print "Updating VCS header $outfile\n",
            "    from: $1 = $2\n",
            "    to:   $opt_N = \"$opt_V\"\n"
            unless $opt_q;
    }
} else {
    print "Creating VCS header $outfile\n",
        "    $opt_N = \"$opt_V\"\n"
        unless $opt_q;
    open($DST, '>', $absfile)
        or die "Can't create $outfile: $!\n";
}

if ($opt_i) { exit 1 }; # exit if make is run in "question" mode

seek $DST, 0, 0;
truncate $DST, 0;
print $DST $output;
close $DST;

sub HELP_MESSAGE {
    print STDERR <<EOF;
Usage:
    genVersionHeader.pl -h
        Display this Usage message
    genVersionHeader.pl [-v] [-d] [-q] [-t top] [-N NAME] [-V version] output.h";
        Generate or update the header file output.h
        -v         - Verbose (debugging messages)
        -d         - Dry-run
        -i         - Question mode
        -q         - Quiet
        -t top     - Path to the module's top (default '$opt_t')
        -N NAME    - Macro name to be defined (default '$opt_N')
        -V version - Version if no VCS (e.g. '$opt_V')
EOF
    exit $opt_h ? 0 : 1;
}
