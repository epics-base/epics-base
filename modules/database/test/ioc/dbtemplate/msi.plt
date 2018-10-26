#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Script to run tests on the msi program

use strict;
use Test;

BEGIN {plan tests => 9}

# Check include/substitute command model
ok(msi('-I .. ../t1-template.txt'),             slurp('../t1-result.txt'));

# Substitution file, dbLoadTemplate format
ok(msi('-I.. -S ../t2-substitution.txt'),       slurp('../t2-result.txt'));

# Macro scoping
ok(msi('-I. -I.. -S ../t3-substitution.txt'),   slurp('../t3-result.txt'));

# Global scope (backwards compatibility check)
ok(msi('-g -I.. -S ../t4-substitution.txt'),    slurp('../t4-result.txt'));

# Substitution file, regular format
ok(msi('-S ../t5-substitute.txt ../t5-template.txt'), slurp('../t5-result.txt'));

# Substitution file, pattern format
ok(msi('-S../t6-substitute.txt ../t6-template.txt'), slurp('../t6-result.txt'));

# Output option -o and verbose option -V
my $out = 't7-output.txt';
my $count = 5; # Try up to 5 times...
my $result;
do {
    unlink $out;
    msi("-I.. -V -o $out ../t1-template.txt");
    $result = slurp($out);
    print "# msi output file empty, retrying\n"
        if $result eq '';
} while ($result eq '') && (--$count > 0);
ok($result, slurp('../t7-result.txt'));

# Dependency generation, include/substitute model
ok(msi('-I.. -D -o t8.txt ../t1-template.txt'), slurp('../t8-result.txt'));

# Dependency generation, dbLoadTemplate format
ok(msi('-I.. -D -ot9.txt -S ../t2-substitution.txt'), slurp('../t9-result.txt'));


# Test support routines

sub slurp {
    my ($file) = @_;
    open my $in, '<', $file
        or die "Can't open file $file: $!\n";
    my $contents = do { local $/; <$in> };
    return $contents;
}

sub msi {
    my ($args) = @_;
    my $msi = '@TOP@/bin/@ARCH@/msi';
    $msi .= '.exe' if ($^O eq 'MSWin32') || ($^O eq 'cygwin');
    my $result;
    if ($args =~ m/-o / && $args !~ m/-D/) {
        # An empty result is expected
        $result = `$msi $args`;
    }
    else {
        # Try up to 5 times, sometimes msi fails on Windows
        my $count = 5;
        do {
            $result = `$msi $args`;
            print "# result of '$msi $args' empty, retrying\n"
                if $result eq '';
        } while ($result eq '') && (--$count > 0);
    }
    return $result;
}
