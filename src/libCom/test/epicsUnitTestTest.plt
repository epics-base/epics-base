#!/usr/bin/perl

use strict;
use Test;

BEGIN {plan tests => 1}

my $prog = "./$0";
$prog =~ s/\.t$//;

my $expected = join '', <main::DATA>;

$ENV{HARNESS_ACTIVE} = 1;
my $result = `$prog`;

ok($result, $expected); # test output matches

__DATA__
1..11
ok  1 - testOk(1)
not ok  2 - testOk(0)
ok  3 - testPass()
not ok  4 - testFail()
ok  5 # SKIP Skipping two
ok  6 # SKIP Skipping two
ok  7 - Todo pass # TODO Testing Todo
not ok  8 - Todo fail # TODO Testing Todo
ok  9 # SKIP Todo skip
ok 10 - testOk1_success
not ok 11 - testOk1_failure
# Diagnostic
