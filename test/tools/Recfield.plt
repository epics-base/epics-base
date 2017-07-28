#!/usr/bin/perl

use lib '../..';

use Test::More tests => 76;

use DBD::Recfield;

my $fld_string = DBD::Recfield->new('str', 'DBF_STRING');
isa_ok $fld_string, 'DBD::Recfield';
isa_ok $fld_string, 'DBD::Recfield::DBF_STRING';
$fld_string->set_number(0);
is $fld_string->number, 0, 'Field number';
$fld_string->add_attribute("size", "41");
is keys %{$fld_string->attributes}, 1, "Size set";
ok $fld_string->legal_value("Hello, world!"), 'Legal value';
ok !$fld_string->legal_value("x"x41), 'Illegal string';
$fld_string->check_valid;
like $fld_string->toDeclaration, qr/^\s*char\s+str\[41\];\s*$/, "C declaration";

my $fld_char = DBD::Recfield->new('chr', 'DBF_CHAR');
isa_ok $fld_char, 'DBD::Recfield';
isa_ok $fld_char, 'DBD::Recfield::DBF_CHAR';
is $fld_char->name, 'chr', 'Field name';
is $fld_char->dbf_type, 'DBF_CHAR', 'Field type';
ok !$fld_char->legal_value("-129"), 'Illegal - value';
ok $fld_char->legal_value("-128"), 'Legal - value';
ok $fld_char->legal_value("127"), 'Legal + value';
ok !$fld_char->legal_value("0x80"), 'Illegal + hex value';
$fld_char->check_valid;
like $fld_char->toDeclaration, qr/^\s*epicsInt8\s+chr;\s*$/, "C declaration";

my $fld_uchar = DBD::Recfield->new('uchr', 'DBF_UCHAR');
isa_ok $fld_uchar, 'DBD::Recfield';
isa_ok $fld_uchar, 'DBD::Recfield::DBF_UCHAR';
is $fld_uchar->name, 'uchr', 'Field name';
is $fld_uchar->dbf_type, 'DBF_UCHAR', 'Field type';
ok !$fld_uchar->legal_value("-1"), 'Illegal - value';
ok $fld_uchar->legal_value("0"), 'Legal 0 value';
ok $fld_uchar->legal_value("0377"), 'Legal + value';
ok !$fld_uchar->legal_value("0400"), 'Illegal + octal value';
$fld_uchar->check_valid;
like $fld_uchar->toDeclaration, qr/^\s*epicsUInt8\s+uchr;\s*$/, "C declaration";

my $fld_short = DBD::Recfield->new('shrt', 'DBF_SHORT');
isa_ok $fld_short, 'DBD::Recfield';
isa_ok $fld_short, 'DBD::Recfield::DBF_SHORT';
is $fld_short->name, 'shrt', 'Field name';
is $fld_short->dbf_type, 'DBF_SHORT', 'Field type';
ok !$fld_short->legal_value("-32769"), 'Illegal - value';
ok $fld_short->legal_value("-32768"), 'Legal - value';
ok $fld_short->legal_value("32767"), 'Legal + value';
ok !$fld_short->legal_value("0x8000"), 'Illegal + hex value';
$fld_short->check_valid;
like $fld_short->toDeclaration, qr/^\s*epicsInt16\s+shrt;\s*$/, "C declaration";

my $fld_ushort = DBD::Recfield->new('ushrt', 'DBF_USHORT');
isa_ok $fld_ushort, 'DBD::Recfield';
isa_ok $fld_ushort, 'DBD::Recfield::DBF_USHORT';
is $fld_ushort->name, 'ushrt', 'Field name';
is $fld_ushort->dbf_type, 'DBF_USHORT', 'Field type';
ok !$fld_ushort->legal_value("-1"), 'Illegal - value';
ok $fld_ushort->legal_value("0"), 'Legal 0 value';
ok $fld_ushort->legal_value("65535"), 'Legal + value';
ok !$fld_ushort->legal_value("0x10000"), 'Illegal + hex value';
$fld_ushort->check_valid;
like $fld_ushort->toDeclaration, qr/^\s*epicsUInt16\s+ushrt;\s*$/, "C declaration";

my $fld_long = DBD::Recfield->new('lng', 'DBF_LONG');
isa_ok $fld_long, 'DBD::Recfield';
isa_ok $fld_long, 'DBD::Recfield::DBF_LONG';
is $fld_long->name, 'lng', 'Field name';
is $fld_long->dbf_type, 'DBF_LONG', 'Field type';
ok $fld_long->legal_value("-12345678"), 'Legal - value';
ok $fld_long->legal_value("0x12345678"), 'Legal + value';
ok !$fld_long->legal_value("0xfigure"), 'Illegal value';
$fld_long->check_valid;
like $fld_long->toDeclaration, qr/^\s*epicsInt32\s+lng;\s*$/, "C declaration";

my $fld_ulong = DBD::Recfield->new('ulng', 'DBF_ULONG');
isa_ok $fld_ulong, 'DBD::Recfield';
isa_ok $fld_ulong, 'DBD::Recfield::DBF_ULONG';
is $fld_ulong->name, 'ulng', 'Field name';
is $fld_ulong->dbf_type, 'DBF_ULONG', 'Field type';
ok !$fld_ulong->legal_value("-1"), 'Illegal - value';
ok $fld_ulong->legal_value("00"), 'Legal 0 value';
ok $fld_ulong->legal_value("0xffffffff"), 'Legal + value';
ok !$fld_ulong->legal_value("0xfacepaint"), 'Illegal value';
$fld_ulong->check_valid;
like $fld_ulong->toDeclaration, qr/^\s*epicsUInt32\s+ulng;\s*$/, "C declaration";

my $fld_float = DBD::Recfield->new('flt', 'DBF_FLOAT');
isa_ok $fld_float, 'DBD::Recfield';
isa_ok $fld_float, 'DBD::Recfield::DBF_FLOAT';
is $fld_float->name, 'flt', 'Field name';
is $fld_float->dbf_type, 'DBF_FLOAT', 'Field type';
ok $fld_float->legal_value("-1.2345678e9"), 'Legal - value';
ok $fld_float->legal_value("0.12345678e9"), 'Legal + value';
ok !$fld_float->legal_value("0x1.5"), 'Illegal value';
$fld_float->check_valid;
like $fld_float->toDeclaration, qr/^\s*epicsFloat32\s+flt;\s*$/, "C declaration";

my $fld_double = DBD::Recfield->new('dbl', 'DBF_DOUBLE');
isa_ok $fld_double, 'DBD::Recfield';
isa_ok $fld_double, 'DBD::Recfield::DBF_DOUBLE';
is $fld_double->name, 'dbl', 'Field name';
is $fld_double->dbf_type, 'DBF_DOUBLE', 'Field type';
ok $fld_double->legal_value("-12345e-67"), 'Legal - value';
ok $fld_double->legal_value("12345678e+9"), 'Legal + value';
ok !$fld_double->legal_value("e5"), 'Illegal value';
$fld_double->check_valid;
like $fld_double->toDeclaration, qr/^\s*epicsFloat64\s+dbl;\s*$/, "C declaration";

