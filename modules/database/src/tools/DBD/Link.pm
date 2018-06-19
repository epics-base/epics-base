package DBD::Link;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    my ($this, $name, $jlif) = @_;
    $this->SUPER::init($jlif, "link support (jlif)");
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
