#!/usr/bin/env perl
######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

use lib '@TOP@/lib/perl';

use Test::More tests => 4;

use DBD::Variable;

my $ivar = DBD::Variable->new('test');
isa_ok $ivar, 'DBD::Variable';
is $ivar->name, 'test', 'Variable name';
is $ivar->var_type, 'int', 'variable defaults to int';
my $dvar = DBD::Variable->new('test', 'double');
is $dvar->var_type, 'double', 'double variable';
