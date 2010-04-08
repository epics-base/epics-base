#!/usr/bin/perl

use Test::More tests => 18;

use DBD;

my $dbd = DBD->new;
isa_ok $dbd, 'DBD';

is keys %{$dbd->breaktables}, 0, 'No breaktables yet';
my $reg = DBD::Breaktable->new('Brighton');
$dbd->add($reg);
my %regs = $dbd->breaktables;
is_deeply \%regs, {Brighton => $reg}, 'Added breaktable';

is keys %{$dbd->drivers}, 0, 'No drivers yet';
my $reg = DBD::Driver->new('Danforth');
$dbd->add($reg);
my %regs = $dbd->drivers;
is_deeply \%regs, {Danforth => $reg}, 'Added driver';

is keys %{$dbd->functions}, 0, 'No functions yet';
my $reg = DBD::Function->new('Frank');
$dbd->add($reg);
my %regs = $dbd->functions;
is_deeply \%regs, {Frank => $reg}, 'Added function';

is keys %{$dbd->menus}, 0, 'No menus yet';
my $menu = DBD::Menu->new('Mango');
$dbd->add($menu);
my %menus = $dbd->menus;
is_deeply \%menus, {Mango => $menu}, 'Added menu';
is $dbd->menu('Mango'), $menu, 'Named menu';

is keys %{$dbd->recordtypes}, 0, 'No recordtypes yet';
my $rtyp = DBD::Recordtype->new('Rita');
$dbd->add($rtyp);
my %rtypes = $dbd->recordtypes;
is_deeply \%rtypes, {Rita => $rtyp}, 'Added recordtype';
is $dbd->recordtype('Rita'), $rtyp, 'Named recordtype';

is keys %{$dbd->registrars}, 0, 'No registrars yet';
my $reg = DBD::Registrar->new('Reggie');
$dbd->add($reg);
my %regs = $dbd->registrars;
is_deeply \%regs, {Reggie => $reg}, 'Added registrar';

is keys %{$dbd->variables}, 0, 'No variables yet';
my $ivar = DBD::Variable->new('IntVar');
my $dvar = DBD::Variable->new('DblVar', 'double');
$dbd->add($ivar);
my %vars = $dbd->variables;
is_deeply \%vars, {IntVar => $ivar}, 'First variable';
$dbd->add($dvar);
%vars = $dbd->variables;
is_deeply \%vars, {IntVar => $ivar, DblVar => $dvar}, 'Second variable';

