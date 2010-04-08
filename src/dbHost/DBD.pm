package DBD;

use DBD::Breaktable;
use DBD::Device;
use DBD::Driver;
use DBD::Menu;
use DBD::Recordtype;
use DBD::Recfield;
use DBD::Registrar;
use DBD::Function;
use DBD::Variable;

sub new {
        my $proto = shift;
        my $class = ref($proto) || $proto;
        my $this = {
		BREAKTABLES => {},
		DEVICES     => {},
		DRIVERS     => {},
		MENUS       => {},
		RECORDTYPES => {},
		REGISTRARS  => {},
		FUNCTIONS   => {},
		VARIABLES   => {}
	};
        bless $this, $class;
	return $this;
}

sub add_breaktable {
}

sub add_driver {
}

sub add_menu {
}

sub add_recordtype {
}

sub add_registrar {
}

sub add_function {
}

sub add_variable {
	my ($this, $obj) = @_;
	confess "Not a DBD::Variable" unless $obj->isa('DBD::Variable');
	my $obj_name = $obj->name;
	dieContext("Duplicate variable '$obj_name'")
		if exists $this->{VARIABLES}->{$obj_name};
	$this->{VARIABLES}->{$obj_name} = $obj;
}

1;
