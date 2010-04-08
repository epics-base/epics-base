package DBD::Recordtype;
use DBD::Base;
@ISA = qw(DBD::Base);

use Carp;

sub init {
    my $this = shift;
    $this->SUPER::init(@_);
    $this->{FIELD_LIST} = [];
    $this->{FIELD_INDEX} = {};
    $this->{DEVICE_LIST} = [];
    $this->{DEVICE_INDEX} = {};
    return $this;
}

sub add_field {
    my ($this, $field) = @_;
    confess "Not a DBD::Recfield" unless $field->isa('DBD::Recfield');
    my $field_name = $field->name;
    dieContext("Duplicate field name '$field_name'")
        if exists $this->{FIELD_INDEX}->{$field_name};
    $field->check_valid;
    $field->set_number(scalar @{$this->{FIELD_LIST}});
    push @{$this->{FIELD_LIST}}, $field;
    $this->{FIELD_INDEX}->{$field_name} = $field;
}

sub fields {
    return @{shift->{FIELD_LIST}};
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
    my ($this, $field_name) = @_;
    return $this->{FIELD_INDEX}->{$field_name};
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
        warnContext(@warning);
        return;
    }
    push @{$this->{DEVICE_LIST}}, $device;
    $this->{DEVICE_INDEX}->{$choice} = $device;
}

sub devices {
    my $this = shift;
    return @{$this->{DEVICE_LIST}};
}

sub device {
    my ($this, $choice) = @_;
    return $this->{DEVICE_INDEX}->{$choice};
}

sub toDeclaration {
    my $this = shift;
    my @fields = map {
        $_->toDeclaration
    } $this->fields;
    my $name = $this->name;
    $name .= "Record" unless $name eq "dbCommon";
    return "typedef struct $name {\n" .
               join("\n", @fields) .
           "\n} $name;\n";
}

1;
