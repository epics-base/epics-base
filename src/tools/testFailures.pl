#!/usr/bin/env perl
#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

# This file lets the build system fail a top-level 'make test-results'
# target with output showing the directories where test failures were
# reported and the test programs that failed there.
#
# The exit status of this program is 1 (failure) if any tests failed,
# otherwise 0 (success).

use strict;
use warnings;

use File::Basename;

die "Usage: testFailures.pl /path/to/base/.tests-failed\n"
    unless @ARGV == 1;

my $path = shift;
my $base = dirname($path);

open(my $failures, '<', $path) or
    exit 0;
my @failures = dedup(<$failures>);
close $failures;
chomp @failures;

exit 0 unless @failures;

print "\nTests failed:\n";
for my $dir (@failures) {
    my $reldir = $dir;
    $reldir =~ s($base/)();
    print "    In $reldir:\n";
    open(my $taps, '<', "$dir/.taps-failed") or next;
    my @taps = dedup(<$taps>);
    close $taps;
    chomp @taps;
    print '', (map {"        $_\n"} @taps), "\n";
}

exit 1;

sub dedup {
    my %dedup;
    $dedup{$_}++ for @_;
    return sort keys %dedup;
}
