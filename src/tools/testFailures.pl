#!/usr/bin/env perl
#*************************************************************************
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

# This file may appear trivial, but it exists to let the build system
# fail the 'make test-results' target with a nice output including a
# summary of the directories where test failures were reported.
# Test results are collected from the .tap files fed to epicsProve.pl
# which returns with an exit status of 0 (success) if all tests passed
# or 1 (failure) if any of the .tap files contained failed tests.
# When epicsProve.pl indicates a failure, the directory that it was
# running in is appended to the file $(TOP)/.tests-failed which this
# program reads in after all the test directories have been visited.
# The exit status of this program is 1 (failure) if any tests failed,
# otherwise 0 (success).

use strict;
use warnings;

die "Usage: testFailures.pl .tests-failed\n"
    unless @ARGV == 1;

# No file means success.
open FAILURES, '<', shift or
    exit 0;
chomp(my @failures = <FAILURES>);
close FAILURES;

# A file with just empty lines also mean success
my @dirs = grep {$_} @failures;
exit 0 unless @dirs;

print "\nTest failures were reported in:\n",
    (map {"    $_\n"} @dirs), "\n\n";

exit 1;
