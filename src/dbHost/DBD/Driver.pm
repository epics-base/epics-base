package DBD::Driver;
use DBD::Util;
@ISA = qw(DBD::Util);

sub init {
    return shift->SUPER::init(shift, "driver entry table name");
}

1;
