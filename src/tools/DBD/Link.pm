package DBD::Link;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    my ($this, $name, $lset) = @_;
    $this->SUPER::init($lset, "link support (lset)");
    $this->{KEY} = $name;
    return $this;
}

sub key {
    return shift->{KEY};
}

sub equals {
    my ($a, $b) = @_;
    return $a->SUPER::equals($b)
        && $a->{KEY} eq $b->{KEY};
}

1;
