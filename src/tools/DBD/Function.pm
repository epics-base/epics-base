package DBD::Function;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    return shift->SUPER::init(shift, "function");
}

1;
