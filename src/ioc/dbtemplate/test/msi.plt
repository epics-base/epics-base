#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Script to run tests on the msi program

use FindBin qw($Bin);   # To find the msi executable

use strict;
use Test;

BEGIN {plan tests => 7}

ok(msi('-I .. ../t1-template.txt'),             slurp('../t1-result.txt'));
ok(msi('-I.. -S ../t2-substitution.txt'),       slurp('../t2-result.txt'));
ok(msi('-I. -I.. -S ../t3-substitution.txt'),   slurp('../t3-result.txt'));
ok(msi('-g -I.. -S ../t4-substitution.txt'),    slurp('../t4-result.txt'));
ok(msi('-S ../t5-substitute.txt ../t5-template.txt'), slurp('../t5-result.txt'));
ok(msi('-S ../t6-substitute.txt ../t6-template.txt'), slurp('../t6-result.txt'));

# Check -o works
my $out = 't7-output.txt';
unlink $out;
msi("-I.. -o $out ../t1-template.txt");
ok(slurp($out), slurp('../t1-result.txt'));


# Support routines

sub slurp {
    my ($file) = @_;
    open my $in, '<', $file
        or die "Can't open file $file: $!\n";
    my $contents = do { local $/; <$in> };
    return $contents;
}

sub msi {
    my ($args) = @_;
    my $arch = $ENV{EPICS_HOST_ARCH};
    my $exe = ($^O eq 'MSWin32') || ($^O eq 'cygwin') ? '.exe' : '';
    my $msi = "$Bin/../../../O.$arch/msi$exe";
    return `$msi $args`;
}
