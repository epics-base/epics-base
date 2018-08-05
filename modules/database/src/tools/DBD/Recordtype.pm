package DBD::Recordtype;
use DBD::Base;
our @ISA = qw(DBD::Base);

use Carp;
use strict;

sub init {
    my ($this, $name) = @_;
    $this->SUPER::init($name, "record type");
    $this->{FIELD_LIST} = [];
    $this->{FIELD_INDEX} = {};
    $this->{DEVICE_LIST} = [];
    $this->{DEVICE_INDEX} = {};
    $this->{CDEFS} = [];
    $this->{COMMENTS} = [];
    $this->{POD} = [];
    return $this;
}

sub add_field {
    my ($this, $field) = @_;
    confess "DBD::Recordtype::add_field: Not a DBD::Recfield"
        unless $field->isa('DBD::Recfield');
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
    return map {$_->name} @{shift->{FIELD_LIST}};
}

sub field {
    my ($this, $field_name) = @_;
    return $this->{FIELD_INDEX}->{$field_name};
}

sub add_device {
    my ($this, $device) = @_;
    confess "DBD::Recordtype::add_device: Not a DBD::Device"
        unless $device->isa('DBD::Device');
    my $choice = $device->choice;
    if (exists $this->{DEVICE_INDEX}->{$choice}) {
        return if $device->equals($this->{DEVICE_INDEX}->{$choice});
        my @warning = ("Two $this->{NAME} device supports '$choice' conflict");
        my $old = $this->{DEVICE_INDEX}->{$choice};
        push @warning, "Link types differ"
            if $old->link_type ne $device->link_type;
        push @warning, "DSETs differ"
            if $old->name ne $device->name;
        dieContext(@warning);
    }
    push @{$this->{DEVICE_LIST}}, $device;
    $this->{DEVICE_INDEX}->{$choice} = $device;
}

sub devices {
    return @{shift->{DEVICE_LIST}};
}

sub device {
    my ($this, $choice) = @_;
    return $this->{DEVICE_INDEX}->{$choice};
}

sub add_comment {
    my ($this, $comment) = @_;
    push @{$this->{COMMENTS}}, $comment;
}

sub comments {
    return @{shift->{COMMENTS}};
}

sub add_cdef {
    my ($this, $cdef) = @_;
    push @{$this->{CDEFS}}, $cdef;
}

sub cdefs {
    return @{shift->{CDEFS}};
}

sub toCdefs {
    return join("\n", shift->cdefs) . "\n\n";
}

sub add_pod {
    my $this = shift;
    push @{$this->{POD}}, @_;
}

sub pod {
    return @{shift->{POD}};
}

sub equals {
    my ($new, $known) = @_;
    return 1 if $new eq $known;
    return 0 if $new->{NAME} ne $known->{NAME};
    return 1 if ! $new->fields; # Later declarations always match
    # NB: Definition after declaration is handled in parse_recordtype()
    my @nf = @{$new->{FIELD_LIST}};
    my @kf = @{$known->{FIELD_LIST}};
    return 0 if scalar @nf != scalar @kf;
    while (@nf) {
        my $nf = shift @nf;
        my $kf = shift @kf;
        return 0 if ! $nf->equals($kf);
    }
    return 1;
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
           "\n} $name;\n\n";
}

1;
