#!/usr/bin/env perl
#*************************************************************************
# SPDX-License-Identifier: EPICS
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

die "Usage: testFailures.pl /path/to/top/.tests-failed.log .taps-failed.log\n"
    unless @ARGV == 2;

my ($dirlog, $faillog) = @ARGV;
my $top = dirname($dirlog);

# No file means success.
open(my $logfile, '<', $dirlog) or
    exit 0;
my @faildirs = dedup(<$logfile>);
close $logfile;
chomp @faildirs;

# Empty file also means success.
exit 0 unless grep {$_} @faildirs;

print "\nTests failed in:\n";
for my $dir (@faildirs) {
    my $reldir = $dir;
    $reldir =~ s($top/)();
    print "    $reldir\n";
    open(my $taplog, '<', "$dir/$faillog") or next;
    my @taps = dedup(<$taplog>);
    close $taplog;
    chomp @taps;
    print '', (map {"        $_\n"} @taps), "\n";
}

exit 1;

sub dedup {
    my %dedup;
    $dedup{$_}++ for @_;
    return sort keys %dedup;
}
