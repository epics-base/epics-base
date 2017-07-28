#!/usr/bin/perl

use lib '../..';

use Test::More tests => 17;

use DBD::Recordtype;
use DBD::Recfield;
use DBD::Device;

my $rtyp = DBD::Recordtype->new('test');
isa_ok $rtyp, 'DBD::Recordtype';
is $rtyp->name, 'test', 'Record name';
is $rtyp->fields, 0, 'No fields yet';

my $fld1 = DBD::Recfield->new('NAME', 'DBF_STRING');
$fld1->add_attribute("size", "41");
$fld1->check_valid;

my $fld2 = DBD::Recfield->new('DTYP', 'DBF_DEVICE');
$fld2->check_valid;

$rtyp->add_field($fld1);
is $rtyp->fields, 1, 'First field added';

$rtyp->add_field($fld2);
is $rtyp->fields, 2, 'Second field added';

my @fields = $rtyp->fields;
is_deeply \@fields, [$fld1, $fld2], 'Field list';

my @names = $rtyp->field_names;
is_deeply \@names, ['NAME', 'DTYP'], 'Field name list';

is $rtyp->field('NAME'), $fld1, 'Field name lookup';

is $fld1->number, 0, 'Field number 0';
is $fld2->number, 1, 'Field number 1';

is $rtyp->devices, 0, 'No devices yet';

my $dev1 = DBD::Device->new('INST_IO', 'testDset', 'test device');
$rtyp->add_device($dev1);
is $rtyp->devices, 1, 'First device added';

my @devices = $rtyp->devices;
is_deeply \@devices, [$dev1], 'Device list';

is $rtyp->device('test device'), $dev1, 'Device name lookup';

is $rtyp->cdefs, 0, 'No cdefs yet';
$rtyp->add_cdef("cdef");
is $rtyp->cdefs, 1, 'First cdef added';

my @cdefs = $rtyp->cdefs;
is_deeply \@cdefs, ["cdef"], 'cdef list';
