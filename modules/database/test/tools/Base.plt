#!/usr/bin/perl
######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

use lib '@TOP@/lib/perl';

use Test::More tests => 127;

use DBD::Base;
use DBD::Registrar;

note "*** Testing DBD::Base class ***";

my $base = DBD::Base->new('test', 'Base class');
isa_ok $base, 'DBD::Base';
is $base->what, 'Base class', 'DBD Object type';
is $base->name, 'test', 'Base class name';

my $base2 = DBD::Base->new('test2', 'Base class');
isa_ok $base, 'DBD::Base';
ok !$base->equals($base2), 'Different names';

my $reg = DBD::Registrar->new('test');
ok !$base->equals($reg), 'Different types';

eval {
    $base->add_comment('testing');
};
ok $@, 'add_comment died';

{
    local *STDERR;
    my $warning = '';
    open STDERR, '>', \$warning;
    $base->add_pod('testing');
    like $warning, qr/^Warning:/, 'add_pod warned';
    # Also proves that warnContext works
}

note "*** Testing push/pop contexts ***";
pushContext "a";
pushContext "b";
eval {
    popContext "b";
};
ok !$@, "pop: Expected context didn't die";

eval {
    popContext "b";
};
ok $@, "pop: Incorrect context died";
# Also proves that dieContext dies properly

note "*** Testing basic RXs ***";

# For help in debugging regex's, wrap tests below inside
#   use re 'debugcolor';
#   ...
#   no re;

like($_, qr/^ $RXident $/x, "Good RXident: $_")
    foreach qw(a A1 a111 z9 Z9 Z_999);
unlike($_, qr/^ $RXident $/x, "Bad RXident: $_")
    foreach qw(. 1 _ : a. _: 9.0);

like($_, qr/^ $RXname $/x, "Good RXname: $_")
    foreach qw(a A1 a1:x _ -; Z[9] Z<999> a{x}b);
unlike($_, qr/^ $RXname $/x, "Bad RXname: $_")
    foreach qw({x} a{x} {x}b @A 9.0% $x);

like($_, qr/^ $RXhex $/x, "Good RXhex: $_")
    foreach qw(0x0 0XA 0xAf 0x99 0xfedbca987654321 0XDEADBEEF);
unlike($_, qr/^ $RXhex $/x, "Bad RXhex: $_")
    foreach qw(1 x1 0123 0b1010101 -0x12345);

like($_, qr/^ $RXoct $/x, "Good RXoct: $_")
    foreach qw(0 01 07 077 0777 00001 010101 01234567);
unlike($_, qr/^ $RXoct $/x, "Bad RXoct: $_")
    foreach qw(1 08 018 0f 0x777 00009 0b1010101);

like($_, qr/^ $RXuint $/x, "Good RXuint: $_")
    foreach qw(0 01 1 9 999 00001 987654321);
unlike($_, qr/^ $RXuint $/x, "Bad RXuint: $_")
    foreach qw(-1 0x1 -9 0xf 1.0 1e3 -0x9 0b1010101);

like($_, qr/^ $RXint $/x, "Good RXint: $_")
    foreach qw(0 1 9 -09 999 -90909 00001 010101 123456789);
unlike($_, qr/^ $RXint $/x, "Bad RXint: $_")
    foreach qw(0f 0-1 0x777 1.0 1e30 fedcba 0b1010101);

like($_, qr/^ $RXnum $/x, "Good RXnum: $_")
    foreach qw(0 01 0.1 .9 -.9 9.0 -1e2 0.1e+1 .1e1 -.1e1 -1.1E-1 3.1415926535);
unlike($_, qr/^ $RXnum $/x, "Bad RXnum: $_")
    foreach qw(0f 0-1 e1 1.e1 1.x -e2 1e3-0 +1 0b1010101);

# All '\' chars must be doubled inside qr()
like($_, qr/^ $RXdqs $/x, "Good RXdqs: $_")
    foreach qw("" "a" "\\"" "\\\\" "\\'" "\\x" "\\\\\\"" "\\"\\\\\\"");
unlike($_, qr/^ $RXdqs $/x, "Bad RXdqs: $_")
    foreach qw(x 'x' "x\\" "x\\"x\\");
