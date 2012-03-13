package DBD;

use DBD::Base;
use DBD::Breaktable;
use DBD::Driver;
use DBD::Menu;
use DBD::Recordtype;
use DBD::Recfield;
use DBD::Registrar;
use DBD::Function;
use DBD::Variable;

use Carp;

sub new {
    my ($class) = @_;
    my $this = {
        'DBD::Breaktable' => {},
        'DBD::Driver'     => {},
        'DBD::Function'   => {},
        'DBD::Menu'       => {},
        'DBD::Recordtype' => {},
        'DBD::Registrar'  => {},
        'DBD::Variable'   => {}
    };
    bless $this, $class;
    return $this;
}

sub add {
    my ($this, $obj) = @_;
    my $obj_class;
    foreach (keys %{$this}) {
        next unless m/^DBD::/;
        $obj_class = $_ and last if $obj->isa($_);
    }
    confess "Unknown object type"
        unless defined $obj_class;
    my $obj_name = $obj->name;
    dieContext("Duplicate name '$obj_name'")
        if exists $this->{$obj_class}->{$obj_name};
    $this->{$obj_class}->{$obj_name} = $obj;
}

sub breaktables {
    return shift->{'DBD::Breaktable'};
}

sub drivers {
    return shift->{'DBD::Driver'};
}

sub functions {
    return shift->{'DBD::Function'};
}

sub menus {
    return shift->{'DBD::Menu'};
}
sub menu {
    my ($this, $menu_name) = @_;
    return $this->{'DBD::Menu'}->{$menu_name};
}

sub recordtypes {
    return shift->{'DBD::Recordtype'};
}
sub recordtype {
    my ($this, $rtyp_name) = @_;
    return $this->{'DBD::Recordtype'}->{$rtyp_name};
}

sub registrars {
    return shift->{'DBD::Registrar'};
}

sub variables {
    return shift->{'DBD::Variable'};
}

1;
