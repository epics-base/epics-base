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

our ($opt_h, $opt_v);
our $opt_t = '.';
our $opt_N = 'VCSVERSION';
our $opt_V = strftime($tfmt, localtime);

my $foundvcs = 0;
my $result;

getopts('hvt:N:V:') && @ARGV == 1
    or HELP_MESSAGE();

my ($outfile) = @ARGV;

if (!$foundvcs && -d "$opt_t/_darcs") { # Darcs
    # v1-4-dirty
    # is tag 'v1' plus 4 patches
    # with uncommited modifications
    $result = `cd "$opt_t" && echo "\$(darcs show tags | head -1)-\$((\$(darcs changes --count --from-tag .)-1))"`;
    chomp $result;
    if (!$? && length($result) > 1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `darcs whatsnew --repodir="$opt_t" -l`;
        $opt_V .= '-dirty' unless $?;
    }
}
if (!$foundvcs && -d "$opt_t/.hg") { # Mercurial
    # v1-4-abcdef-dirty
    # is 4 commits after tag 'v1' with short hash abcdef
    # with uncommited modifications
    $result = `hg --cwd "$opt_t" tip --template '{latesttag}-{latesttagdistance}-{node|short}\n'`;
    chomp $result;
    if (!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `hg --cwd "$opt_t" status -m -a -r -d`;
        chomp $hasmod;
        $opt_V .= '-dirty' if length($hasmod) > 0;
    }
}
if (!$foundvcs && -d "$opt_t/.git") {
    # same format as Mercurial
    $result = `git --git-dir="$opt_t/.git" describe --tags --dirty`;
    chomp $result;
    if (!$? && length($result) > 1) {
        $opt_V = $result;
        $foundvcs = 1;
    }
}
if (!$foundvcs && -d "$opt_t/.svn") {
    # 12345
    $result = `cd "$opt_t" && svn info --non-interactive`;
    chomp $result;
    if (!$? && $result =~ /^Revision:\s*(\d+)/m) {
        $opt_V = $1;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `cd "$opt_t" && svn status -q --non-interactive`;
        chomp $hasmod;
	$opt_V .= '-dirty' if length($hasmod) > 0;
    }
}
if (!$foundvcs && -d "$opt_t/.bzr") {
    # 12444-anj@aps.anl.gov-20131003210403-icfd8mc37g8vctpf
    $result = `cd "$opt_t" && bzr version-info -q --custom --template="{revno}-{revision_id}"`;
    chomp $result;
    if (!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        # unfortunately "bzr version-info --check-clean ..." doesn't seem to work as documented
        my $hasmod = `cd "$opt_t" && bzr status -SV`;
        chomp $hasmod;
	$opt_V .= '-dirty' if length($hasmod) > 0;
    }
}

my $output = << "__END";
/* Generated file, do not edit! */

#ifndef $opt_N
#  define $opt_N \"$opt_V\"
#endif
__END
print "== would\n$output" if $opt_v;

my $DST;
if (open($DST, '+<', $outfile)) {
    my $actual = join('', <$DST>);
    print "== have\n$actual" if $opt_v;

    if ($actual eq $output) {
        print "Keeping existing VCS version header $outfile with \"$opt_V\"\n";
        exit 0;
    }
    print "Updating VCS version header $outfile with \"$opt_V\"\n";
} else {
    print "Creating VCS version header $outfile with \"$opt_V\"\n";
    open($DST, '>', $outfile)
        or die "Unable to open or create VCS version header $outfile";
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
    genVersionHeader.pl [-v] [-t top] [-N NAME] [-V version] output.h";
        Generate or update the header file output.h
        -t top     - Path to the module's top (default '$opt_t')
        -N NAME    - Macro name to be defined (default '$opt_N')
        -v version - Version number if no VCS (default '$opt_V')
EOF
    exit $opt_h ? 0 : 1;
}

