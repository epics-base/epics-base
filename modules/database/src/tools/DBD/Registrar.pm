package DBD::Registrar;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

sub init {
    return shift->SUPER::init(shift, "registrar function");
}

1;
