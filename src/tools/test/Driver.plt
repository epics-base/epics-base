#!/usr/bin/perl

use lib '../..';

use Test::More tests => 2;

use DBD::Driver;

my $drv = DBD::Driver->new('test');
isa_ok $drv, 'DBD::Driver';
is $drv->name, 'test', 'Driver name';

