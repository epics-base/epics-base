#!/usr/bin/env perl

use lib '@TOP@/lib/perl';

use Test::More tests => 2;

use DBD::Function;

my $func = DBD::Function->new('test');
isa_ok $func, 'DBD::Function';
is $func->name, 'test', 'Function name';

