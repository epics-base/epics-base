#!/usr/bin/perl
#
# This is a test director for running yajl JSON parser tests.
# The tests are actually defined in the yajlTestCases.pm module,
# which is generated from the yajl cases by yajlTestConverter.pl

use strict;
use Test::More;
use IO::Handle;
use IPC::Open3;

# Load test cases
use lib "..";
use yajlTestCases;

my @cases = cases();
plan tests => scalar @cases;

# The yajl_test program reads JSON from stdin and sends a description
# of what it got to stdout, with errors going to stderr. We merge the
# two output streams for the purpose of checking the test results.
my $prog = './yajl_test';
$prog .= '.exe' if ($^O eq 'MSWin32') || ($^O eq 'cygwin');

foreach my $case (@cases) {
    my $name  = $case->{name};
    my @opts  = @{$case->{opts}};
    my @input = @{$case->{input}};
    my @gives = @{$case->{gives}};

    my ($rx, $tx);
    my $pid = open3($tx, $rx, 0, $prog, @opts);

    # Send the test case, then EOF
    print $tx join "\n", @input;
    close $tx;

    # Receive the result
    my @result;
    while (!$rx->eof) {
        chomp(my $line = <$rx>);
        push @result, $line;
    }
    close $rx;

    # Clean up the child process
    waitpid $pid, 0;

    # Report the result of this test case
    is_deeply(\@result, \@gives, $name);
}
