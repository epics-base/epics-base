package DBD::Variable;
use DBD::Base;
@ISA = qw(DBD::Base);

my %valid_types = (
    # C type name => corresponding iocshArg type identifier
    int => 'iocshArgInt',
    double => 'iocshArgDouble'
);

sub init {
    my ($this, $name, $type) = @_;
    $type = "int" unless defined $type;
    exists $valid_types{$type} or
        dieContext("Unknown variable type '$type', valid types are:",
            sort keys %valid_types);
    $this->SUPER::init($name, "variable");
    $this->{VAR_TYPE} = $type;
    return $this;
}

sub var_type {
    my $this = shift;
    return $this->{VAR_TYPE};
}

sub iocshArg_type {
    my $this = shift;
    return $valid_types{$this->{VAR_TYPE}};
}

sub equals {
    my ($a, $b) = @_;
    return $a->SUPER::equals($b)
        && $a->{VAR_TYPE} eq $b->{VAR_TYPE};
}

1;
