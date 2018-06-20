package DBD;

use strict;
use warnings;

use DBD::Base;
use DBD::Breaktable;
use DBD::Driver;
use DBD::Link;
use DBD::Menu;
use DBD::Recordtype;
use DBD::Recfield;
use DBD::Record;
use DBD::Registrar;
use DBD::Function;
use DBD::Variable;

use Carp;

sub new {
    my ($class) = @_;
    my $this = {
        'DBD::Breaktable' => {},
        'DBD::Driver'     => {},
	'DBD::Link'       => {},
        'DBD::Function'   => {},
        'DBD::Menu'       => {},
        'DBD::Recordtype' => {},
        'DBD::Record'     => {},
        'DBD::Registrar'  => {},
        'DBD::Variable'   => {},
        'COMMENTS'        => [],
        'POD'             => []
    };
    bless $this, $class;
    return $this;
}

sub add {
    my ($this, $obj, $obj_name) = @_;
    my $obj_class = ref $obj;
    confess "DBD::add: Unknown DBD object type '$obj_class'"
        unless $obj_class =~ m/^DBD::/
        and exists $this->{$obj_class};
    $obj_name = $obj->name unless defined $obj_name;
    if (exists $this->{$obj_class}->{$obj_name}) {
        return if $obj->equals($this->{$obj_class}->{$obj_name});
        dieContext("A different $obj->{WHAT} named '$obj_name' already exists");
    }
    else {
        $this->{$obj_class}->{$obj_name} = $obj;
    }
}

sub add_comment {
    my $this = shift;
    push @{$this->{COMMENTS}}, @_;
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

sub breaktables {
    return shift->{'DBD::Breaktable'};
}
sub breaktable {
    my ($this, $name) = @_;
    return $this->{'DBD::Breaktable'}->{$name};
}

sub drivers {
    return shift->{'DBD::Driver'};
}

sub links {
    return shift->{'DBD::Link'};
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

sub records {
    return shift->{'DBD::Record'};
}
sub record {
    my ($this, $record_name) = @_;
    return $this->{'DBD::Record'}->{$record_name};
}

sub registrars {
    return shift->{'DBD::Registrar'};
}

sub variables {
    return shift->{'DBD::Variable'};
}

1;
