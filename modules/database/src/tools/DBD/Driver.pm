package DBD::Driver;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    return shift->SUPER::init(shift, "driver support (drvet)");
}

1;
