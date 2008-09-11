#!/usr/bin/perl

use strict;
use Test;

BEGIN {plan tests => 1}

my $prog = "./$0";
$prog =~ s/\.t$//;

my $expected =
    "1..11\n" .
    "ok  1 - testOk(1)\n" .
    "not ok  2 - testOk(0)\n" .
    "ok  3 - testPass()\n" .
    "not ok  4 - testFail()\n" .
    "ok  5 # SKIP Skipping two\n" .
    "ok  6 # SKIP Skipping two\n" .
    "ok  7 - Todo pass # TODO Testing Todo\n" .
    "not ok  8 - Todo fail # TODO Testing Todo\n" .
    "ok  9 # SKIP Todo skip\n" .
    "ok 10 - testOk1_success\n" .
    "not ok 11 - testOk1_failure\n" .
    "# Diagnostic\n";

$ENV{HARNESS_ACTIVE} = 1;
my $result = `$prog`;

ok($result, $expected); # test output matches
