#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2014 Brookhaven National Laboratory.
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
use POSIX qw(strftime);

use strict;

# RFC 8601 date+time w/ zone (eg "2014-08-29T09:42:47-0700")
my $tfmt = '%Y-%m-%dT%H:%M:%S';
$tfmt .= '%z' unless $^O eq 'MSWin32'; # %z returns zone name on Windows
my $now = strftime($tfmt, localtime);

our ($opt_h, $opt_v, $opt_q);
our $opt_t = '.';
our $opt_N = 'VCSVERSION';
our $opt_V = $now;

my $vcs;

getopts('hvqt:N:V:') && @ARGV == 1
    or HELP_MESSAGE();

my ($outfile) = @ARGV;

if (!$vcs && -d "$opt_t/_darcs") { # Darcs
    print "== Found <top>/_darcs directory\n" if $opt_v;
    # v1-4-dirty
    # is tag 'v1' plus 4 patches
    # with uncommited modifications
    my $result = `cd "$opt_t" && echo "\$(darcs show tags | head -1)-\$((\$(darcs changes --count --from-tag .)-1))"`;
    chomp $result;
    print "== darcs show tags, changes:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $opt_V = $result;
        $vcs = 'Darcs';
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `darcs whatsnew --repodir="$opt_t" -l`;
        $opt_V .= '-dirty' unless $?;
    }
}
if (!$vcs && -d "$opt_t/.hg") { # Mercurial
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
}
if (!$vcs && -d "$opt_t/.git") { # Git
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
}
if (!$vcs && -d "$opt_t/.svn") { # Subversion
    print "== Found <top>/.svn directory\n" if $opt_v;
    # 12345-dirty
    my $result = `cd "$opt_t" && svn info --non-interactive`;
    chomp $result;
    print "== svn info:\n$result\n==\n" if $opt_v;
    if (!$? && $result =~ /^Revision:\s*(\d+)/m) {
        $opt_V = $1;
        $vcs = 'Subversion';
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `cd "$opt_t" && svn status --non-interactive`;
        chomp $hasmod;
        $opt_V .= '-dirty' if $hasmod ne '';
    }
}
if (!$vcs && -d "$opt_t/.bzr") { # Bazaar
    print "== Found <top>/.bzr directory\n" if $opt_v;
    # 12444-anj@aps.anl.gov-20131003210403-icfd8mc37g8vctpf-dirty
    my $result = `bzr version-info -q --custom --template="{revno}-{revision_id}-{clean}"`;
    print "== bzr version-info:\n$result\n==\n" if $opt_v;
    if (!$? && $result ne '') {
        $result =~ s/-([01])$/$1 ? '' : '-dirty'/e;
        $opt_V = $result;
        $vcs = 'Bazaar';
    }
}
if (!$vcs) {
    print "== No VCS directories\n" if $opt_v;
    if ($opt_V eq '') {
        $vcs = 'build date/time';
        $opt_V = $now;
    }
    else {
        $vcs = 'Makefile';
    }
}

my $output = << "__END";
/* Generated file, do not edit! */

/* Version determined from $vcs */

#ifndef $opt_N
  #define $opt_N \"$opt_V\"
#endif
__END

print "== Want:\n$output==\n" if $opt_v;

my $DST;
if (open($DST, '+<', $outfile)) {
    my $actual = join('', <$DST>);
    print "== Current:\n$actual==\n" if $opt_v;

    if ($actual eq $output) {
        print "Keeping VCS header $outfile\n    $opt_N = \"$opt_V\"\n"
            unless $opt_q;
        exit 0;
    }
    print "Updating VCS header $outfile\n    $opt_N = \"$opt_V\"\n"
        unless $opt_q;
} else {
    print "Creating VCS header $outfile\n    $opt_N = \"$opt_V\"\n"
        unless $opt_q;
    open($DST, '>', $outfile)
        or die "Can't create $outfile: $!\n";
}

seek $DST, 0, 0;
truncate $DST, 0;
print $DST $output;
close $DST;

sub HELP_MESSAGE {
    print STDERR <<EOF;
Usage:
    genVersionHeader.pl -h
        Display this Usage message
    genVersionHeader.pl [-v] [-q] [-t top] [-N NAME] [-V version] output.h";
        Generate or update the header file output.h
        -v         - Verbose (debugging messages)
        -q         - Quiet
        -t top     - Path to the module's top (default '$opt_t')
        -N NAME    - Macro name to be defined (default '$opt_N')
        -V version - Version if no VCS (e.g. '$opt_V')
EOF
    exit $opt_h ? 0 : 1;
}

