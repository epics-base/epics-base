package DBD::Recordtype;
use DBD::Util;
@ISA = qw(DBD::Util);

use Carp;

sub init {
	my $this = shift;
	$this->SUPER::init(@_);
        $this->{FIELDS} = [];		# Ordered list
	$this->{FIELD_INDEX} = {};	# Indexed by name
	$this->{DEVICES} = [];		# Ordered list
	$this->{DEVICE_INDEX} = {};	# Indexed by choice
        return $this;
}

sub add_field {
	my ($this, $field) = @_;
	confess "Not a DBD::Recfield" unless $field->isa('DBD::Recfield');
	my $field_name = $field->name;
	dieContext("Duplicate field name '$field_name'")
		if exists $this->{FIELD_INDEX}->{$field_name};
	$field->check_valid;
	push $this->{FIELDS}, $field;
	$this->{FIELD_INDEX}->{$field_name} = $field;
}

sub fields {
	return shift->{FIELDS};
}

sub field_names { # In their original order...
	my $this = shift;
	my @names = ();
	foreach ($this->fields) {
		push @names, $_->name
	}
	return @names;
}

sub field {
	my ($this, $field) = @_;
	return $this->{FIELD_INDEX}->{$field};
}

sub add_device {
	my ($this, $device) = @_;
	confess "Not a DBD::Device" unless $device->isa('DBD::Device');
	my $choice = $device->choice;
	if (exists $this->{DEVICE_INDEX}->{$choice}) {
		my @warning = ("Duplicate device type '$choice'");
		my $old = $this->{DEVICE_INDEX}->{$choice};
		push @warning, "Link types differ"
			if ($old->link_type ne $device->link_type);
		push @warning, "DSETs differ"
			if ($old->name ne $device->name);
		warnContext @warning;
		return;
	}
	$device->check_valid;
	push $this->{DEVICES}, $device;
	$this->{DEVICE_INDEX}->{$choice} = $device;
}

sub devices {
	my $this = shift;
	return $this->{DEVICES};
}

sub device {
	my ($this, $choice) = @_;
	return $this->{DEVICE_INDEX}->{$choice};
}

1;
