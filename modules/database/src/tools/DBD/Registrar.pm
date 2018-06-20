package DBD::Registrar;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    return shift->SUPER::init(shift, "registrar function");
}

1;
