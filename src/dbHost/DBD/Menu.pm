package DBD::Menu;
use DBD::Util;
@ISA = qw(DBD::Util);

sub init {
    my ($this, $name) = @_;
    $this->SUPER::init($name, "menu name");
    $this->{CHOICES} = [];
    $this->{CHOICE_INDEX} = {};
    return $this;
}

sub add_choice {
    my ($this, $name, $value) = @_;
    $name = identifier($name, "Choice name");
    $value = unquote($value, "Choice value");
    foreach $pair ($this->choices) {
    	dieContext("Duplicate choice name") if ($pair->[0] eq $name);
    	dieContext("Duplicate choice string") if ($pair->[1] eq $value);
    }
    push @{$this->{CHOICES}}, [$name, $value];
    $this->{CHOICE_INDEX}->{$value} = $name;
}

sub choices {
    return @{shift->{CHOICES}};
}

sub choice {
    my ($this, $idx) = @_;
    return $this->{CHOICES}[$idx];
}

sub legal_choice {
    my $this = shift;
    my $value = unquote(shift);
    return exists $this->{CHOICE_INDEX}->{$value};
}

1;
