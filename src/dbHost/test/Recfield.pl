#!/usr/bin/perl

use Test::More tests => 7;

use DBD::Recfield;

my $fld = DBD::Recfield->new('test', 'DBF_LONG');
isa_ok $fld, 'DBD::Recfield';
is $fld->name, 'test', 'Field name';
is $fld->dbf_type, 'DBF_LONG', 'Field type';
is keys %{$fld->attributes}, 0, 'No attributes';
ok $fld->legal_value("-1234"), 'Legal long value';
$fld->add_attribute("asl", "ASL0");
is keys %{$fld->attributes}, 1, "Attribute added";
$fld->check_valid;
is $fld->attribute("asl"), "ASL0", "Attribute value";
