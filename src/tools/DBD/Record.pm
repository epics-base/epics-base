package DBD::Record;

use strict;
use warnings;

use DBD::Base;

our @ISA = qw(DBD::Base);

use Carp;

our ($macrosOk);
my $warned;

sub init {
    my ($this, $type, $name) = @_;
    confess "DBD::Record::init: Not a DBD::Recordtype"
        unless $type->isa('DBD::Recordtype');
    $this->SUPER::init($name, "record");
    $this->{RECORD_TYPE} = $type;
    $this->{ALIASES} = [];
    $this->{RECFIELD_LIST} = [];
    $this->{FIELD_INDEX} = {};
    $this->{INFO_LIST} = [];
    $this->{INFO_ITEMS} = {};
    $this->{COMMENTS} = [];
    $this->{POD} = [];
    return $this;
}

# Override, record names are not as strict as recordtype and menu names
sub identifier {
    my ($this, $id, $what) = @_;
    confess "DBD::Record::identifier: $what undefined!"
        unless defined $id;
    if ($macrosOk) {
        # FIXME - Check name with macro
    }
    elsif ($id !~ m/^$RXname$/o) {
        my @message;
        push @message, "A $what should contain only letters, digits and these",
            "special characters: _ - : . [ ] < > ;" unless $warned++;
        warnContext("Deprecated $what '$id'", @message);
    }
    return $id;
}

sub recordtype {
    return shift->{RECORD_TYPE};
}

sub add_alias {
    my ($this, $alias) = @_;
    push @{$this->{ALIASES}}, $this->identifier($alias, "alias name");
}

sub aliases {
    return @{shift->{ALIASES}};
}

sub put_field {
    my ($this, $field_name, $value) = @_;
    my $recfield = $this->{RECORD_TYPE}->field($field_name);
    dieContext("No field named '$field_name'")
        unless defined $recfield;
    dieContext("Can't set $field_name to '$value'")
        unless $recfield->legal_value($value);
    push @{$this->{RECFIELD_LIST}}, $recfield
        unless exists $this->{FIELD_INDEX}->{$field_name};
    $this->{FIELD_INDEX}->{$field_name} = $value;
}

sub recfields {
    return @{shift->{RECFIELD_LIST}};
}

sub field_names { # In their original order...
    return map {$_->name} @{shift->{RECFIELD_LIST}};
}

sub get_field {
    my ($this, $field_name) = @_;
    return $this->{FIELD_INDEX}->{$field_name}
        if exists $this->{FIELD_INDEX}->{$field_name};
    my $recfield = $this->{RECORD_TYPE}->field($field_name);
    return $recfield->attribute("initial");
}

sub add_info {
    my ($this, $info_name, $value) = @_;
    push @{$this->{INFO_LIST}}, $info_name
        unless exists $this->{INFO_ITEMS}->{$info_name};
    $this->{INFO_ITEMS}->{$info_name} = $value;
}

sub info_names {
    return @{shift->{INFO_LIST}};
}

sub info_value {
    my ($this, $info_name) = @_;
    return $this->{INFO_ITEMS}->{$info_name};
}

sub add_comment {
    my ($this, $comment) = @_;
    push @{$this->{COMMENTS}}, $comment;
}

sub comments {
    return @{shift->{COMMENTS}};
}

sub add_pod {
    my $this = shift;
    push @{$this->{POD}}, @_;
}

sub pod {
    return @{shift->{POD}};
}

1;
