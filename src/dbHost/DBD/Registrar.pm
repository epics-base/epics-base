package DBD::Registrar;
use DBD::Util;
@ISA = qw(DBD::Util);

sub init {
    return shift->SUPER::init(shift, "registrar function name");
}


1;

