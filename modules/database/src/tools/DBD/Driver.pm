package DBD::Driver;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

sub init {
    return shift->SUPER::init(shift, "driver support (drvet)");
}

1;
