package DBD::Function;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

sub init {
    return shift->SUPER::init(shift, "function");
}

1;
