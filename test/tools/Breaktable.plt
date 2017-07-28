#!/usr/bin/perl

use lib '../..';

use Test::More tests => 9;

use DBD::Breaktable;

my $bpt = DBD::Breaktable->new('test');
isa_ok $bpt, 'DBD::Breaktable';
is $bpt->name, 'test', 'Breakpoint table name';
is $bpt->points, 0, 'Points == zero';
$bpt->add_point(0, 0.5);
is $bpt->points, 1, 'First point added';
is_deeply $bpt->point(0), [0, 0.5], 'First point correct';
$bpt->add_point(1, 1.5);
is $bpt->points, 2, 'Second point added';
is_deeply $bpt->point(0), [0, 0.5], 'First point still correct';
is_deeply $bpt->point(1), [1, 1.5], 'Second point correct';
is_deeply $bpt->point(2), undef, 'Third point undefined';

